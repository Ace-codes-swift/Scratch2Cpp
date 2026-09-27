#include "scratch/ProjectSource.hpp"

#include "scratch/Sb3Archive.hpp"
#include "scratch/ScratchDownloader.hpp"
#include "utils/Error.hpp"
#include "utils/StringUtils.hpp"

namespace s2c {

std::unique_ptr<ProjectSource> createProjectSource(const std::string& input) {
    const std::string trimmed = str::trim(input);
    if (trimmed.empty()) throw ConversionError("No project URL given. Use --url <scratch project url>.");

    const std::string lower = str::toLower(trimmed);
    if (lower.find("turbowarp.org") != std::string::npos) {
        // Planned: a TurboWarpSource implementing ProjectSource. TurboWarp
        // projects are regular Scratch projects (same project.json), so the
        // parser and code generator will be reused as-is.
        throw ConversionError("TurboWarp URLs are not supported yet. Version 1 supports scratch.mit.edu project URLs "
                              "and local .sb3 files.");
    }
    if (str::endsWith(lower, ".sb3")) {
        return std::make_unique<Sb3FileSource>(trimmed);
    }
    if (ScratchWebSource::looksLikeScratchUrl(trimmed) || std::all_of(trimmed.begin(), trimmed.end(), ::isdigit)) {
        std::optional<std::string> id = ScratchWebSource::extractProjectId(trimmed);
        if (!id) {
            throw ConversionError("Could not find a project id in \"" + trimmed +
                                  "\". Expected something like https://scratch.mit.edu/projects/123456789/");
        }
        return std::make_unique<ScratchWebSource>(*id, trimmed);
    }
    throw ConversionError("Unrecognised project source \"" + trimmed +
                          "\". Expected a scratch.mit.edu project URL or a path to an .sb3 file.");
}

}  // namespace s2c
