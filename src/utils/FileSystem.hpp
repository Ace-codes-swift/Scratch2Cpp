// FileSystem.hpp - thin wrappers over std::filesystem with clear errors.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace s2c::fs {

using Path = std::filesystem::path;

void ensureDirectory(const Path& dir);
// Creates `dir` if needed and checks that files can be written there.
// Call this before an expensive download so a bad --output fails immediately.
void requireWritableDirectory(const Path& dir, const std::string& what);
// Hint when `dir` looks like `/out` (absolute) instead of `./out` (relative).
std::string absoluteOutputHint(const Path& dir);
void writeTextFile(const Path& path, const std::string& contents);
void writeBinaryFile(const Path& path, const std::vector<std::uint8_t>& bytes);
std::vector<std::uint8_t> readBinaryFile(const Path& path);
std::string readTextFile(const Path& path);
void makeExecutable(const Path& path);   // chmod +x on POSIX, no-op on Windows

}  // namespace s2c::fs
