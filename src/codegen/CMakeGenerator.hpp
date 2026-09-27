// CMakeGenerator.hpp - CMakeLists.txt (and .gitignore) for generated projects.
#pragma once

#include <string>
#include <vector>

#include "ir/Project.hpp"

namespace s2c::codegen {

std::string generateCMakeLists(const ir::Project& project, const std::string& executableName,
                               const std::vector<std::string>& sourceFiles);
std::string generateGitignore();

}  // namespace s2c::codegen
