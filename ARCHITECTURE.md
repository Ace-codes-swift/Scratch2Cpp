# Architecture

scratch2cpp is a three-layer compiler, not a Scratch interpreter that happens to emit C++. The frontend, the intermediate representation, and the backend are separate so a new project source (TurboWarp, a local editor, another block language) can be added without rewriting the generator.

```
scratch.mit.edu URL / .sb3 file
        │
        ▼
  ProjectSource::load()          frontend   src/scratch/
        │
        ▼
     RawProject                  project.json + asset bytes
        │
        ▼
   ScratchParser::parse()
        │
        ▼
     ir::Project                 IR         src/ir/
        │
        ▼
  codegen::generateProject()     backend    src/codegen/
        │
        ▼
  standalone C++ / CMake / SDL3 project
  (generated sources + copied runtime/ + assets/)
  native build on macOS, Windows, and Linux
```

## Why three layers

Scratch’s `project.json` is a graph of block ids, shadow inputs, and mutation strings. TurboWarp uses the same JSON, plus extra options. If the C++ emitter walked that JSON, every new source would have to re-implement the same quirks.

Instead:

- The **frontend** is the only place that knows about URLs, HTTP tokens, zip entries, and Scratch 2 vs 3.
- The **IR** is a tree: each block has resolved inputs (literal, reporter, or substack) and string fields.
- The **backend** walks the IR and emits C++ that calls a small runtime library.

`createProjectSource()` already special-cases TurboWarp URLs and rejects them. A `TurboWarpSource` that implements `ProjectSource` is the intended next frontend.

## Frontend (`src/scratch/`)

`ProjectSource` is the extension point:

```cpp
struct RawProject {
    std::string title;
    std::string sourceDescription;
    std::string projectJson;
    std::map<std::string, std::vector<std::uint8_t>> assets;
};

class ProjectSource {
public:
    virtual RawProject load() = 0;
};
```

| Implementation | Input | Notes |
|---|---|---|
| `ScratchWebSource` | `https://scratch.mit.edu/projects/<id>/` or a bare id | Metadata from `api.scratch.mit.edu` (title + `project_token`), JSON from `projects.scratch.mit.edu`, costumes/sounds from `assets.scratch.mit.edu` |
| `Sb3FileSource` | path ending in `.sb3` | Zip via miniz; title from `meta.title` when `--save-sb3` wrote it, else the file stem |
| (planned) `TurboWarpSource` | `https://turbowarp.org/<id>/` | Same JSON as Scratch; optional TurboWarp extras (fps, stage size) would be recorded on `ir::Project` |

Scratch 2 projects (`objName` in the JSON) are rejected with a clear error.

`ScratchParser` turns `RawProject` into `ir::Project`:

- Walks each target’s `blocks` map, follows `next` / `parent`, and builds a tree.
- Decodes Scratch input encoding (`[shadowKind, primary, obscuredShadow]`), primitives (types 4–13), and procedure mutations (`proccode`, argument ids/names/defaults, warp).
- Treats top-level hats as scripts. Unknown hats that look like `when*` are reported and skipped; other orphan blocks are ignored.
- Cloud variables and visible monitors are recorded as unsupported features, not fatal errors.

## Intermediate representation (`src/ir/`)

`ir::Project` holds the stage first, then sprites in layer order, plus the asset map (keyed by Scratch `md5ext`).

A `Block` keeps the Scratch opcode (`motion_movesteps`) as its kind. Inputs are already resolved:

| `Input::Kind` | Meaning |
|---|---|
| `Empty` | Missing / unused |
| `Literal` | Number, string, or bool |
| `Block` | Nested reporter, boolean, or menu shadow |
| `Substack` | The C-shaped body of a control block |

Fields (dropdowns) are `{value, optional id}` — the id is the variable/list/broadcast id from Scratch.

Nothing in `ir/` includes nlohmann/json or knows about C++ identifiers. Diagnostics (`Diagnostics`) collect warnings of the form:

```
Warning: Unsupported Scratch block "music_playDrum" in sprite "Player"
```

## Backend (`src/codegen/`)

### Block registry

Each Scratch palette has one file under `src/codegen/blocks/`. At startup the files register statement and expression handlers on a singleton `BlockRegistry`:

```cpp
r.statement("motion_movesteps", [](const ir::Block& b, TargetCompiler& c, Emitter& out) {
    out.line("moveSteps(" + c.number(b, "STEPS") + ");");
});
r.expression("operator_add", ...);
```

Unknown opcodes go through `TargetCompiler::unsupportedStatement` / `unsupportedExpression`: a comment, a one-time `unsupported("opcode")` call, and a diagnostic. Conversion never stops because of one block.

To add a block: implement the runtime method if needed, then register a handler. No other files change.

### Target compiler

`TargetCompiler` emits one C++ class per Scratch target (`ProjectStage`, `Sprite_<name>`):

- Costumes, sounds, variables, lists, and initial motion/looks state in the constructor.
- Each hat becomes `scratch::Task Class::script_N(scratch::Thread&)`.
- Custom blocks become `proc_<sanitized proccode>` coroutines; warp definitions wrap the body in `scratch::WarpGuard`.
- `when timer >` hats also emit `hatPredicate_N()` and are registered with `addEdgeScript`.

Expressions are typed (`Expr` with `Number` / `String` / `Bool` / `Value`) so generated code can stay in `double`/`bool` and only box into `scratch::Value` when Scratch’s dynamic typing is required.

### Project layout

`ProjectGenerator` writes:

```
<output>/<Project>/
  CMakeLists.txt     find or fetch SDL3; build the exe against runtime/
  src/main.cpp       CLI flags, Runtime, registerTargets
  src/Project.cpp    createStage / createSprite in layer order
  src/targets/       one .cpp per sprite/stage
  include/           matching headers
  runtime/           copy of the embedded runtime sources
  assets/            costume and sound files (Scratch md5 names, not embedded in .cpp)
  scripts/           build / run / debug wrappers
  README.md          per-project notes, including conversion warnings
  build/             empty; cmake output goes here
```

The runtime is compiled into the converter as byte arrays (`tools/embed_files.cpp` → `EmbeddedRuntime.cpp`) so a single `scratch2cpp` binary can emit a self-contained project.

## Runtime (`runtime/scratch/`)

The generated scripts are not a reimplementation of Scratch in each project. They call a small static library that mirrors [scratch-vm](https://github.com/scratchfoundation/scratch-vm) on top of SDL3.

### Values and lists

`scratch::Value` is a tagged number/string/bool with Scratch/JS casting:

- `toNumber` / `toString` / `toBool` / `compare` follow scratch-vm (`Cast`).
- Number formatting aims at ECMAScript `Number#toString` (so `join` and monitors match Scratch).
- Lists resolve `"last"`, `"random"` / `"any"`, and 1-based indices the same way.

`scratch::ops` implements the Operators palette (including JS-style `Infinity`/`NaN` and `mod` with the sign of the divisor).

### Threads as coroutines

Every script is a C++20 coroutine returning `scratch::Task`. The sequencer steps all active threads each frame (default 30 FPS). If nobody requested a redraw and there is time left in the frame, it steps again — the same “run until redraw or 75% of the frame” rule as scratch-vm.

Loops `co_await scratch::Yield{}` at the end of each iteration. `wait`, `glide`, `playSoundUntilDone`, and `broadcastAndWait` are also awaits. Nested custom blocks use symmetric transfer so a `co_await proc(...)` stays on the same Scratch thread.

`Thread::leaf` is the innermost coroutine to resume. Stopped threads and deleted clones are destroyed after the step loop, never while a frame is running.

Warp mode (`WarpGuard`) skips yields for up to 500 ms, matching scratch-vm’s warp timer.

### Sprites, clones, and the stage

`Target` holds variables, lists, costumes, sounds, and hat bindings. `Sprite` adds motion, looks, fencing, and pixel-accurate touching. `Stage` is a target whose costume change fires backdrop-switched hats.

Clones:

- `createCloneOf("_myself_")` or a sprite name
- inserted immediately behind the original
- cap of 300 (Scratch’s limit)
- `when I start as a clone` hats start on the new instance only

### Rendering and coordinates

The stage is Scratch’s 480×360, origin at the centre, x ∈ [−240, 240], y ∈ [−180, 180]. SDL3 uses `SDL_SetRenderLogicalPresentation` so the window letterboxes that rectangle.

Costumes:

- PNG/JPEG via stb_image
- SVG via nanosvg, rasterised at 2× (capped); `<text>` and embedded PNG `<image>` are drawn in a second pass (nanosvg skips both)
- Rotation centre and `bitmapResolution` applied the same way as scratch-vm

Sprites are drawn with `SDL_RenderTextureRotated`. Ghost → alpha; negative brightness → colour mod. Hit testing and “touching color?” use the CPU-side RGBA buffer and Scratch’s colour mask `(0xF8, 0xF8, 0xF0)`.

The **pen** is a stage-resolution RGBA layer drawn after the backdrop and before sprites. Pen-down movement strokes a thick line; stamp copies opaque costume pixels (ghost applied). Colour parameters match scratch-vm (hue / saturation / brightness / transparency 0–100).

Speech bubbles are wrapped and drawn with `SDL_RenderDebugText`.

### Input and audio

`Input` maps SDL keys to Scratch names (`space`, `right arrow`, …) and exposes mouse position in stage coordinates. Key-pressed hats fire for the specific key and for `"any"`. A click on a sprite starts `SpriteClicked`; a click on empty stage starts `StageClicked`.

Audio is SDL3 (`SDL_LoadWAV` + `SDL_AudioStream`). Volume is per-sprite. Pitch/pan are not implemented.

### Adding a new source

1. Implement `ProjectSource` (download or read files into a `RawProject`).
2. Teach `createProjectSource()` to pick it (URL host, file extension, …).
3. If the source has extra options (fps, stage size), add fields to `ir::Project` and thread them through `RuntimeConfig`.
4. Leave `ScratchParser` and `codegen/` alone unless the new source uses a different block encoding.

### Adding a new block

1. Add a method on `Target` / `Sprite` / `Runtime` if the behaviour is not already there. Comment Scratch-specific rules (fencing, costume number wrap, …) next to the code.
2. Register a handler in the matching `src/codegen/blocks/*Blocks.cpp`.
3. Add a unit test in `tests/test_converter.cpp` if the encoding is non-obvious; use `--frames` / `--screenshot` on a real project for visual behaviour.
