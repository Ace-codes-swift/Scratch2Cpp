#include "scratch/Sb3Archive.hpp"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <filesystem>

#include "utils/Error.hpp"
#include "utils/FileSystem.hpp"
#include "utils/Log.hpp"
#include "utils/Zip.hpp"

namespace s2c {

Sb3FileSource::Sb3FileSource(std::string path) : path_(std::move(path)) {}

RawProject Sb3FileSource::load() {
    log::step("Reading " + path_ + "...");
    if (!std::filesystem::exists(path_)) {
        throw ConversionError("File not found: " + path_);
    }
    RawProject raw = parseSb3(fs::readBinaryFile(path_), path_);
    if (raw.title.empty()) raw.title = std::filesystem::path(path_).stem().string();
    return raw;
}

RawProject parseSb3(const std::vector<std::uint8_t>& bytes, const std::string& description) {
    log::step("Extracting project...");
    ZipArchive zip(bytes);
    if (!zip.hasEntry("project.json")) {
        throw ConversionError("The archive does not contain project.json - is this really an .sb3 file?");
    }
    RawProject raw;
    raw.sourceDescription = description;
    const std::vector<std::uint8_t> jsonBytes = zip.read("project.json");
    raw.projectJson.assign(jsonBytes.begin(), jsonBytes.end());
    try {
        const nlohmann::json root = nlohmann::json::parse(raw.projectJson);
        if (root.contains("meta") && root["meta"].is_object() && root["meta"].contains("title") &&
            root["meta"]["title"].is_string()) {
            raw.title = root["meta"]["title"].get<std::string>();
        }
    } catch (...) {
        // Title is optional; ScratchParser will still accept the project.
    }
    for (const std::string& name : zip.entryNames()) {
        if (name == "project.json") continue;
        raw.assets[name] = zip.read(name);
    }
    return raw;
}

std::vector<std::uint8_t> writeSb3(const RawProject& project) {
    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    if (!mz_zip_writer_init_heap(&zip, 0, 1024 * 1024)) {
        throw ConversionError("Could not initialise zip writer");
    }
    auto fail = [&](const std::string& what) {
        mz_zip_writer_end(&zip);
        throw ConversionError("Could not write .sb3: " + what);
    };
    std::string projectJson = project.projectJson;
    if (!project.title.empty()) {
        try {
            nlohmann::json root = nlohmann::json::parse(projectJson);
            if (!root.contains("meta") || !root["meta"].is_object()) root["meta"] = nlohmann::json::object();
            root["meta"]["title"] = project.title;
            projectJson = root.dump();
        } catch (...) {
        }
    }
    if (!mz_zip_writer_add_mem(&zip, "project.json", projectJson.data(), projectJson.size(),
                               MZ_DEFAULT_COMPRESSION)) {
        fail("project.json");
    }
    for (const auto& [name, bytes] : project.assets) {
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(), MZ_DEFAULT_COMPRESSION)) {
            fail(name);
        }
    }
    void* buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) fail("finalize");
    std::vector<std::uint8_t> out(static_cast<std::uint8_t*>(buf), static_cast<std::uint8_t*>(buf) + size);
    mz_free(buf);
    mz_zip_writer_end(&zip);
    return out;
}

}  // namespace s2c
