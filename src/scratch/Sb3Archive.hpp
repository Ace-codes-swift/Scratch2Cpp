// Sb3Archive.hpp - reading and writing .sb3 files.
//
// An .sb3 is a ZIP archive with project.json at the root plus one file per
// asset named "<md5>.<ext>".
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "scratch/ProjectSource.hpp"

namespace s2c {

class Sb3FileSource : public ProjectSource {
public:
    explicit Sb3FileSource(std::string path);
    RawProject load() override;

private:
    std::string path_;
};

// Parses .sb3 bytes (from any origin) into a RawProject.
RawProject parseSb3(const std::vector<std::uint8_t>& bytes, const std::string& description);

// Serialises a RawProject back into an .sb3 archive (used by --save-sb3).
std::vector<std::uint8_t> writeSb3(const RawProject& project);

}  // namespace s2c
