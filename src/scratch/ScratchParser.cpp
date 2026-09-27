#include "scratch/ScratchParser.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>

#include "utils/Error.hpp"
#include "utils/Log.hpp"
#include "utils/StringUtils.hpp"

namespace s2c {

using nlohmann::json;
using namespace ir;

namespace {

// Opcodes that start a script. Anything else at the top level is an orphan
// stack that Scratch never executes.
const std::set<std::string>& hatOpcodes() {
    static const std::set<std::string> hats = {
        "event_whenflagclicked",      "event_whenkeypressed",        "event_whenthisspriteclicked",
        "event_whenstageclicked",     "event_whenbroadcastreceived", "event_whenbackdropswitchesto",
        "event_whengreaterthan",      "control_start_as_clone",      "procedures_definition",
        "event_whentouchingobject",
    };
    return hats;
}

// sb3 primitive input type tags (scratch-vm/src/serialization/sb3.js).
enum PrimitiveType {
    MATH_NUM = 4, POSITIVE_NUM = 5, WHOLE_NUM = 6, INTEGER_NUM = 7, ANGLE_NUM = 8,
    COLOR_PICKER = 9, TEXT = 10, BROADCAST = 11, VAR = 12, LIST = 13,
};

Literal literalFromJson(const json& v) {
    if (v.is_number()) return Literal::makeNumber(v.get<double>());
    if (v.is_boolean()) return Literal::makeBool(v.get<bool>());
    if (v.is_string()) return Literal::makeString(v.get<std::string>());
    if (v.is_null()) return Literal::makeString("");
    return Literal::makeString(v.dump());
}

// Number-typed input slots hold whatever text the user typed. Scratch stores
// it as a string most of the time; we keep it numeric when it parses.
Literal numericSlotLiteral(const json& v) {
    if (v.is_number()) return Literal::makeNumber(v.get<double>());
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        const std::string t = str::trim(s);
        if (!t.empty()) {
            char* end = nullptr;
            const double d = std::strtod(t.c_str(), &end);
            if (end == t.c_str() + t.size() && std::isfinite(d)) return Literal::makeNumber(d);
        }
        return Literal::makeString(s);
    }
    return literalFromJson(v);
}

class TargetParser {
public:
    TargetParser(const json& target, const std::string& targetName, Diagnostics& diagnostics)
        : blocks_(target.value("blocks", json::object())), targetName_(targetName), diagnostics_(diagnostics) {}

    void parseScripts(Target& out) {
        for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
            const std::string& id = it.key();
            const json& block = it.value();
            if (!block.is_object()) continue;   // top-level variable reporter, not a script
            if (!block.value("topLevel", false)) continue;
            const std::string opcode = block.value("opcode", "");
            if (block.value("shadow", false)) continue;
            if (!hatOpcodes().count(opcode)) {
                if (opcode.find("when") != std::string::npos || str::endsWith(opcode, "_hat")) {
                    diagnostics_.unsupportedBlock(opcode, targetName_, "hat block; script skipped");
                } else {
                    log::detail("Ignoring orphan block stack starting with " + opcode + " in " + targetName_);
                }
                continue;
            }
            Script script;
            script.hat = buildBlock(id);
            if (block.contains("next") && block["next"].is_string()) {
                script.body = buildSequence(block["next"].get<std::string>());
            }
            out.scripts.push_back(std::move(script));
        }
    }

private:
    BlockPtr buildBlock(const std::string& id) {
        auto found = blocks_.find(id);
        if (found == blocks_.end()) {
            throw ConversionError("project.json references missing block \"" + id + "\" in target \"" + targetName_ + "\"");
        }
        const json& b = *found;
        auto out = std::make_unique<Block>();
        out->id = id;
        if (b.is_array()) {
            // Top-level variable/list reporters are serialised as primitives.
            fillPrimitiveBlock(*out, b);
            return out;
        }
        out->opcode = b.value("opcode", "");
        out->shadow = b.value("shadow", false);

        if (b.contains("fields") && b["fields"].is_object()) {
            for (auto it = b["fields"].begin(); it != b["fields"].end(); ++it) {
                Field f;
                const json& arr = it.value();
                if (arr.is_array() && !arr.empty()) {
                    f.value = arr[0].is_string() ? arr[0].get<std::string>() : arr[0].dump();
                    if (arr.size() > 1 && arr[1].is_string()) f.id = arr[1].get<std::string>();
                }
                out->fields[it.key()] = std::move(f);
            }
        }
        if (b.contains("inputs") && b["inputs"].is_object()) {
            for (auto it = b["inputs"].begin(); it != b["inputs"].end(); ++it) {
                out->inputs[it.key()] = buildInput(it.key(), it.value());
            }
        }
        if (b.contains("mutation") && b["mutation"].is_object()) {
            out->mutation = parseMutation(b["mutation"]);
        }
        return out;
    }

    Sequence buildSequence(const std::string& firstId) {
        Sequence seq;
        std::string current = firstId;
        std::set<std::string> seen;
        while (!current.empty()) {
            if (!seen.insert(current).second) {
                throw ConversionError("Cyclic block chain detected in target \"" + targetName_ + "\"");
            }
            auto found = blocks_.find(current);
            if (found == blocks_.end()) break;
            const json& b = *found;
            seq.blocks.push_back(buildBlock(current));
            if (b.is_object() && b.contains("next") && b["next"].is_string()) {
                current = b["next"].get<std::string>();
            } else {
                current.clear();
            }
        }
        return seq;
    }

    Input buildInput(const std::string& name, const json& raw) {
        Input in;
        if (!raw.is_array() || raw.size() < 2) return in;
        const json& primary = raw[1];
        if (primary.is_null()) {
            // Empty slot (e.g. no condition, empty C-mouth). If a shadow is
            // present in slot 2 (obscured shadow), fall back to it.
            if (raw.size() > 2 && !raw[2].is_null()) return buildInputValue(name, raw[2]);
            return in;
        }
        return buildInputValue(name, primary);
    }

    Input buildInputValue(const std::string& name, const json& value) {
        Input in;
        if (value.is_array()) {
            fillPrimitiveInput(in, value);
            return in;
        }
        if (value.is_string()) {
            const std::string id = value.get<std::string>();
            if (str::startsWith(name, "SUBSTACK")) {
                in.kind = Input::Kind::Substack;
                in.substack = buildSequence(id);
                return in;
            }
            auto found = blocks_.find(id);
            if (found == blocks_.end()) return in;
            in.kind = Input::Kind::Block;
            in.block = buildBlock(id);
            return in;
        }
        return in;
    }

    void fillPrimitiveInput(Input& in, const json& prim) {
        if (prim.empty() || !prim[0].is_number_integer()) return;
        const int type = prim[0].get<int>();
        const json value = prim.size() > 1 ? prim[1] : json(nullptr);
        switch (type) {
        case MATH_NUM: case POSITIVE_NUM: case WHOLE_NUM: case INTEGER_NUM: case ANGLE_NUM:
            in.kind = Input::Kind::Literal;
            in.literal = numericSlotLiteral(value);
            return;
        case COLOR_PICKER: case TEXT:
            in.kind = Input::Kind::Literal;
            in.literal = literalFromJson(value);
            return;
        case BROADCAST:
            in.kind = Input::Kind::Literal;
            in.literal = Literal::makeString(value.is_string() ? value.get<std::string>() : "");
            return;
        case VAR: case LIST: {
            in.kind = Input::Kind::Block;
            in.block = std::make_unique<Block>();
            fillPrimitiveBlock(*in.block, prim);
            return;
        }
        default:
            return;
        }
    }

    // [12, name, id] / [13, name, id] -> data_variable / data_listcontents block.
    void fillPrimitiveBlock(Block& block, const json& prim) {
        const int type = prim.size() > 0 && prim[0].is_number_integer() ? prim[0].get<int>() : 0;
        Field f;
        f.value = prim.size() > 1 && prim[1].is_string() ? prim[1].get<std::string>() : "";
        if (prim.size() > 2 && prim[2].is_string()) f.id = prim[2].get<std::string>();
        if (type == VAR) {
            block.opcode = "data_variable";
            block.fields["VARIABLE"] = std::move(f);
        } else if (type == LIST) {
            block.opcode = "data_listcontents";
            block.fields["LIST"] = std::move(f);
        } else {
            block.opcode = "unknown_primitive";
        }
    }

    static std::vector<std::string> parseJsonStringArray(const json& mutation, const char* key) {
        std::vector<std::string> out;
        if (!mutation.contains(key)) return out;
        const json& v = mutation[key];
        json arr;
        if (v.is_string()) {
            try {
                arr = json::parse(v.get<std::string>());
            } catch (...) {
                return out;
            }
        } else {
            arr = v;
        }
        if (!arr.is_array()) return out;
        for (const json& item : arr) out.push_back(item.is_string() ? item.get<std::string>() : item.dump());
        return out;
    }

    static Mutation parseMutation(const json& m) {
        Mutation mut;
        mut.proccode = m.value("proccode", "");
        mut.argumentIds = parseJsonStringArray(m, "argumentids");
        mut.argumentNames = parseJsonStringArray(m, "argumentnames");
        mut.argumentDefaults = parseJsonStringArray(m, "argumentdefaults");
        if (m.contains("warp")) {
            const json& w = m["warp"];
            mut.warp = w.is_boolean() ? w.get<bool>() : (w.is_string() && w.get<std::string>() == "true");
        }
        return mut;
    }

    const json blocks_;
    std::string targetName_;
    Diagnostics& diagnostics_;
};

RotationStyle parseRotationStyle(const std::string& s) {
    if (s == "left-right") return RotationStyle::LeftRight;
    if (s == "don't rotate") return RotationStyle::DontRotate;
    return RotationStyle::AllAround;
}

}  // namespace

ir::Project ScratchParser::parse(const RawProject& raw) {
    log::step("Parsing project.json...");
    json root;
    try {
        root = json::parse(raw.projectJson);
    } catch (const std::exception& e) {
        throw ConversionError(std::string("project.json is not valid JSON: ") + e.what());
    }
    if (root.contains("objName")) {
        throw ConversionError("Scratch 2 projects are not supported; please load the project in Scratch 3 and re-share it.");
    }
    if (!root.contains("targets") || !root["targets"].is_array()) {
        throw ConversionError("project.json has no \"targets\" array");
    }

    Project project;
    project.title = raw.title;
    if (project.title.empty() && root.contains("meta") && root["meta"].is_object() &&
        root["meta"].contains("title") && root["meta"]["title"].is_string()) {
        project.title = root["meta"]["title"].get<std::string>();
    }
    if (project.title.empty()) project.title = "Scratch Project";
    project.sourceDescription = raw.sourceDescription;
    project.assets = raw.assets;

    if (root.contains("extensions") && root["extensions"].is_array()) {
        for (const json& ext : root["extensions"]) {
            if (ext.is_string()) project.extensions.push_back(ext.get<std::string>());
        }
    }

    for (const json& t : root["targets"]) {
        Target target;
        target.isStage = t.value("isStage", false);
        target.name = t.value("name", target.isStage ? "Stage" : "Sprite");
        target.currentCostume = t.value("currentCostume", 0);
        target.volume = t.value("volume", 100.0);
        target.layerOrder = t.value("layerOrder", 0);
        if (!target.isStage) {
            target.x = t.value("x", 0.0);
            target.y = t.value("y", 0.0);
            target.size = t.value("size", 100.0);
            target.direction = t.value("direction", 90.0);
            target.visible = t.value("visible", true);
            target.draggable = t.value("draggable", false);
            target.rotationStyle = parseRotationStyle(t.value("rotationStyle", "all around"));
        }

        if (t.contains("variables") && t["variables"].is_object()) {
            for (auto it = t["variables"].begin(); it != t["variables"].end(); ++it) {
                const json& arr = it.value();
                if (!arr.is_array() || arr.empty()) continue;
                Variable v;
                v.id = it.key();
                v.name = arr[0].is_string() ? arr[0].get<std::string>() : arr[0].dump();
                v.initial = arr.size() > 1 ? literalFromJson(arr[1]) : Literal::makeNumber(0);
                v.isCloud = arr.size() > 2 && arr[2].is_boolean() && arr[2].get<bool>();
                if (v.isCloud) diagnostics_.unsupportedFeature("cloud variable \"" + v.name + "\" (treated as a normal variable)", target.name);
                target.variables.push_back(std::move(v));
            }
        }
        if (t.contains("lists") && t["lists"].is_object()) {
            for (auto it = t["lists"].begin(); it != t["lists"].end(); ++it) {
                const json& arr = it.value();
                if (!arr.is_array() || arr.empty()) continue;
                List l;
                l.id = it.key();
                l.name = arr[0].is_string() ? arr[0].get<std::string>() : arr[0].dump();
                if (arr.size() > 1 && arr[1].is_array()) {
                    for (const json& item : arr[1]) l.initial.push_back(literalFromJson(item));
                }
                target.lists.push_back(std::move(l));
            }
        }
        if (t.contains("broadcasts") && t["broadcasts"].is_object()) {
            for (auto it = t["broadcasts"].begin(); it != t["broadcasts"].end(); ++it) {
                if (it.value().is_string()) target.broadcasts[it.key()] = it.value().get<std::string>();
            }
        }
        if (t.contains("costumes") && t["costumes"].is_array()) {
            for (const json& c : t["costumes"]) {
                Costume costume;
                costume.name = c.value("name", "costume");
                costume.assetId = c.value("assetId", "");
                costume.dataFormat = c.value("dataFormat", "");
                costume.md5ext = c.value("md5ext", costume.assetId + "." + costume.dataFormat);
                costume.rotationCenterX = c.value("rotationCenterX", 0.0);
                costume.rotationCenterY = c.value("rotationCenterY", 0.0);
                costume.bitmapResolution = c.value("bitmapResolution", 1.0);
                if (!project.assets.count(costume.md5ext)) {
                    diagnostics_.warning("Costume \"" + costume.name + "\" asset " + costume.md5ext + " is missing", target.name);
                }
                target.costumes.push_back(std::move(costume));
            }
        }
        if (t.contains("sounds") && t["sounds"].is_array()) {
            for (const json& s : t["sounds"]) {
                Sound sound;
                sound.name = s.value("name", "sound");
                sound.assetId = s.value("assetId", "");
                sound.dataFormat = s.value("dataFormat", "");
                sound.md5ext = s.value("md5ext", sound.assetId + "." + sound.dataFormat);
                sound.rate = s.value("rate", 0.0);
                sound.sampleCount = s.value("sampleCount", 0.0);
                if (!project.assets.count(sound.md5ext)) {
                    diagnostics_.warning("Sound \"" + sound.name + "\" asset " + sound.md5ext + " is missing", target.name);
                }
                target.sounds.push_back(std::move(sound));
            }
        }

        TargetParser blocks(t, target.name, diagnostics_);
        blocks.parseScripts(target);
        project.targets.push_back(std::move(target));
    }

    // Stage first, then sprites by layer order (back to front).
    std::stable_sort(project.targets.begin(), project.targets.end(), [](const Target& a, const Target& b) {
        if (a.isStage != b.isStage) return a.isStage;
        return a.layerOrder < b.layerOrder;
    });
    if (!project.stage()) throw ConversionError("project.json has no stage target");

    if (root.contains("monitors") && root["monitors"].is_array()) {
        for (const json& m : root["monitors"]) {
            Monitor mon;
            mon.id = m.value("id", "");
            mon.opcode = m.value("opcode", "");
            mon.mode = m.value("mode", "default");
            if (m.contains("spriteName") && m["spriteName"].is_string()) {
                mon.spriteName = m["spriteName"].get<std::string>();
            }
            auto num = [&](const char* key, double fallback) {
                if (!m.contains(key) || !m[key].is_number()) return fallback;
                return m[key].get<double>();
            };
            auto flag = [&](const char* key, bool fallback) {
                if (!m.contains(key) || !m[key].is_boolean()) return fallback;
                return m[key].get<bool>();
            };
            mon.x = num("x", 5.0);
            mon.y = num("y", 5.0);
            mon.width = num("width", 0.0);
            mon.height = num("height", 0.0);
            mon.visible = flag("visible", false);
            mon.sliderMin = num("sliderMin", 0.0);
            mon.sliderMax = num("sliderMax", 100.0);
            mon.isDiscrete = flag("isDiscrete", true);
            if (m.contains("params") && m["params"].is_object()) {
                if (m["params"].contains("VARIABLE") && m["params"]["VARIABLE"].is_string()) {
                    mon.variableName = m["params"]["VARIABLE"].get<std::string>();
                }
                if (m["params"].contains("LIST") && m["params"]["LIST"].is_string()) {
                    mon.listName = m["params"]["LIST"].get<std::string>();
                }
            }
            project.monitors.push_back(std::move(mon));
        }
    }
    return project;
}

}  // namespace s2c
