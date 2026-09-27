// ScratchDownloader.hpp - downloads a shared project from scratch.mit.edu.
//
// Scratch 3 does not serve .sb3 files directly. The equivalent of downloading
// the .sb3 is:
//   1. GET https://api.scratch.mit.edu/projects/<id>      -> title + project_token
//   2. GET https://projects.scratch.mit.edu/<id>?token=... -> project.json
//   3. GET https://assets.scratch.mit.edu/internalapi/asset/<md5ext>/get/ for
//      each costume and sound referenced by project.json
// The result is exactly the content of an .sb3 archive (and can optionally be
// saved as one with --save-sb3).
#pragma once

#include <optional>
#include <string>

#include "scratch/ProjectSource.hpp"

namespace s2c {

class ScratchWebSource : public ProjectSource {
public:
    explicit ScratchWebSource(std::string projectId, std::string originalUrl);
    RawProject load() override;

    // Extracts the numeric id from URLs like
    // https://scratch.mit.edu/projects/123456789/ (with optional /editor, query...).
    static std::optional<std::string> extractProjectId(const std::string& url);
    static bool looksLikeScratchUrl(const std::string& input);

private:
    std::string projectId_;
    std::string originalUrl_;
};

}  // namespace s2c
