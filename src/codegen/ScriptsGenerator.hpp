// ScriptsGenerator.hpp - build/run/debug convenience scripts.
//
// The scripts are thin wrappers around CMake:
//   scripts/build.sh  [Release|Debug]   configure + build into build/
//   scripts/run.sh                       build then launch
//   scripts/debug.sh                     Debug build into build/debug, then lldb/gdb
//   scripts/build.bat, scripts/debug.bat Windows equivalents
#pragma once

#include <map>
#include <string>

namespace s2c::codegen {

// Returns relative path -> contents.
std::map<std::string, std::string> generateScripts(const std::string& executableName);

}  // namespace s2c::codegen
