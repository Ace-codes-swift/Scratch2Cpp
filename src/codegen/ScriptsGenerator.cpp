#include "codegen/ScriptsGenerator.hpp"

namespace s2c::codegen {

std::map<std::string, std::string> generateScripts(const std::string& exe) {
    std::map<std::string, std::string> files;

    files["scripts/build.sh"] =
        "#!/usr/bin/env bash\n"
        "# Configure and build the project with CMake. Usage: scripts/build.sh [Release|Debug]\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "CONFIG=\"${1:-Release}\"\n"
        "cmake -S . -B build -DCMAKE_BUILD_TYPE=\"$CONFIG\"\n"
        "cmake --build build --config \"$CONFIG\" --parallel\n"
        "echo \"Built: build/" + exe + "\"\n";

    files["scripts/run.sh"] =
        "#!/usr/bin/env bash\n"
        "# Build (Release) and run the project.\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "scripts/build.sh Release\n"
        "exec \"build/" + exe + "\" \"$@\"\n";

    files["scripts/debug.sh"] =
        "#!/usr/bin/env bash\n"
        "# Build a Debug configuration into build/debug and launch it under a debugger.\n"
        "# Uses lldb when available, then gdb; falls back to running the binary directly.\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug\n"
        "cmake --build build/debug --config Debug --parallel\n"
        "EXE=\"build/debug/" + exe + "\"\n"
        "if command -v lldb >/dev/null 2>&1; then\n"
        "    echo \"Launching under lldb (type 'bt' after a crash, 'q' to quit)...\"\n"
        "    exec lldb -o run -- \"$EXE\" \"$@\"\n"
        "elif command -v gdb >/dev/null 2>&1; then\n"
        "    echo \"Launching under gdb (type 'bt' after a crash, 'q' to quit)...\"\n"
        "    exec gdb -q -ex run --args \"$EXE\" \"$@\"\n"
        "else\n"
        "    echo \"No debugger (lldb/gdb) found; running the Debug build directly.\"\n"
        "    exec \"$EXE\" \"$@\"\n"
        "fi\n";

    files["scripts/build.bat"] =
        "@echo off\r\n"
        "REM Configure and build the project with CMake. Usage: scripts\\build.bat [Release|Debug]\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0..\"\r\n"
        "set CONFIG=%1\r\n"
        "if \"%CONFIG%\"==\"\" set CONFIG=Release\r\n"
        "cmake -S . -B build -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1\r\n"
        "cmake --build build --config %CONFIG% --parallel || exit /b 1\r\n"
        "echo Built: build\\%CONFIG%\\" + exe + ".exe (or build\\" + exe + ".exe for single-config generators)\r\n";

    files["scripts/debug.bat"] =
        "@echo off\r\n"
        "REM Build a Debug configuration into build\\debug and launch it under a debugger if one is found.\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0..\"\r\n"
        "cmake -S . -B build\\debug -DCMAKE_BUILD_TYPE=Debug || exit /b 1\r\n"
        "cmake --build build\\debug --config Debug --parallel || exit /b 1\r\n"
        "set EXE=build\\debug\\Debug\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" set EXE=build\\debug\\" + exe + ".exe\r\n"
        "where gdb >nul 2>nul\r\n"
        "if %errorlevel%==0 (\r\n"
        "    gdb -q -ex run --args \"%EXE%\" %*\r\n"
        "    exit /b\r\n"
        ")\r\n"
        "where lldb >nul 2>nul\r\n"
        "if %errorlevel%==0 (\r\n"
        "    lldb -o run -- \"%EXE%\" %*\r\n"
        "    exit /b\r\n"
        ")\r\n"
        "echo No gdb/lldb found. Open build\\debug\\" + exe + ".sln in Visual Studio to debug, or running directly:\r\n"
        "\"%EXE%\" %*\r\n";

    return files;
}

}  // namespace s2c::codegen
