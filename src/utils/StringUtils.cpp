#include "utils/StringUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace s2c::str {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(std::string_view s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return std::string(s);
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::vector<std::string> split(std::string_view s, char delimiter) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t pos = s.find(delimiter, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::string sanitizeFileName(std::string_view name, std::string_view fallback) {
    static const std::string forbidden = "<>:\"/\\|?*";
    std::string out;
    for (char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || forbidden.find(c) != std::string::npos) {
            out.push_back('_');
        } else {
            out.push_back(c);
        }
    }
    // Collapse runs of whitespace and trim dots/spaces (Windows dislikes them).
    std::string collapsed;
    bool lastSpace = false;
    for (char c : out) {
        const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
        if (space && lastSpace) continue;
        collapsed.push_back(space ? ' ' : c);
        lastSpace = space;
    }
    while (!collapsed.empty() && (collapsed.back() == ' ' || collapsed.back() == '.')) collapsed.pop_back();
    while (!collapsed.empty() && (collapsed.front() == ' ' || collapsed.front() == '.')) collapsed.erase(0, 1);
    if (collapsed.size() > 100) collapsed.resize(100);
    static const std::set<std::string> reserved = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4",
                                                   "LPT1", "LPT2", "LPT3", "LPT4"};
    if (collapsed.empty() || reserved.count(collapsed)) return std::string(fallback);
    return collapsed;
}

std::string sanitizeIdentifier(std::string_view name, std::string_view fallback) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            out.push_back(c);
        } else if (c == ' ' || c == '-') {
            out.push_back('_');
        }
        // Other characters (unicode, punctuation) are dropped.
    }
    if (out.empty()) out = std::string(fallback);
    if (std::isdigit(static_cast<unsigned char>(out[0]))) out = "_" + out;
    static const std::set<std::string> keywords = {
        "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch", "char", "class",
        "concept", "const", "constexpr", "continue", "default", "delete", "do", "double", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int",
        "long", "mutable", "namespace", "new", "noexcept", "not", "nullptr", "operator", "or", "private",
        "protected", "public", "register", "requires", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using", "virtual", "void", "volatile", "while", "xor", "main", "NULL"};
    if (keywords.count(out)) out += "_";
    return out;
}

std::string cppStringLiteral(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '?': out += "\\?"; break;   // avoid trigraph surprises
        default:
            if (c < 32 || c == 127) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\%03o", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));   // UTF-8 passes through
            }
        }
    }
    out += "\"";
    return out;
}

std::string cppDoubleLiteral(double value) {
    if (std::isnan(value)) return "std::numeric_limits<double>::quiet_NaN()";
    if (std::isinf(value)) {
        return value < 0 ? "(-std::numeric_limits<double>::infinity())" : "std::numeric_limits<double>::infinity()";
    }
    char buf[64];
    if (std::trunc(value) == value && std::fabs(value) < 1e15) {
        // Whole numbers read best as "200.0" rather than "2e+02".
        std::snprintf(buf, sizeof(buf), "%.1f", value);
        return value < 0 ? "(" + std::string(buf) + ")" : std::string(buf);
    }
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof(buf), "%.*g", precision, value);
        if (std::strtod(buf, nullptr) == value) break;
    }
    std::string text(buf);
    // Make sure it is a floating point literal (has '.' or exponent).
    if (text.find_first_of(".eEn") == std::string::npos) text += ".0";
    if (value < 0) return "(" + text + ")";
    return text;
}

}  // namespace s2c::str
