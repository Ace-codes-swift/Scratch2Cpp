#include "codegen/ProjectGenerator.hpp"

#include <set>

#include "codegen/CMakeGenerator.hpp"
#include "codegen/CppGenerator.hpp"
#include "codegen/EmbeddedFiles.hpp"
#include "codegen/ReadmeGenerator.hpp"
#include "codegen/ScriptsGenerator.hpp"
#include "utils/Error.hpp"
#include "utils/FileSystem.hpp"
#include "utils/Log.hpp"
#include "utils/StringUtils.hpp"

namespace s2c::codegen {

namespace {

// Directories/files the generator owns and may replace with --force.
const char* const kGeneratedEntries[] = {"src", "include", "runtime", "assets", "scripts", "CMakeLists.txt",
                                          "README.md", ".gitignore"};

void prepareOutputDirectory(const std::filesystem::path& dir, bool overwrite) {
    if (std::filesystem::exists(dir)) {
        bool hasGenerated = false;
        for (const char* entry : kGeneratedEntries) {
            if (std::filesystem::exists(dir / entry)) hasGenerated = true;
        }
        if (hasGenerated) {
            if (!overwrite) {
                throw ConversionError("Output directory \"" + dir.string() +
                                      "\" already contains a generated project. Use --force to overwrite it.");
            }
            for (const char* entry : kGeneratedEntries) {
                std::error_code ec;
                std::filesystem::remove_all(dir / entry, ec);
            }
        }
    }
    fs::ensureDirectory(dir);
}

// Writes the embedded runtime sources into <project>/runtime/.
int writeRuntime(const std::filesystem::path& projectDir) {
    int count = 0;
    for (const embedded::File& f : embedded::runtimeFiles()) {
        std::vector<std::uint8_t> bytes(f.data, f.data + f.size);
        fs::writeBinaryFile(projectDir / "runtime" / f.path, bytes);
        ++count;
    }
    return count;
}

// Copies every referenced costume/sound into <project>/assets/.
int writeAssets(const ir::Project& project, const std::filesystem::path& projectDir, Diagnostics& diagnostics) {
    std::set<std::string> wanted;
    for (const ir::Target& t : project.targets) {
        for (const ir::Costume& c : t.costumes) wanted.insert(c.md5ext);
        for (const ir::Sound& s : t.sounds) wanted.insert(s.md5ext);
    }
    int count = 0;
    fs::ensureDirectory(projectDir / "assets");
    for (const std::string& name : wanted) {
        auto it = project.assets.find(name);
        if (it == project.assets.end()) {
            diagnostics.warning("Asset " + name + " was not available and was not copied");
            continue;
        }
        fs::writeBinaryFile(projectDir / "assets" / name, it->second);
        ++count;
    }
    return count;
}

}  // namespace

GenerateResult generateProject(const ir::Project& project, Diagnostics& diagnostics, const GenerateOptions& options) {
    GenerateResult result;
    const std::string dirName = options.projectDirName.empty() ? str::sanitizeFileName(project.title) : options.projectDirName;
    result.projectDir = options.outputRoot / dirName;
    prepareOutputDirectory(result.projectDir, options.overwrite);

    log::step("Generating C++...");
    CppGenerator cpp(project, diagnostics, options.windowScale);
    CppSources sources = cpp.generate();
    result.executableName = sources.executableName;
    for (const auto& [path, contents] : sources.files) {
        fs::writeTextFile(result.projectDir / path, contents);
        ++result.filesWritten;
    }

    log::step("Generating runtime...");
    result.filesWritten += writeRuntime(result.projectDir);

    log::step("Copying assets...");
    result.filesWritten += writeAssets(project, result.projectDir, diagnostics);

    log::step("Generating CMake project...");
    fs::writeTextFile(result.projectDir / "CMakeLists.txt", generateCMakeLists(project, sources.executableName, sources.sourceFiles));
    fs::writeTextFile(result.projectDir / ".gitignore", generateGitignore());
    for (const auto& [path, contents] : generateScripts(sources.executableName)) {
        fs::writeTextFile(result.projectDir / path, contents);
        if (str::endsWith(path, ".sh")) fs::makeExecutable(result.projectDir / path);
        ++result.filesWritten;
    }
    fs::writeTextFile(result.projectDir / "README.md", generateReadme(project, diagnostics, sources.executableName));
    fs::ensureDirectory(result.projectDir / "build");
    result.filesWritten += 3;
    return result;
}

}  // namespace s2c::codegen
