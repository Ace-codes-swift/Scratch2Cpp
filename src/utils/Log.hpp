// Log.hpp - console output helpers for the converter CLI.
#pragma once

#include <string>

namespace s2c::log {

void setVerbose(bool verbose);
bool verbose();

void step(const std::string& message);     // "Downloading Scratch project..."
void info(const std::string& message);     // "Found 4 sprites"
void detail(const std::string& message);   // only printed with --verbose
void warning(const std::string& message);  // "Warning: ..."
void error(const std::string& message);    // "Error: ..."

}  // namespace s2c::log
