#include "utils/Log.hpp"

#include <iostream>

namespace s2c::log {

namespace {
bool g_verbose = false;
}

void setVerbose(bool verbose) { g_verbose = verbose; }
bool verbose() { return g_verbose; }

void step(const std::string& message) { std::cout << message << std::endl; }
void info(const std::string& message) { std::cout << message << std::endl; }

void detail(const std::string& message) {
    if (g_verbose) std::cout << "  " << message << std::endl;
}

void warning(const std::string& message) { std::cerr << "Warning: " << message << std::endl; }
void error(const std::string& message) { std::cerr << "Error: " << message << std::endl; }

}  // namespace s2c::log
