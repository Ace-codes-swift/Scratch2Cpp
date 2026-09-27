// CppGenerator.hpp - IR -> C++ sources for the generated game.
//
// Produces (as in-memory files, relative to the project directory):
//   include/targets/<Class>.hpp, src/targets/<Class>.cpp   (via TargetCompiler)
//   include/Project.hpp, src/Project.cpp                    (target registration)
//   src/main.cpp                                            (entry point)
#pragma once

#include <map>
#include <string>
#include <vector>

#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"

namespace s2c::codegen {

struct CppSources {
    std::map<std::string, std::string> files;     // relative path -> contents
    std::vector<std::string> sourceFiles;         // src/... entries for CMake
    std::string executableName;
};

class CppGenerator {
public:
    CppGenerator(const ir::Project& project, Diagnostics& diagnostics, int windowScale)
        : project_(project), diagnostics_(diagnostics), windowScale_(windowScale) {}

    CppSources generate();

private:
    std::string generateMain() const;
    std::string generateProjectHeader() const;
    std::string generateProjectSource(const std::vector<std::string>& classNames) const;

    const ir::Project& project_;
    Diagnostics& diagnostics_;
    int windowScale_;
};

}  // namespace s2c::codegen
