# Scratch runtime library used by generated projects.
#
# Depends only on SDL3. Image decoding (stb_image) and SVG rasterisation
# (nanosvg) are vendored single-header libraries in third_party/.
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
target_link_libraries(scratch_runtime PUBLIC SDL3::SDL3)

if(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(scratch/GpuAccel.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(scratch_runtime PUBLIC "-framework Metal" "-framework Foundation")
endif()

if(WIN32)
    target_link_libraries(scratch_runtime PUBLIC d3d11 d3dcompiler)
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
