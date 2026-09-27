// StringUtils.hpp - small string helpers shared across the converter.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace s2c::str {

std::string toLower(std::string s);
std::string trim(std::string_view s);
bool startsWith(std::string_view s, std::string_view prefix);
bool endsWith(std::string_view s, std::string_view suffix);
std::string replaceAll(std::string s, std::string_view from, std::string_view to);
std::vector<std::string> split(std::string_view s, char delimiter);

// Turns an arbitrary project/sprite name into a safe directory name.
std::string sanitizeFileName(std::string_view name, std::string_view fallback = "ScratchProject");

// Turns an arbitrary name into a valid C++ identifier fragment
// ([A-Za-z0-9_], not starting with a digit, no reserved words).
std::string sanitizeIdentifier(std::string_view name, std::string_view fallback = "item");

// Produces a double-quoted C++ string literal with all escapes applied.
std::string cppStringLiteral(std::string_view s);

// Formats a double as a C++ literal that round-trips exactly.
std::string cppDoubleLiteral(double value);

}  // namespace s2c::str
