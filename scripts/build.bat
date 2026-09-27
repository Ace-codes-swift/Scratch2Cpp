@echo off
REM Configure and build the converter on Windows.
REM Usage: scripts\build.bat [Release|Debug]
setlocal
cd /d "%~dp0.."
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
cmake --build build --config %CONFIG% --parallel || exit /b 1
if exist "build\%CONFIG%\scratch2cpp.exe" (
    echo Built: build\%CONFIG%\scratch2cpp.exe
) else if exist "build\scratch2cpp.exe" (
    echo Built: build\scratch2cpp.exe
) else (
    echo Built. Look for scratch2cpp.exe under build\
)
