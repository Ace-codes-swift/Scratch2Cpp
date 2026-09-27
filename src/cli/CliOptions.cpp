#include "cli/CliOptions.hpp"

#include <cstdlib>

namespace s2c::cli {

const char* versionString() { return "scratch2cpp 0.1.0"; }

std::string usage(const std::string& program) {
    return "Usage:\n"
           "  " + program + " --url <scratch project url> [--output <dir>] [options]\n"
           "  " + program + " --sb3 <file.sb3> [--output <dir>] [options]\n"
           "\n"
           "Converts a Scratch 3 project into a standalone C++ project using SDL3.\n"
           "The converter and every generated project build natively on macOS, Windows, and Linux:\n"
           "  cmake -S . -B build && cmake --build build --config Release\n"
           "\n"
           "Options:\n"
           "  --url <url>        Scratch project URL, e.g. https://scratch.mit.edu/projects/123456789/\n"
           "                     (a bare project id is accepted too)\n"
           "  --sb3 <file>       Convert a local .sb3 file instead of downloading\n"
           "  --output <dir>     Directory that will contain the generated project (default: ./projects)\n"
           "                     Use a relative path such as ./out  (/out is the root of the disk)\n"
           "  --name <name>      Override the generated project directory name\n"
           "  --force            Overwrite a previously generated project in the output directory\n"
           "  --save-sb3 <file>  Also save the downloaded project as an .sb3 archive\n"
           "  --scale <n>        Window scale of the generated game (window = 480x360 * n, default 2)\n"
           "  --verbose          Print per-asset / per-sprite progress\n"
           "  --help             Show this help\n"
           "  --version          Show the version\n";
}

ParseResult parse(int argc, char** argv) {
    ParseResult result;
    Options& o = result.options;
    auto needValue = [&](int& i, const std::string& flag) -> const char* {
        if (i + 1 >= argc) {
            result.error = "Option " + flag + " requires a value";
            return nullptr;
        }
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            o.showHelp = true;
        } else if (arg == "--version" || arg == "-V") {
            o.showVersion = true;
        } else if (arg == "--verbose" || arg == "-v") {
            o.verbose = true;
        } else if (arg == "--force" || arg == "-f") {
            o.force = true;
        } else if (arg == "--url" || arg == "-u" || arg == "--sb3") {
            if (const char* v = needValue(i, arg)) o.input = v;
        } else if (arg == "--output" || arg == "-o") {
            if (const char* v = needValue(i, arg)) o.output = v;
        } else if (arg == "--name" || arg == "-n") {
            if (const char* v = needValue(i, arg)) o.projectName = v;
        } else if (arg == "--save-sb3") {
            if (const char* v = needValue(i, arg)) o.saveSb3 = v;
        } else if (arg == "--scale") {
            if (const char* v = needValue(i, arg)) {
                o.windowScale = std::atoi(v);
                if (o.windowScale < 1 || o.windowScale > 8) result.error = "--scale must be between 1 and 8";
            }
        } else if (!arg.empty() && arg[0] == '-') {
            result.error = "Unknown option: " + arg;
        } else if (o.input.empty()) {
            o.input = arg;   // positional URL / file
        } else {
            result.error = "Unexpected argument: " + arg;
        }
        if (result.error) break;
    }
    if (!result.error && !o.showHelp && !o.showVersion && o.input.empty()) {
        result.error = "No project given. Use --url <scratch project url> or --sb3 <file>.";
    }
    return result;
}

}  // namespace s2c::cli
