#include "scratch/ScratchDownloader.hpp"

#include <nlohmann/json.hpp>

#include <regex>
#include <set>

#include "utils/Error.hpp"
#include "utils/Http.hpp"
#include "utils/Log.hpp"
#include "utils/StringUtils.hpp"

namespace s2c {

using nlohmann::json;

ScratchWebSource::ScratchWebSource(std::string projectId, std::string originalUrl)
    : projectId_(std::move(projectId)), originalUrl_(std::move(originalUrl)) {}

bool ScratchWebSource::looksLikeScratchUrl(const std::string& input) {
    const std::string lower = str::toLower(input);
    return lower.find("scratch.mit.edu") != std::string::npos;
}

std::optional<std::string> ScratchWebSource::extractProjectId(const std::string& url) {
    // Accept: https://scratch.mit.edu/projects/123/, scratch.mit.edu/projects/123/editor/,
    // https://scratch.mit.edu/projects/123?foo=bar, and a bare numeric id.
    static const std::regex idOnly(R"(^\s*(\d+)\s*$)");
    static const std::regex urlPattern(R"(scratch\.mit\.edu/projects/(\d+))", std::regex::icase);
    std::smatch m;
    if (std::regex_match(url, m, idOnly)) return m[1].str();
    if (std::regex_search(url, m, urlPattern)) return m[1].str();
    return std::nullopt;
}

namespace {

std::set<std::string> collectAssetNames(const json& project) {
    std::set<std::string> names;
    if (!project.contains("targets") || !project["targets"].is_array()) {
        throw ConversionError("project.json has no \"targets\" array - this does not look like a Scratch 3 project");
    }
    for (const json& target : project["targets"]) {
        for (const char* key : {"costumes", "sounds"}) {
            if (!target.contains(key) || !target[key].is_array()) continue;
            for (const json& asset : target[key]) {
                std::string md5ext;
                if (asset.contains("md5ext") && asset["md5ext"].is_string()) {
                    md5ext = asset["md5ext"].get<std::string>();
                } else if (asset.contains("assetId") && asset.contains("dataFormat")) {
                    md5ext = asset["assetId"].get<std::string>() + "." + asset["dataFormat"].get<std::string>();
                }
                if (!md5ext.empty()) names.insert(md5ext);
            }
        }
    }
    return names;
}

}  // namespace

RawProject ScratchWebSource::load() {
    HttpClient http;
    RawProject raw;
    raw.sourceDescription = originalUrl_;

    log::step("Downloading Scratch project " + projectId_ + "...");
    log::detail(std::string("HTTP backend: ") + HttpClient::backendName());

    // 1. Project metadata (title + access token).
    const std::string metaUrl = "https://api.scratch.mit.edu/projects/" + projectId_;
    HttpResponse meta = http.get(metaUrl);
    if (meta.status == 404) {
        throw ConversionError("Scratch project " + projectId_ +
                              " was not found. Check the URL and make sure the project is shared.");
    }
    if (!meta.ok()) {
        throw ConversionError("Scratch API returned HTTP " + std::to_string(meta.status) + " for " + metaUrl);
    }
    json metaJson;
    try {
        metaJson = json::parse(meta.body.begin(), meta.body.end());
    } catch (const std::exception& e) {
        throw ConversionError(std::string("Could not parse project metadata: ") + e.what());
    }
    raw.title = metaJson.value("title", "Scratch Project " + projectId_);
    const std::string token = metaJson.value("project_token", "");

    // 2. project.json
    std::string projectUrl = "https://projects.scratch.mit.edu/" + projectId_;
    if (!token.empty()) projectUrl += "?token=" + token;
    HttpResponse project = http.get(projectUrl);
    if (!project.ok()) {
        throw ConversionError("Could not download project.json (HTTP " + std::to_string(project.status) +
                              "). The project may be unshared.");
    }
    raw.projectJson.assign(project.body.begin(), project.body.end());
    json projectJson;
    try {
        projectJson = json::parse(raw.projectJson);
    } catch (const std::exception& e) {
        throw ConversionError(std::string("project.json is not valid JSON: ") + e.what());
    }
    if (projectJson.contains("objName")) {
        throw ConversionError("This is a Scratch 2 project (project.json v2). Only Scratch 3 projects are supported.");
    }

    // 3. Assets.
    const std::set<std::string> assetNames = collectAssetNames(projectJson);
    log::info("Downloading " + std::to_string(assetNames.size()) + " assets...");
    size_t index = 0;
    for (const std::string& name : assetNames) {
        ++index;
        const std::string url = "https://assets.scratch.mit.edu/internalapi/asset/" + name + "/get/";
        log::detail("[" + std::to_string(index) + "/" + std::to_string(assetNames.size()) + "] " + name);
        HttpResponse asset = http.get(url);
        if (!asset.ok()) {
            log::warning("Could not download asset " + name + " (HTTP " + std::to_string(asset.status) + ")");
            continue;
        }
        raw.assets[name] = std::move(asset.body);
    }
    return raw;
}

}  // namespace s2c
