# scratch2cpp

Converts a Scratch 3 project into a standalone, buildable C++ project that uses SDL3.

The converter **and** every generated project build natively on **macOS**, **Windows**, and **Linux** from the same CMake. There is no cross-compile step: run CMake on the machine you want the binary for. GitHub Actions builds both the converter and a transpiled sample on all three operating systems.

The converter downloads a shared Scratch project (or reads a local `.sb3`), parses it into an intermediate representation, and emits a CMake project you can build and run without Scratch. Unsupported blocks produce a warning and conversion continues.

```
scratch2cpp --url https://scratch.mit.edu/projects/10128431/ --output ./projects
```

## Requirements

To **build the converter**:

- CMake 3.21 or newer
- A C++20 compiler (Apple Clang, GCC, or MSVC)
- Optional: libcurl (otherwise downloads go through the `curl` command-line tool)

Vendored: [nlohmann/json](https://github.com/nlohmann/json) and [miniz](https://github.com/richgel999/miniz).

To **build a generated project**:

- CMake 3.21 or newer
- A C++20 compiler
- SDL3 (found with `find_package(SDL3)`). If it is not installed, the generated CMakeLists downloads SDL 3.2.20 automatically (`SCRATCH_FETCH_SDL3`, on by default).

| | Converter | Generated game |
|---|---|---|
| **macOS** | CMake + Apple Clang (`xcode-select --install`). Optional: `brew install cmake curl`. | Same, plus SDL3 (`brew install sdl3`) or let CMake fetch it. Optional GPU: Metal. |
| **Linux** | CMake + GCC or Clang. Optional: `libcurl4-openssl-dev`. | Same, plus `libsdl3-dev` (or fetch). Optional GPU: Vulkan if `vulkan/vulkan.h` is installed. |
| **Windows** | CMake + Visual Studio 2022 (MSVC) or Ninja. Optional: libcurl / `curl.exe`. | Same, plus SDL3 via vcpkg or fetch. Optional GPU: Direct3D 11 (always linked). |

## Building the converter

Same commands on every platform (`scripts/build.sh` / `scripts\build.bat` wrap them):

```sh
cmake -S . -B build
cmake --build build --config Release
```

| Platform | Converter binary |
|---|---|
| macOS / Linux | `./build/scratch2cpp` |
| Windows (Visual Studio) | `build\Release\scratch2cpp.exe` |
| Windows (Ninja) | `build\scratch2cpp.exe` |

Unit tests:

```sh
ctest --test-dir build -C Release --output-on-failure
```

## Using it

```
Usage:
  scratch2cpp --url <scratch project url> [--output <dir>] [options]
  scratch2cpp --sb3 <file.sb3> [--output <dir>] [options]

Options:
  --url <url>        Scratch project URL, or a bare numeric project id
  --sb3 <file>       Convert a local .sb3 instead of downloading
  --output <dir>     Parent directory for the generated project (default: ./projects).
                     Use a relative path such as ./out — /out is the root of the disk.
  --name <name>      Override the generated project directory name
  --force            Overwrite a previously generated project
  --save-sb3 <file>  Also write the downloaded project as an .sb3
  --scale <n>        Window scale of the generated game (1–8, default 2)
  --verbose          Per-asset / per-sprite progress
  --help
  --version
```

The project is written to `<output>/<sanitized project name>/`. Typical progress:

```
Downloading Scratch project 10128431...
Project: "Maze Starter"
Found 2 sprites
Found 4 costumes
Found 1 sounds
Found 8 scripts
Done!
Project generated at:
./projects/Maze Starter/
```

Invalid URLs, unshared projects, Scratch 2 files, and failed downloads produce a short error and a non-zero exit code. TurboWarp URLs are recognised and rejected with a message that they are planned, not supported yet.

## Building a generated project

Transpilation writes a normal CMake tree. Build that tree **on the OS you want to run**:

```sh
cd "projects/Maze Starter"
cmake -S . -B build
cmake --build build --config Release
```

| Platform | Game binary |
|---|---|
| macOS / Linux | `./build/Maze_Starter` |
| Windows (Visual Studio) | `build\Release\Maze_Starter.exe` |
| Windows (Ninja) | `build\Maze_Starter.exe` |

Or use the wrappers (they look in both single-config and multi-config output dirs):

| Script | Platform | What it does |
|---|---|---|
| `scripts/build.sh [Release\|Debug]` | macOS / Linux | Configure and build into `build/` |
| `scripts/run.sh` | macOS / Linux | Build and launch |
| `scripts/debug.sh` | macOS / Linux | Debug build into `build/debug`, then lldb or gdb |
| `scripts\build.bat` / `scripts\run.bat` / `scripts\debug.bat` | Windows | The same, for cmd.exe |

Generated apps accept:

- `--scale N` — window = 480×360 × N
- `--assets DIR` — costume/sound directory
- `--no-autostart` — do not fire the green flag on launch
- `--frames N` — quit after N frames (testing)
- `--screenshot file.bmp` — write the last frame
- `--key-at FRAME:KEY` / `--click-at FRAME:X,Y` — inject input at a given frame

## Debugging a generated project

`scripts/debug.sh` builds with `-DCMAKE_BUILD_TYPE=Debug` and runs `lldb -o run -- <exe>` (or `gdb -ex run --args <exe>`).

Each Scratch script is a C++20 coroutine named `script_N` on the generated sprite/stage class (`src/targets/Sprite_Player.cpp`). Custom blocks are `proc_<name>` member functions. Example:

```
(lldb) b Sprite_Player::script_1
(lldb) run
```

The runtime library lives in `runtime/scratch/` inside the generated tree (sprites, scheduler, SDL3 rendering/input/audio).

## Supported Scratch blocks

Hats: green flag, key pressed, this sprite / stage clicked, broadcast received, backdrop switches to, start as clone, when timer >.

| Category | Translated |
|---|---|
| Motion | move, turn, go to, glide, point, x/y/direction, if on edge bounce, rotation style |
| Looks | say/think (+ for secs), costumes, backdrops (+ and wait), size, show/hide, layers, ghost/brightness |
| Sound | play, play until done, stop all, volume (WAV assets) |
| Events | broadcast, broadcast and wait |
| Control | wait, repeat, forever, if / if-else, wait until, repeat until, while, for each, stop, clones |
| Sensing | touching, touching color, color is touching color, distance, key/mouse, timer, of, current, days since 2000, username |
| Operators | arithmetic, random, comparisons, and/or/not, join, letter of, length, contains, round, math op |
| Variables / lists | get/set/change, all list operations, show/hide, sliders and list watchers |
| My Blocks | custom procedures, arguments, “run without screen refresh” (warp) |
| Pen | clear, stamp, pen down/up, colour, colour parameters, size |

## Unsupported features

These produce a warning of the form `Warning: Unsupported Scratch block "music_playDrum" in sprite "Player"` (or an “unsupported feature” note). Conversion continues; the generated script calls `unsupported("opcode")` at runtime once.

- **Extensions:** music, video sensing, text to speech, translate, makey makey, boost, ev3, wedo, micro:bit, gdx for force
- **Looks:** graphic effects other than ghost and brightness (color, fisheye, whirl, pixelate, mosaic)
- **Sound:** pitch/pan effects; non-WAV assets (mp3/ogg are copied but not decoded)
- **Sensing:** ask and wait, loudness (reports −1), sprite mouse-drag
- **Events:** when loudness >, when touching object (hat)
- **Data:** cloud variables; slider interaction is mouse-only (no keyboard)
- **Control:** the hidden Scratch 2 counter blocks
- **Other:** comments on the stage, video, TurboWarp add-ons, Scratch 2 `.sb2` / `objName` projects

Speech bubbles, monitors, and SVG costume text use a system TrueType font when one is available (Hiragino / Arial on macOS, Arial / Segoe on Windows, DejaVu / Noto on Linux). SVG costumes are rasterised at 2× via nanosvg.

## Planned TurboWarp support

v1 accepts `scratch.mit.edu` URLs and local `.sb3` files only. TurboWarp URLs are rejected on purpose.

The pipeline is three layers so a TurboWarp frontend can be added without rewriting the generator:

1. **Frontend** (`src/scratch/`) — `ProjectSource` implementations fetch `project.json` + assets
2. **IR** (`src/ir/`) — opcode tree, targets, costumes, sounds; no JSON, no C++
3. **Backend** (`src/codegen/`) — C++/SDL3 project from the IR

TurboWarp projects use the same Scratch 3 `project.json`. A future `TurboWarpSource` would implement `ProjectSource`, optionally record TurboWarp options (frame rate, interpolation, stage size), and leave the parser and code generator in place. See [ARCHITECTURE.md](ARCHITECTURE.md).

## Repository layout

```
src/cli/          command-line options
src/scratch/      Scratch URL / .sb3 frontend
src/ir/           intermediate representation + diagnostics
src/codegen/      C++ / CMake / scripts / README emission
src/utils/        log, HTTP, zip, filesystem, strings
runtime/          SDL3 Scratch runtime (embedded into the converter)
scripts/          converter build wrappers (macOS/Linux and Windows)
tests/            unit tests + tiny.sb3 fixture used by CI
.github/          GitHub Actions: converter + generated project on macOS, Windows, Linux
third_party/      miniz, nlohmann/json
tools/            embed_files (packs runtime/ into the converter binary)
```

Third-party headers keep their original licenses under `third_party/` and `runtime/third_party/`.
