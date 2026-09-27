// Http.hpp - minimal HTTP GET client.
//
// Uses libcurl when the converter was built with it, otherwise shells out to
// the `curl` command line tool (available by default on macOS and Windows 10+).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace s2c {

struct HttpResponse {
    long status = 0;
    std::vector<std::uint8_t> body;
    bool ok() const { return status >= 200 && status < 300; }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Performs a GET request. Throws ConversionError on transport failure;
    // HTTP error statuses are returned in the response.
    HttpResponse get(const std::string& url);

    static const char* backendName();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace s2c
