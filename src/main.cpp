// scratch2cpp - converts a Scratch 3 project into a standalone C++/SDL3 project.
//
// Pipeline:
//   ProjectSource (Scratch URL / .sb3)  ->  RawProject (project.json + assets)
//   ScratchParser                        ->  ir::Project
//   codegen::generateProject             ->  C++ sources + runtime + CMake + assets
#include <exception>
#include <filesystem>
#include <iostream>

#include "cli/CliOptions.hpp"
#include "codegen/ProjectGenerator.hpp"
#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"
#include "scratch/ProjectSource.hpp"
#include "scratch/Sb3Archive.hpp"
#include "scratch/ScratchParser.hpp"
#include "utils/Error.hpp"
#include "utils/FileSystem.hpp"
#include "utils/Log.hpp"

namespace {

void printSummary(const s2c::ir::Project& project) {
    size_t sprites = 0, costumes = 0, sounds = 0, scripts = 0;
    for (const auto& t : project.targets) {
        if (!t.isStage) ++sprites;
        costumes += t.costumes.size();
        sounds += t.sounds.size();
        scripts += t.scripts.size();
    }
    s2c::log::info("Project: \"" + project.title + "\"");
    s2c::log::info("Found " + std::to_string(sprites) + " sprites");
    s2c::log::info("Found " + std::to_string(costumes) + " costumes");
    s2c::log::info("Found " + std::to_string(sounds) + " sounds");
    s2c::log::info("Found " + std::to_string(scripts) + " scripts");
    if (!project.extensions.empty()) {
        std::string ext;
        for (const auto& e : project.extensions) ext += (ext.empty() ? "" : ", ") + e;
        s2c::log::info("Extensions used: " + ext);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string program = std::filesystem::path(argv[0]).filename().string();
    s2c::cli::ParseResult parsed = s2c::cli::parse(argc, argv);
    if (parsed.error) {
        s2c::log::error(*parsed.error);
        std::cerr << "\n" << s2c::cli::usage(program);
        return 2;
    }
    const s2c::cli::Options& opts = parsed.options;
    if (opts.showHelp) {
        std::cout << s2c::cli::usage(program);
        return 0;
    }
    if (opts.showVersion) {
        std::cout << s2c::cli::versionString() << "\n";
        return 0;
    }
    s2c::log::setVerbose(opts.verbose);

    try {
        // Fail before downloading if --output (or --save-sb3) cannot be written.
        s2c::fs::requireWritableDirectory(opts.output, "output directory");
        if (!opts.saveSb3.empty()) {
            std::filesystem::path sb3Parent = std::filesystem::path(opts.saveSb3).parent_path();
            if (sb3Parent.empty()) sb3Parent = ".";
            s2c::fs::requireWritableDirectory(sb3Parent, "directory for --save-sb3");
        }

        // 1. Frontend: obtain project.json + assets from wherever they live.
        std::unique_ptr<s2c::ProjectSource> source = s2c::createProjectSource(opts.input);
        s2c::RawProject raw = source->load();

        if (!opts.saveSb3.empty()) {
            s2c::fs::writeBinaryFile(opts.saveSb3, s2c::writeSb3(raw));
            s2c::log::info("Saved " + opts.saveSb3);
        }

        // 2. Parse into the IR.
        s2c::Diagnostics diagnostics;
        s2c::ScratchParser parser(diagnostics);
        s2c::ir::Project project = parser.parse(raw);
        printSummary(project);

        // 3. Backend: emit the C++ project.
        s2c::codegen::GenerateOptions gen;
        gen.outputRoot = opts.output;
        gen.projectDirName = opts.projectName;
        gen.overwrite = opts.force;
        gen.windowScale = opts.windowScale;
        s2c::codegen::GenerateResult result = s2c::codegen::generateProject(project, diagnostics, gen);

        s2c::log::step("Done!");
        if (!diagnostics.empty()) {
            s2c::log::info("");
            s2c::log::info(std::to_string(diagnostics.count()) + " warning(s); see README.md in the generated project for details.");
        }
        s2c::log::info("");
        s2c::log::info("Project generated at:");
        s2c::log::info(result.projectDir.string() + "/");
        s2c::log::info("");
        s2c::log::info("Build it with:");
        s2c::log::info("  cd \"" + result.projectDir.string() + "\"");
        s2c::log::info("  cmake -S . -B build && cmake --build build");
        s2c::log::info("  ./build/" + result.executableName);
        return 0;
    } catch (const s2c::ConversionError& e) {
        s2c::log::error(e.what());
        return 1;
    } catch (const std::exception& e) {
        s2c::log::error(std::string("Unexpected failure: ") + e.what());
        return 1;
    }
}
