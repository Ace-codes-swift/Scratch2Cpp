#include "codegen/ScriptsGenerator.hpp"

namespace s2c::codegen {

std::map<std::string, std::string> generateScripts(const std::string& exe) {
    std::map<std::string, std::string> files;

    // Locate the built binary for single-config (Ninja/Make) and multi-config
    // (Visual Studio, Xcode) generators on macOS, Windows, and Linux.
    const std::string findExeSh =
        "find_exe() {\n"
        "    local root=\"$1\" name=\"$2\"\n"
        "    local p\n"
        "    for p in \\\n"
        "        \"$root/$name\" \\\n"
        "        \"$root/Release/$name\" \\\n"
        "        \"$root/RelWithDebInfo/$name\" \\\n"
        "        \"$root/Debug/$name\" \\\n"
        "        \"$root/$name.exe\" \\\n"
        "        \"$root/Release/$name.exe\" \\\n"
        "        \"$root/RelWithDebInfo/$name.exe\" \\\n"
        "        \"$root/Debug/$name.exe\"\n"
        "    do\n"
        "        if [[ -f \"$p\" ]]; then\n"
        "            printf '%s\\n' \"$p\"\n"
        "            return 0\n"
        "        fi\n"
        "    done\n"
        "    return 1\n"
        "}\n";

    files["scripts/build.sh"] =
        "#!/usr/bin/env bash\n"
        "# Configure and build on macOS or Linux. Usage: scripts/build.sh [Release|Debug]\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "CONFIG=\"${1:-Release}\"\n"
        "cmake -S . -B build -DCMAKE_BUILD_TYPE=\"$CONFIG\"\n"
        "cmake --build build --config \"$CONFIG\" --parallel\n" +
        findExeSh +
        "if EXE=\"$(find_exe build \"" + exe + "\")\"; then\n"
        "    echo \"Built: $EXE\"\n"
        "else\n"
        "    echo \"Built (executable not found in the usual places; check build/)\"\n"
        "fi\n";

    files["scripts/run.sh"] =
        "#!/usr/bin/env bash\n"
        "# Build (Release) and run the project on macOS or Linux.\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "scripts/build.sh Release\n" +
        findExeSh +
        "EXE=\"$(find_exe build \"" + exe + "\")\" || {\n"
        "    echo \"Could not find " + exe + " under build/.\" >&2\n"
        "    exit 1\n"
        "}\n"
        "exec \"$EXE\" \"$@\"\n";

    files["scripts/debug.sh"] =
        "#!/usr/bin/env bash\n"
        "# Build a Debug configuration into build/debug and launch it under a debugger.\n"
        "# Uses lldb when available (macOS), then gdb (Linux); otherwise runs the binary.\n"
        "set -euo pipefail\n"
        "cd \"$(dirname \"$0\")/..\"\n"
        "cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug\n"
        "cmake --build build/debug --config Debug --parallel\n" +
        findExeSh +
        "EXE=\"$(find_exe build/debug \"" + exe + "\")\" || {\n"
        "    echo \"Could not find the Debug build of " + exe + ".\" >&2\n"
        "    exit 1\n"
        "}\n"
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
        "REM Configure and build on Windows. Usage: scripts\\build.bat [Release|Debug]\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0..\"\r\n"
        "set CONFIG=%1\r\n"
        "if \"%CONFIG%\"==\"\" set CONFIG=Release\r\n"
        "cmake -S . -B build -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1\r\n"
        "cmake --build build --config %CONFIG% --parallel || exit /b 1\r\n"
        "if exist \"build\\%CONFIG%\\" + exe + ".exe\" (\r\n"
        "    echo Built: build\\%CONFIG%\\" + exe + ".exe\r\n"
        ") else if exist \"build\\" + exe + ".exe\" (\r\n"
        "    echo Built: build\\" + exe + ".exe\r\n"
        ") else (\r\n"
        "    echo Built. Look for " + exe + ".exe under build\\\r\n"
        ")\r\n";

    files["scripts/run.bat"] =
        "@echo off\r\n"
        "REM Build (Release) and run the project on Windows.\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0..\"\r\n"
        "call scripts\\build.bat Release || exit /b 1\r\n"
        "set EXE=build\\Release\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" set EXE=build\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" set EXE=build\\RelWithDebInfo\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" (\r\n"
        "    echo Could not find " + exe + ".exe under build\\.\r\n"
        "    exit /b 1\r\n"
        ")\r\n"
        "\"%EXE%\" %*\r\n";

    files["scripts/debug.bat"] =
        "@echo off\r\n"
        "REM Build a Debug configuration into build\\debug and launch it under a debugger if one is found.\r\n"
        "setlocal\r\n"
        "cd /d \"%~dp0..\"\r\n"
        "cmake -S . -B build\\debug -DCMAKE_BUILD_TYPE=Debug || exit /b 1\r\n"
        "cmake --build build\\debug --config Debug --parallel || exit /b 1\r\n"
        "set EXE=build\\debug\\Debug\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" set EXE=build\\debug\\" + exe + ".exe\r\n"
        "if not exist \"%EXE%\" (\r\n"
        "    echo Could not find the Debug build of " + exe + ".exe\r\n"
        "    exit /b 1\r\n"
        ")\r\n"
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
        "echo No gdb/lldb found. Open the .sln under build\\debug in Visual Studio to debug, or running directly:\r\n"
        "\"%EXE%\" %*\r\n";

    return files;
}

}  // namespace s2c::codegen
