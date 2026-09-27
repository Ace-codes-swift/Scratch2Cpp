// Zip.hpp - read-only ZIP archive access (backed by miniz).
//
// Scratch .sb3 files are plain ZIP archives containing project.json and one
// file per asset named by its md5 hash and extension.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace s2c {

class ZipArchive {
public:
    explicit ZipArchive(std::vector<std::uint8_t> bytes);
    ~ZipArchive();
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    std::vector<std::string> entryNames() const;
    bool hasEntry(const std::string& name) const;
    std::vector<std::uint8_t> read(const std::string& name) const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace s2c
