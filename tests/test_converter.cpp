// Tests for the Scratch frontend (parser) and the C++ backend on a small
// in-memory project.json covering hats, loops, variables, procedures and an
// unsupported block.
#include <iostream>
#include <string>

#include "codegen/CMakeGenerator.hpp"
#include "codegen/CppGenerator.hpp"
#include "codegen/ScriptsGenerator.hpp"
#include "ir/Diagnostics.hpp"
#include "scratch/ProjectSource.hpp"
#include "scratch/ScratchDownloader.hpp"
#include "scratch/ScratchParser.hpp"
#include "utils/FileSystem.hpp"
#include "utils/StringUtils.hpp"

namespace {
int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

void checkContains(const std::string& haystack, const std::string& needle, const std::string& what) {
    if (haystack.find(needle) == std::string::npos) {
        std::cerr << "FAIL: " << what << " (missing \"" << needle << "\")\n";
        ++failures;
    }
}

const char* kProjectJson = R"JSON({
  "targets": [
    {
      "isStage": true, "name": "Stage", "layerOrder": 0,
      "variables": {"gv": ["score", 0]},
      "lists": {"gl": ["items", ["a", "b"]]},
      "broadcasts": {"bc": "go"},
      "blocks": {
        "s1": {"opcode": "event_whenflagclicked", "next": "s2", "parent": null, "inputs": {}, "fields": {}, "shadow": false, "topLevel": true},
        "s2": {"opcode": "event_broadcast", "next": null, "parent": "s1", "inputs": {"BROADCAST_INPUT": [1, [11, "go", "bc"]]}, "fields": {}, "shadow": false, "topLevel": false}
      },
      "currentCostume": 0,
      "costumes": [{"name": "backdrop1", "assetId": "aaaa", "md5ext": "aaaa.svg", "dataFormat": "svg", "rotationCenterX": 240, "rotationCenterY": 180}],
      "sounds": [], "volume": 100
    },
    {
      "isStage": false, "name": "Player 1", "layerOrder": 1,
      "variables": {"lv": ["speed", "5"]},
      "lists": {},
      "blocks": {
        "h1": {"opcode": "event_whenbroadcastreceived", "next": "b1", "parent": null, "inputs": {}, "fields": {"BROADCAST_OPTION": ["Go", "bc"]}, "shadow": false, "topLevel": true},
        "b1": {"opcode": "control_forever", "next": null, "parent": "h1", "inputs": {"SUBSTACK": [2, "b2"]}, "fields": {}, "shadow": false, "topLevel": false},
        "b2": {"opcode": "control_if", "next": "b4", "parent": "b1", "inputs": {"CONDITION": [2, "b3"], "SUBSTACK": [2, "b5"]}, "fields": {}, "shadow": false, "topLevel": false},
        "b3": {"opcode": "sensing_keypressed", "next": null, "parent": "b2", "inputs": {"KEY_OPTION": [1, "m1"]}, "fields": {}, "shadow": false, "topLevel": false},
        "m1": {"opcode": "sensing_keyoptions", "next": null, "parent": "b3", "inputs": {}, "fields": {"KEY_OPTION": ["right arrow", null]}, "shadow": true, "topLevel": false},
        "b5": {"opcode": "motion_changexby", "next": null, "parent": "b2", "inputs": {"DX": [3, [12, "speed", "lv"], [4, "10"]]}, "fields": {}, "shadow": false, "topLevel": false},
        "b4": {"opcode": "data_changevariableby", "next": "b6", "parent": "b2", "inputs": {"VALUE": [1, [4, "1"]]}, "fields": {"VARIABLE": ["score", "gv"]}, "shadow": false, "topLevel": false},
        "b6": {"opcode": "procedures_call", "next": "b7", "parent": "b4", "inputs": {"argid": [1, [10, "hello"]]}, "fields": {}, "shadow": false, "topLevel": false,
               "mutation": {"tagName": "mutation", "children": [], "proccode": "shout %s", "argumentids": "[\"argid\"]", "warp": "false"}},
        "b7": {"opcode": "pen_down", "next": "b8", "parent": "b6", "inputs": {}, "fields": {}, "shadow": false, "topLevel": false},
        "b8": {"opcode": "data_showvariable", "next": "b9", "parent": "b7", "inputs": {}, "fields": {"VARIABLE": ["score", "gv"]}, "shadow": false, "topLevel": false},
        "b9": {"opcode": "data_hidevariable", "next": null, "parent": "b8", "inputs": {}, "fields": {"VARIABLE": ["score", "gv"]}, "shadow": false, "topLevel": false},
        "d1": {"opcode": "procedures_definition", "next": "d3", "parent": null, "inputs": {"custom_block": [1, "d2"]}, "fields": {}, "shadow": false, "topLevel": true},
        "d2": {"opcode": "procedures_prototype", "next": null, "parent": "d1", "inputs": {}, "fields": {}, "shadow": true, "topLevel": false,
               "mutation": {"tagName": "mutation", "children": [], "proccode": "shout %s", "argumentids": "[\"argid\"]", "argumentnames": "[\"what\"]", "argumentdefaults": "[\"\"]", "warp": "true"}},
        "d3": {"opcode": "looks_say", "next": null, "parent": "d1", "inputs": {"MESSAGE": [3, "d4", [10, "Hello!"]]}, "fields": {}, "shadow": false, "topLevel": false},
        "d4": {"opcode": "argument_reporter_string_number", "next": null, "parent": "d3", "inputs": {}, "fields": {"VALUE": ["what", null]}, "shadow": false, "topLevel": false}
      },
      "currentCostume": 0,
      "costumes": [{"name": "costume1", "assetId": "bbbb", "md5ext": "bbbb.png", "dataFormat": "png", "bitmapResolution": 2, "rotationCenterX": 48, "rotationCenterY": 50}],
      "sounds": [{"name": "pop", "assetId": "cccc", "md5ext": "cccc.wav", "dataFormat": "wav"}],
      "volume": 100, "visible": true, "x": 10, "y": -20, "size": 100, "direction": 90, "draggable": false, "rotationStyle": "left-right"
    }
  ],
  "monitors": [
    {"id": "gv", "mode": "slider", "opcode": "data_variable", "params": {"VARIABLE": "score"},
     "spriteName": null, "value": 0, "width": 0, "height": 0, "x": 10, "y": 20,
     "visible": false, "sliderMin": 0, "sliderMax": 50, "isDiscrete": true}
  ], "extensions": ["pen"], "meta": {"semver": "3.0.0"}
})JSON";

}  // namespace

int main() {
    // URL parsing.
    check(s2c::ScratchWebSource::extractProjectId("https://scratch.mit.edu/projects/123456789/").value_or("") == "123456789", "url id");
    check(s2c::ScratchWebSource::extractProjectId("scratch.mit.edu/projects/42/editor/").value_or("") == "42", "editor url id");
    check(s2c::ScratchWebSource::extractProjectId("https://scratch.mit.edu/projects/7?x=1").value_or("") == "7", "query url id");
    check(!s2c::ScratchWebSource::extractProjectId("https://scratch.mit.edu/users/foo").has_value(), "non-project url");
    check(s2c::str::sanitizeFileName("My: Game?/v2 ") == "My_ Game__v2", "sanitize file name");
    check(s2c::str::sanitizeFileName("...") == "ScratchProject", "sanitize fallback");
    check(s2c::str::sanitizeIdentifier("Player 1") == "Player_1", "sanitize identifier");
    check(s2c::str::sanitizeIdentifier("2fast") == "_2fast", "identifier digit prefix");
    check(s2c::str::sanitizeIdentifier("int") == "int_", "identifier keyword");
    check(s2c::fs::absoluteOutputHint("/out").find("./out") != std::string::npos, "/out suggests ./out");
    check(s2c::fs::absoluteOutputHint("./out").empty(), "relative output has no hint");
    check(s2c::fs::absoluteOutputHint("/tmp/projects").empty(), "deeper absolute path has no hint");

    // Parse.
    s2c::RawProject raw;
    raw.title = "Test Project";
    raw.sourceDescription = "unit test";
    raw.projectJson = kProjectJson;
    raw.assets["aaaa.svg"] = {'<', 's', 'v', 'g', '/', '>'};
    raw.assets["bbbb.png"] = {0};
    raw.assets["cccc.wav"] = {0};

    s2c::Diagnostics diagnostics;
    s2c::ScratchParser parser(diagnostics);
    s2c::ir::Project project = parser.parse(raw);

    check(project.targets.size() == 2, "two targets");
    check(project.targets[0].isStage, "stage first");
    const s2c::ir::Target& sprite = project.targets[1];
    check(sprite.name == "Player 1", "sprite name");
    check(sprite.scripts.size() == 2, "two scripts (hat + definition)");
    check(sprite.rotationStyle == s2c::ir::RotationStyle::LeftRight, "rotation style");
    check(sprite.variables.size() == 1 && sprite.variables[0].name == "speed", "local variable");
    check(project.targets[0].lists.size() == 1 && project.targets[0].lists[0].initial.size() == 2, "global list");

    // Find the broadcast script and verify the structure of the parsed tree.
    const s2c::ir::Script* bcScript = nullptr;
    for (const auto& s : sprite.scripts) {
        if (s.hat && s.hat->opcode == "event_whenbroadcastreceived") bcScript = &s;
    }
    check(bcScript != nullptr, "broadcast script found");
    if (bcScript) {
        check(bcScript->body.blocks.size() == 1 && bcScript->body.blocks[0]->opcode == "control_forever", "forever body");
        const s2c::ir::Input* sub = bcScript->body.blocks[0]->input("SUBSTACK");
        check(sub && sub->kind == s2c::ir::Input::Kind::Substack && sub->substack.blocks.size() == 6, "substack has 6 blocks");
        if (sub && sub->substack.blocks.size() >= 4) {
            const s2c::ir::Block& ifBlock = *sub->substack.blocks[0];
            const s2c::ir::Input* cond = ifBlock.input("CONDITION");
            check(cond && cond->kind == s2c::ir::Input::Kind::Block && cond->block->opcode == "sensing_keypressed", "condition block");
            const s2c::ir::Input* dx = sub->substack.blocks[0]->input("SUBSTACK")->substack.blocks[0]->input("DX");
            check(dx && dx->kind == s2c::ir::Input::Kind::Block && dx->block->opcode == "data_variable", "obscured shadow resolves to variable");
        }
    }

    // Generate C++.
    s2c::codegen::CppGenerator gen(project, diagnostics, 2);
    s2c::codegen::CppSources sources = gen.generate();
    check(sources.executableName == "Test_Project", "executable name");
    const std::string& spriteSrc = sources.files.at("src/targets/Sprite_Player_1.cpp");
    const std::string& spriteHdr = sources.files.at("include/targets/Sprite_Player_1.hpp");
    const std::string& stageSrc = sources.files.at("src/targets/ProjectStage.cpp");

    checkContains(spriteHdr, "class Sprite_Player_1 final : public scratch::Sprite", "sprite class");
    checkContains(spriteHdr, "v_speed = 0", "variable constant");
    checkContains(spriteHdr, "scratch::Task proc_shout(scratch::Thread& th, scratch::Value arg_what);", "procedure declaration");
    checkContains(spriteSrc, "addScript(scratch::Thread::Hat::BroadcastReceived, \"go\"", "broadcast hat lower-cased");
    checkContains(spriteSrc, "for (;;) {", "forever loop");
    checkContains(spriteSrc, "co_await scratch::Yield{};", "loop yields");
    checkContains(spriteSrc, "if (keyPressed(scratch::Value(std::string(\"right arrow\"))))", "key pressed condition from menu");
    checkContains(spriteSrc, "changeX((variables_[v_speed]).toNumber());", "local variable reference");
    checkContains(spriteSrc, "stage().variables_[ProjectStage::v_score] = scratch::Value(stage().variables_[ProjectStage::v_score].toNumber() + 1.0);",
                  "global variable change");
    checkContains(spriteSrc, "co_await proc_shout(th, scratch::Value(std::string(\"hello\")));", "procedure call");
    checkContains(spriteSrc, "scratch::WarpGuard warpGuard(th);", "warp procedure");
    checkContains(spriteSrc, "say(arg_what);", "argument reporter");
    checkContains(spriteSrc, "penDown();", "pen down translates");
    checkContains(spriteSrc, "showVariable(\"score\");", "show variable monitor");
    checkContains(spriteSrc, "hideVariable(\"score\");", "hide variable monitor");
    checkContains(sources.files.at("src/Project.cpp"), "m.mode = scratch::Monitor::Mode::Slider;", "slider monitor registered");
    checkContains(sources.files.at("src/Project.cpp"), "m.name = \"score\";", "slider variable name");
    checkContains(spriteSrc, "setRotationStyle(scratch::RotationStyle::LeftRight);", "initial rotation style");
    checkContains(stageSrc, "broadcast(scratch::Value(std::string(\"go\")));", "broadcast literal");
    checkContains(sources.files.at("src/Project.cpp"), "runtime.createSprite<Sprite_Player_1>();", "sprite registration");
    checkContains(sources.files.at("src/main.cpp"), "config.gpuAccel = true;", "optional --gpu flag");

    const std::string cmake = s2c::codegen::generateCMakeLists(project, sources.executableName, sources.sourceFiles);
    checkContains(cmake, "Native CMake project: macOS, Windows, and Linux.", "generated cmake names three platforms");
    checkContains(cmake, "enable_language(OBJCXX)", "generated cmake enables OBJCXX on Apple");
    checkContains(cmake, "if(WIN32)", "generated cmake has Windows branch");
    checkContains(cmake, "SDL3::SDL3-shared", "generated cmake copies SDL on Windows");

    const auto scripts = s2c::codegen::generateScripts(sources.executableName);
    check(scripts.count("scripts/run.sh") == 1, "run.sh");
    check(scripts.count("scripts/run.bat") == 1, "run.bat");
    check(scripts.count("scripts/build.bat") == 1, "build.bat");
    checkContains(scripts.at("scripts/run.bat"), sources.executableName + ".exe", "run.bat launches the exe");
    checkContains(scripts.at("scripts/build.sh"), "find_exe", "build.sh locates multi-config binaries");

    bool sawPenWarning = false;
    for (const auto& d : diagnostics.all()) {
        if (d.kind == s2c::Diagnostic::Kind::UnsupportedBlock && d.opcode == "pen_down") sawPenWarning = true;
    }
    check(!sawPenWarning, "pen_down is supported");

    if (failures == 0) std::cout << "test_converter: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
