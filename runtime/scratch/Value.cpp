#include "scratch/Value.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace scratch {

namespace {

std::string_view trimView(std::string_view s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

// Validates that `s` is a JavaScript StrDecimalLiteral (optionally signed).
bool isDecimalLiteral(std::string_view s) {
    size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    if (s.substr(i) == "Infinity") return true;
    size_t digits = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; ++digits; }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; ++digits; }
    }
    if (digits == 0) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        size_t expDigits = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; ++expDigits; }
        if (expDigits == 0) return false;
    }
    return i == s.size();
}

std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

double Value::parseNumber(std::string_view text) {
    const std::string_view s = trimView(text);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (s.empty()) return 0.0;  // Number("") === 0

    // Hex / octal / binary integer literals (no sign allowed in JS).
    if (s.size() > 2 && s[0] == '0') {
        int base = 0;
        const char p = static_cast<char>(std::tolower(static_cast<unsigned char>(s[1])));
        if (p == 'x') base = 16; else if (p == 'o') base = 8; else if (p == 'b') base = 2;
        if (base != 0) {
            const std::string body(s.substr(2));
            char* end = nullptr;
            const unsigned long long v = std::strtoull(body.c_str(), &end, base);
            if (end == body.c_str() || *end != '\0') return nan;
            for (char c : body) if (!std::isalnum(static_cast<unsigned char>(c))) return nan;
            return static_cast<double>(v);
        }
    }

    if (!isDecimalLiteral(s)) return nan;
    const std::string body(s);
    if (body.find("Infinity") != std::string::npos) {
        return body[0] == '-' ? -std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::infinity();
    }
    char* end = nullptr;
    const double v = std::strtod(body.c_str(), &end);
    if (end != body.c_str() + body.size()) return nan;
    return v;
}

std::string Value::formatNumber(double n) {
    if (std::isnan(n)) return "NaN";
    if (std::isinf(n)) return n < 0 ? "-Infinity" : "Infinity";
    if (n == 0.0) return "0";  // covers -0 as well

    // Find the shortest digit string that round-trips (like JS Number#toString).
    char buf[64];
    int precision = 1;
    for (; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof(buf), "%.*e", precision - 1, n);
        if (std::strtod(buf, nullptr) == n) break;
    }
    // buf looks like "-d.ddddde+XX"
    std::string text(buf);
    bool negative = false;
    if (text[0] == '-') { negative = true; text.erase(0, 1); }
    const size_t ePos = text.find('e');
    std::string mantissa = text.substr(0, ePos);
    const int exponent = std::atoi(text.c_str() + ePos + 1);
    std::string digits;
    for (char c : mantissa) if (c != '.') digits.push_back(c);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();

    const int k = static_cast<int>(digits.size());
    const int pointPos = exponent + 1;  // "n" in the ECMAScript algorithm
    std::string out;
    if (k <= pointPos && pointPos <= 21) {
        out = digits + std::string(static_cast<size_t>(pointPos - k), '0');
    } else if (0 < pointPos && pointPos <= 21) {
        out = digits.substr(0, static_cast<size_t>(pointPos)) + "." + digits.substr(static_cast<size_t>(pointPos));
    } else if (-6 < pointPos && pointPos <= 0) {
        out = "0." + std::string(static_cast<size_t>(-pointPos), '0') + digits;
    } else {
        const int e = pointPos - 1;
        out = digits.substr(0, 1);
        if (k > 1) out += "." + digits.substr(1);
        out += "e";
        out += (e < 0 ? "-" : "+");
        out += std::to_string(std::abs(e));
    }
    return negative ? "-" + out : out;
}

double Value::toNumber() const {
    switch (type_) {
    case Type::Number:
        return std::isnan(number_) ? 0.0 : number_;
    case Type::Bool:
        return boolean_ ? 1.0 : 0.0;
    case Type::String: {
        const double n = parseNumber(string_);
        return std::isnan(n) ? 0.0 : n;
    }
    }
    return 0.0;
}

std::string Value::toString() const {
    switch (type_) {
    case Type::Number: return formatNumber(number_);
    case Type::Bool: return boolean_ ? "true" : "false";
    case Type::String: return string_;
    }
    return {};
}

bool Value::toBool() const {
    switch (type_) {
    case Type::Bool: return boolean_;
    case Type::Number: return number_ != 0.0 && !std::isnan(number_);
    case Type::String: {
        if (string_.empty() || string_ == "0") return false;
        return toLowerCopy(string_) != "false";
    }
    }
    return false;
}

bool Value::isInt() const {
    switch (type_) {
    case Type::Number:
        if (std::isnan(number_)) return true;
        return std::trunc(number_) == number_;
    case Type::Bool: return true;
    case Type::String: return string_.find('.') == std::string::npos;
    }
    return false;
}

bool Value::isWhitespace() const {
    return type_ == Type::String && trimView(string_).empty();
}

double Value::compare(const Value& a, const Value& b) {
    // Number(v) for a string uses JS semantics (NaN on failure); for
    // booleans it is 0/1 and numbers pass through (including NaN).
    auto rawNumber = [](const Value& v) -> double {
        switch (v.type_) {
        case Type::Number: return v.number_;
        case Type::Bool: return v.boolean_ ? 1.0 : 0.0;
        case Type::String: return parseNumber(v.string_);
        }
        return 0.0;
    };
    double n1 = rawNumber(a);
    double n2 = rawNumber(b);
    if (n1 == 0.0 && a.isWhitespace()) {
        n1 = std::numeric_limits<double>::quiet_NaN();
    } else if (n2 == 0.0 && b.isWhitespace()) {
        n2 = std::numeric_limits<double>::quiet_NaN();
    }
    if (std::isnan(n1) || std::isnan(n2)) {
        const std::string s1 = toLowerCopy(a.toString());
        const std::string s2 = toLowerCopy(b.toString());
        if (s1 < s2) return -1.0;
        if (s1 > s2) return 1.0;
        return 0.0;
    }
    if (std::isinf(n1) && std::isinf(n2) && ((n1 > 0) == (n2 > 0))) return 0.0;
    return n1 - n2;
}

}  // namespace scratch
