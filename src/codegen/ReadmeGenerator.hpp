// ReadmeGenerator.hpp - README.md for generated projects.
#pragma once

#include <string>

#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"

namespace s2c::codegen {

std::string generateReadme(const ir::Project& project, const Diagnostics& diagnostics, const std::string& executableName);

}  // namespace s2c::codegen
