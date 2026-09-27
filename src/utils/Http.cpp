#include "utils/Http.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "utils/Error.hpp"
#include "utils/FileSystem.hpp"

#ifdef S2C_HAVE_CURL
#include <curl/curl.h>
#endif

namespace s2c {

#ifdef S2C_HAVE_CURL

struct HttpClient::Impl {
    CURL* curl = nullptr;
};

namespace {
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::vector<std::uint8_t>*>(userdata);
    body->insert(body->end(), reinterpret_cast<std::uint8_t*>(ptr),
                 reinterpret_cast<std::uint8_t*>(ptr) + size * nmemb);
    return size * nmemb;
}
}  // namespace

HttpClient::HttpClient() : impl_(new Impl) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    impl_->curl = curl_easy_init();
    if (!impl_->curl) throw ConversionError("Failed to initialise libcurl");
}

HttpClient::~HttpClient() {
    if (impl_) {
        if (impl_->curl) curl_easy_cleanup(impl_->curl);
        delete impl_;
    }
    curl_global_cleanup();
}

HttpResponse HttpClient::get(const std::string& url) {
    HttpResponse response;
    CURL* curl = impl_->curl;
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "scratch2cpp/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        throw ConversionError("Network request to " + url + " failed: " + curl_easy_strerror(rc));
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

const char* HttpClient::backendName() { return "libcurl"; }

#else  // ---- fallback: curl command line ----------------------------------------

struct HttpClient::Impl {};

HttpClient::HttpClient() : impl_(new Impl) {}
HttpClient::~HttpClient() { delete impl_; }

namespace {
std::string tempFilePath() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream name;
    name << "scratch2cpp_" << std::hex << rng() << ".tmp";
    return (std::filesystem::temp_directory_path() / name.str()).string();
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    return "\"" + s + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out += "'";
    return out;
#endif
}
}  // namespace

HttpResponse HttpClient::get(const std::string& url) {
    const std::string tmp = tempFilePath();
    const std::string command = "curl -s -L -A scratch2cpp/1.0 -o " + shellQuote(tmp) +
                                " -w \"%{http_code}\" " + shellQuote(url);
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) throw ConversionError("Could not run curl. Install curl or build with libcurl.");
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
#ifdef _WIN32
    const int rc = _pclose(pipe);
#else
    const int rc = pclose(pipe);
#endif
    HttpResponse response;
    response.status = std::atol(output.c_str());
    if (rc != 0 && response.status == 0) {
        std::filesystem::remove(tmp);
        throw ConversionError("Network request to " + url + " failed (curl exit code " + std::to_string(rc) + ")");
    }
    if (std::filesystem::exists(tmp)) {
        response.body = fs::readBinaryFile(tmp);
        std::filesystem::remove(tmp);
    }
    return response;
}

const char* HttpClient::backendName() { return "curl command line"; }

#endif

}  // namespace s2c
