// ProjectSource.hpp - where a project comes from.
//
// A ProjectSource yields a RawProject: the project.json text plus the raw
// bytes of every asset. The Scratch website, a local .sb3 file and (later)
// TurboWarp are all just different sources producing the same RawProject,
// which the ScratchParser turns into the IR.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace s2c {

struct RawProject {
    std::string title;                     // best-effort project title
    std::string sourceDescription;         // URL or file path, for the README
    std::string projectJson;               // contents of project.json
    std::map<std::string, std::vector<std::uint8_t>> assets;   // md5ext -> bytes
};

class ProjectSource {
public:
    virtual ~ProjectSource() = default;
    // Loads the project, printing progress as it goes. Throws ConversionError.
    virtual RawProject load() = 0;
};

// Chooses the right source for a user-supplied input:
//   - https://scratch.mit.edu/projects/<id>[/...]  -> ScratchWebSource
//   - path ending in .sb3                          -> Sb3FileSource
//   - TurboWarp URLs                               -> clear "not yet supported" error
std::unique_ptr<ProjectSource> createProjectSource(const std::string& input);

}  // namespace s2c
