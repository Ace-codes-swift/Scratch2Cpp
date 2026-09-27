// Project.hpp - intermediate representation of a Scratch-like project.
//
// The frontend (scratch/) produces this model from project.json; the backend
// (codegen/) consumes it. Nothing in here depends on the JSON layout, so a
// future TurboWarp (or other) frontend only needs to build the same IR.
//
// Blocks keep Scratch opcodes ("motion_movesteps") as their node kind. Inputs
// are resolved into a tree: literals, nested reporter blocks, or sub-stacks
// (the C-shaped mouths of control blocks). Field values (drop-down menus) are
// stored as plain strings.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace s2c::ir {

// A constant appearing in a block input or as a variable's initial value.
struct Literal {
    enum class Kind { Number, String, Bool };
    Kind kind = Kind::String;
    double number = 0.0;
    std::string text;
    bool boolean = false;

    static Literal makeNumber(double n) { Literal l; l.kind = Kind::Number; l.number = n; return l; }
    static Literal makeString(std::string s) { Literal l; l.kind = Kind::String; l.text = std::move(s); return l; }
    static Literal makeBool(bool b) { Literal l; l.kind = Kind::Bool; l.boolean = b; return l; }
};

struct Block;
using BlockPtr = std::unique_ptr<Block>;

// A linear sequence of statement blocks (a script body or a sub-stack).
struct Sequence {
    std::vector<BlockPtr> blocks;
    bool empty() const { return blocks.empty(); }
};

struct Input {
    enum class Kind { Empty, Literal, Block, Substack };
    Kind kind = Kind::Empty;
    Literal literal;               // Kind::Literal
    BlockPtr block;                // Kind::Block (reporter / boolean / menu shadow)
    Sequence substack;             // Kind::Substack
};

struct Field {
    std::string value;             // display value ("space", "my variable", ...)
    std::optional<std::string> id; // referenced variable/list/broadcast id
};

// procedures_call / procedures_prototype metadata.
struct Mutation {
    std::string proccode;                      // "jump %s %b"
    std::vector<std::string> argumentIds;
    std::vector<std::string> argumentNames;
    std::vector<std::string> argumentDefaults;
    bool warp = false;                          // "run without screen refresh"
};

struct Block {
    std::string id;
    std::string opcode;
    std::map<std::string, Input> inputs;
    std::map<std::string, Field> fields;
    std::optional<Mutation> mutation;
    bool shadow = false;

    const Input* input(const std::string& name) const {
        auto it = inputs.find(name);
        return it == inputs.end() ? nullptr : &it->second;
    }
    const Field* field(const std::string& name) const {
        auto it = fields.find(name);
        return it == fields.end() ? nullptr : &it->second;
    }
    std::string fieldValue(const std::string& name, const std::string& fallback = "") const {
        const Field* f = field(name);
        return f ? f->value : fallback;
    }
};

// A hat block plus the statements hanging from it.
struct Script {
    BlockPtr hat;                  // event_whenflagclicked, procedures_definition, ...
    Sequence body;

    Script() = default;
    Script(Script&&) noexcept = default;
    Script& operator=(Script&&) noexcept = default;
    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
};

struct Variable {
    std::string id;
    std::string name;
    Literal initial;
    bool isCloud = false;
};

struct List {
    std::string id;
    std::string name;
    std::vector<Literal> initial;
};

struct Costume {
    std::string name;
    std::string assetId;           // md5
    std::string md5ext;            // "md5.svg" - file name in the assets map
    std::string dataFormat;        // "svg", "png", "jpg", ...
    double rotationCenterX = 0.0;
    double rotationCenterY = 0.0;
    double bitmapResolution = 1.0;
};

struct Sound {
    std::string name;
    std::string assetId;
    std::string md5ext;
    std::string dataFormat;        // "wav", "mp3"
    double rate = 0.0;
    double sampleCount = 0.0;
};

enum class RotationStyle { AllAround, LeftRight, DontRotate };

struct Target {
    std::string name;
    bool isStage = false;
    std::vector<Variable> variables;
    std::vector<List> lists;
    std::map<std::string, std::string> broadcasts;   // id -> name (stage only)
    std::vector<Script> scripts;
    std::vector<Costume> costumes;
    std::vector<Sound> sounds;
    int currentCostume = 0;
    double volume = 100.0;
    int layerOrder = 0;
    // Sprite-only state.
    double x = 0.0;
    double y = 0.0;
    double size = 100.0;
    double direction = 90.0;
    bool visible = true;
    bool draggable = false;
    RotationStyle rotationStyle = RotationStyle::AllAround;

    const Variable* variableById(const std::string& id) const {
        for (const auto& v : variables) if (v.id == id) return &v;
        return nullptr;
    }
    const List* listById(const std::string& id) const {
        for (const auto& l : lists) if (l.id == id) return &l;
        return nullptr;
    }

    Target() = default;
    Target(Target&&) noexcept = default;
    Target& operator=(Target&&) noexcept = default;
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;
};

// A stage watcher (variable, list, or slider) from project.json "monitors".
struct Monitor {
    std::string id;
    std::string opcode;            // data_variable, data_listcontents, ...
    std::string mode;              // default, large, slider, list
    std::string variableName;
    std::string listName;
    std::string spriteName;        // empty = stage (global)
    double x = 5;
    double y = 5;                  // top-left of the stage, pixels
    double width = 0;
    double height = 0;
    bool visible = false;
    double sliderMin = 0;
    double sliderMax = 100;
    bool isDiscrete = true;
};

struct Project {
    std::string title;
    std::string sourceDescription;             // e.g. the URL it came from
    int stageWidth = 480;
    int stageHeight = 360;
    std::vector<Target> targets;               // stage first, then sprites in layer order
    std::vector<Monitor> monitors;
    // Raw asset bytes keyed by md5ext file name.
    std::map<std::string, std::vector<std::uint8_t>> assets;
    // Scratch extensions referenced by the project ("pen", "music", ...).
    std::vector<std::string> extensions;

    const Target* stage() const {
        for (const auto& t : targets) if (t.isStage) return &t;
        return nullptr;
    }
    const Target* spriteByName(const std::string& name) const {
        for (const auto& t : targets) if (!t.isStage && t.name == name) return &t;
        return nullptr;
    }

    Project() = default;
    Project(Project&&) noexcept = default;
    Project& operator=(Project&&) noexcept = default;
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
};

}  // namespace s2c::ir
