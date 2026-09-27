# scratch2cpp - Scratch 3 -> C++/SDL3 converter.
#
# Dependencies: a C++20 compiler, CMake >= 3.21 and (optionally) libcurl.
# nlohmann/json and miniz are vendored in third_party/. Without libcurl the
# converter falls back to invoking the `curl` command line tool.
cmake_minimum_required(VERSION 3.21)
project(scratch2cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(S2C_USE_LIBCURL "Use libcurl for downloads when available" ON)
option(S2C_BUILD_TESTS "Build unit tests" ON)

# ---- Vendored third-party code -------------------------------------------------------------
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)
target_compile_definitions(miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE third_party)

# ---- Embed the runtime sources into the converter ----------------------------------------------
# The generated project must be self-contained, so the converter carries a copy
# of runtime/ and writes it out next to the generated code.
add_executable(embed_files tools/embed_files.cpp)

file(GLOB_RECURSE S2C_RUNTIME_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/scratch/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/scratch/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/third_party/*.h
)
set(S2C_EMBEDDED_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedRuntime.cpp)
add_custom_command(
    OUTPUT ${S2C_EMBEDDED_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND embed_files ${S2C_EMBEDDED_CPP} ${CMAKE_CURRENT_SOURCE_DIR}/runtime ${S2C_RUNTIME_FILES}
    DEPENDS embed_files ${S2C_RUNTIME_FILES}
    COMMENT "Embedding runtime sources"
    VERBATIM
)

# ---- Converter -------------------------------------------------------------------------------------
add_library(s2c_core STATIC
    src/cli/CliOptions.cpp
    src/utils/Log.cpp
    src/utils/StringUtils.cpp
    src/utils/FileSystem.cpp
    src/utils/Http.cpp
    src/utils/Zip.cpp
    src/ir/Diagnostics.cpp
    src/scratch/ProjectSource.cpp
    src/scratch/ScratchDownloader.cpp
    src/scratch/Sb3Archive.cpp
    src/scratch/ScratchParser.cpp
    src/codegen/BlockRegistry.cpp
    src/codegen/TargetCompiler.cpp
    src/codegen/CppGenerator.cpp
    src/codegen/CMakeGenerator.cpp
    src/codegen/ScriptsGenerator.cpp
    src/codegen/ReadmeGenerator.cpp
    src/codegen/ProjectGenerator.cpp
    src/codegen/blocks/MotionBlocks.cpp
    src/codegen/blocks/LooksBlocks.cpp
    src/codegen/blocks/SoundBlocks.cpp
    src/codegen/blocks/EventBlocks.cpp
    src/codegen/blocks/ControlBlocks.cpp
    src/codegen/blocks/SensingBlocks.cpp
    src/codegen/blocks/OperatorBlocks.cpp
    src/codegen/blocks/DataBlocks.cpp
    src/codegen/blocks/ProcedureBlocks.cpp
    src/codegen/blocks/PenBlocks.cpp
    ${S2C_EMBEDDED_CPP}
)
target_include_directories(s2c_core PUBLIC src)
target_link_libraries(s2c_core PUBLIC miniz nlohmann_json)
if(MSVC)
    target_compile_options(s2c_core PRIVATE /W3 /utf-8 /bigobj)
else()
    target_compile_options(s2c_core PRIVATE -Wall -Wextra)
endif()

if(S2C_USE_LIBCURL)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        message(STATUS "scratch2cpp: downloads via libcurl")
        target_link_libraries(s2c_core PUBLIC CURL::libcurl)
        target_compile_definitions(s2c_core PRIVATE S2C_HAVE_CURL)
    else()
        message(STATUS "scratch2cpp: libcurl not found, downloads via the curl command line tool")
    endif()
endif()

add_executable(scratch2cpp src/main.cpp)
target_link_libraries(scratch2cpp PRIVATE s2c_core)

install(TARGETS scratch2cpp RUNTIME DESTINATION bin)

# ---- Tests -------------------------------------------------------------------------------------------
if(S2C_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
