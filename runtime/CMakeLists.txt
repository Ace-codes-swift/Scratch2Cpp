# Scratch runtime library used by generated projects.
#
# Same sources on macOS, Windows, and Linux. Image decoding (stb_image) and
# SVG rasterisation (nanosvg) are vendored single-header libraries.
# Optional GPU: Metal (Apple), Vulkan (dynamic loader), Direct3D 11 (Windows).
cmake_minimum_required(VERSION 3.21)

add_library(scratch_runtime STATIC
    scratch/Audio.cpp
    scratch/Coroutine.cpp
    scratch/Input.cpp
    scratch/List.cpp
    scratch/Ops.cpp
    scratch/Pen.cpp
    scratch/Renderer.cpp
    scratch/Runtime.cpp
    scratch/Sprite.cpp
    scratch/SvgOverlay.cpp
    scratch/Stage.cpp
    scratch/Target.cpp
    scratch/Value.cpp
    scratch/Font.cpp
    scratch/GpuAccel.cpp
    scratch/GpuVulkan.cpp
    scratch/GpuD3D11.cpp
)

target_compile_features(scratch_runtime PUBLIC cxx_std_20)
target_include_directories(scratch_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
# Threads: std::thread in the CPU mesh tracer. CMAKE_DL_LIBS: dlopen of the
# Vulkan loader on Linux/macOS (empty on Windows).
target_link_libraries(scratch_runtime PUBLIC SDL3::SDL3 Threads::Threads ${CMAKE_DL_LIBS})

if(WIN32)
    target_compile_definitions(scratch_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_definitions(scratch_runtime PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(scratch_runtime PUBLIC d3d11 d3dcompiler)
endif()

if(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(scratch/GpuAccel.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(scratch_runtime PUBLIC "-framework Metal" "-framework Foundation")
endif()

find_path(S2C_VULKAN_INCLUDE vulkan/vulkan.h
    HINTS
        "$ENV{VULKAN_SDK}/Include"
        "$ENV{VULKAN_SDK}/include"
        /opt/homebrew/include
        /usr/local/include
        /usr/include
)
if(S2C_VULKAN_INCLUDE)
    message(STATUS "scratch runtime: Vulkan headers at ${S2C_VULKAN_INCLUDE}")
    target_compile_definitions(scratch_runtime PRIVATE SCRATCH_HAS_VULKAN)
    target_include_directories(scratch_runtime PRIVATE ${S2C_VULKAN_INCLUDE})
else()
    message(STATUS "scratch runtime: Vulkan headers not found; Vulkan backend disabled")
endif()

if(MSVC)
    target_compile_options(scratch_runtime PRIVATE /W3 /utf-8)
else()
    target_compile_options(scratch_runtime PRIVATE -Wall -Wextra)
    # Vendored headers are not ours to fix.
    set_source_files_properties(scratch/Renderer.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function;-Wno-sign-compare;-Wno-unused-parameter")
    set_source_files_properties(scratch/Font.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")
    set_source_files_properties(scratch/GpuVulkan.cpp PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers")
endif()
