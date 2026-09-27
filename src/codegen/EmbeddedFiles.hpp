// EmbeddedFiles.hpp - runtime sources embedded in the converter executable.
//
// The table is generated at build time by tools/embed_files.cpp from the
// runtime/ directory, so a single scratch2cpp binary can emit a complete,
// self-contained project anywhere.
#pragma once

#include <cstddef>
#include <vector>

namespace s2c::embedded {

struct File {
    const char* path;             // relative to runtime/, e.g. "scratch/Sprite.hpp"
    const unsigned char* data;
    std::size_t size;
};

const std::vector<File>& runtimeFiles();

}  // namespace s2c::embedded
