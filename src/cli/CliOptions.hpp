// CliOptions.hpp - command line parsing for scratch2cpp.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace s2c::cli {

struct Options {
    std::string input;              // --url (Scratch URL / id) or --sb3 path
    std::string output = "./projects";
    std::string projectName;        // --name override for the directory name
    std::string saveSb3;            // --save-sb3 <file>
    bool force = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    int windowScale = 2;
};

// Returns the parsed options or an error message.
struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

ParseResult parse(int argc, char** argv);
std::string usage(const std::string& program);
const char* versionString();

}  // namespace s2c::cli
