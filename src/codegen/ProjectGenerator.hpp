// ProjectGenerator.hpp - writes a complete, buildable C++/SDL3 project.
//
// Output layout (<output>/<Project Name>/):
//   CMakeLists.txt        finds SDL3 (or fetches it), builds runtime + game
//   README.md             how to build/run/debug + conversion warnings
//   src/main.cpp          entry point
//   src/Project.cpp       creates stage and sprites in layer order
//   src/targets/*.cpp     one file per Scratch target (scripts as coroutines)
//   include/...           matching headers
//   runtime/              the scratch runtime library (SDL3)
//   assets/               costumes and sounds copied from the .sb3
//   scripts/              build/run/debug wrappers around CMake
//   build/                empty; CMake build directory
#pragma once

#include <filesystem>
#include <string>

#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"

namespace s2c::codegen {

struct GenerateOptions {
    std::filesystem::path outputRoot;    // directory that will contain <Project Name>/
    std::string projectDirName;          // override for the directory name (optional)
    bool overwrite = false;              // replace generated files in an existing directory
    int windowScale = 2;                 // window size = stage size * scale
};

struct GenerateResult {
    std::filesystem::path projectDir;
    std::string executableName;
    int filesWritten = 0;
};

// Runs every generator (CppGenerator, RuntimeGenerator, CMakeGenerator,
// ScriptsGenerator, ReadmeGenerator, AssetWriter) and writes the result.
GenerateResult generateProject(const ir::Project& project, Diagnostics& diagnostics, const GenerateOptions& options);

}  // namespace s2c::codegen
