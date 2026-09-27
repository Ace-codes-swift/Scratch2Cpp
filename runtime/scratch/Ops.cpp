#include "scratch/Ops.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

namespace scratch::ops {

namespace {

std::mt19937_64& rng() {
    static std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

double randomUnit() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng());
}

// Decode UTF-8 into code points so "letter of" / "length of" count characters
// the way Scratch (mostly) does rather than counting bytes.
std::vector<std::uint32_t> decodeUtf8(const std::string& s) {
    std::vector<std::uint32_t> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::uint32_t cp = c;
        size_t extra = 0;
        if (c >= 0xF0) { cp = c & 0x07u; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0Fu; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1Fu; extra = 1; }
        ++i;
        for (size_t k = 0; k < extra && i < s.size(); ++k, ++i) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3Fu);
        }
        out.push_back(cp);
    }
    return out;
}

std::string encodeUtf8(std::uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// scratch-vm rounds trig results to 10 decimals to hide floating point noise.
double fixed10(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10f", v);
    return std::strtod(buf, nullptr);
}

}  // namespace

double add(double a, double b) { return a + b; }
double sub(double a, double b) { return a - b; }
double mul(double a, double b) { return a * b; }

double div(double a, double b) {
    if (b == 0.0) {
        if (a == 0.0 || std::isnan(a)) return std::numeric_limits<double>::quiet_NaN();
        const bool negative = (a < 0) != std::signbit(b);
        return negative ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();
    }
    return a / b;
}

double mod(double a, double b) {
    double result = std::fmod(a, b);
    if (std::isnan(result)) return result;
    // Scratch: the result has the sign of the divisor.
    if (result / b < 0) result += b;
    return result;
}

double random(const Value& from, const Value& to) {
    double low = from.toNumber();
    double high = to.toNumber();
    if (low == high) return low;
    if (low > high) std::swap(low, high);
    if (from.isInt() && to.isInt()) {
        return low + std::floor(randomUnit() * (high - low + 1));
    }
    return randomUnit() * (high - low) + low;
}

double round(double a) {
    // JavaScript Math.round: halves round toward +Infinity.
    return std::floor(a + 0.5);
}

double mathop(const std::string& op, double n) {
    const double pi = 3.14159265358979323846;
    if (op == "abs") return std::fabs(n);
    if (op == "floor") return std::floor(n);
    if (op == "ceiling") return std::ceil(n);
    if (op == "sqrt") return std::sqrt(n);
    if (op == "sin") return fixed10(std::sin(pi * n / 180.0));
    if (op == "cos") return fixed10(std::cos(pi * n / 180.0));
    if (op == "tan") {
        const double m = mod(n, 360.0);
        if (m == 90.0) return std::numeric_limits<double>::infinity();
        if (m == 270.0) return -std::numeric_limits<double>::infinity();
        return fixed10(std::tan(pi * n / 180.0));
    }
    if (op == "asin") return std::asin(n) * 180.0 / pi;
    if (op == "acos") return std::acos(n) * 180.0 / pi;
    if (op == "atan") return std::atan(n) * 180.0 / pi;
    if (op == "ln") return std::log(n);
    if (op == "log") return std::log10(n);
    if (op == "e ^") return std::exp(n);
    if (op == "10 ^") return std::pow(10.0, n);
    return 0.0;
}

bool lt(const Value& a, const Value& b) { return Value::compare(a, b) < 0; }
bool gt(const Value& a, const Value& b) { return Value::compare(a, b) > 0; }
bool eq(const Value& a, const Value& b) { return Value::compare(a, b) == 0; }

std::string join(const Value& a, const Value& b) { return a.toString() + b.toString(); }

std::string letterOf(const Value& index, const Value& text) {
    const double idx = index.toNumber();
    const std::vector<std::uint32_t> cps = decodeUtf8(text.toString());
    if (idx < 1 || idx > static_cast<double>(cps.size())) return "";
    return encodeUtf8(cps[static_cast<size_t>(idx) - 1]);
}

double length(const Value& text) {
    return static_cast<double>(decodeUtf8(text.toString()).size());
}

bool contains(const Value& text, const Value& needle) {
    return toLower(text.toString()).find(toLower(needle.toString())) != std::string::npos;
}

}  // namespace scratch::ops
