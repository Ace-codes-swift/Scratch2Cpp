#include "scratch/SvgOverlay.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "scratch/Font.hpp"
#include "third_party/stb_image.h"

namespace scratch {
namespace {

// Public-domain 8x8 ASCII (32–126), row-major, bit 0 = leftmost pixel.
const std::uint8_t kFont[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

struct Xf {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    Xf then(const Xf& o) const {
        return {a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d, b * o.c + d * o.d,
                a * o.e + c * o.f + e, b * o.e + d * o.f + f};
    }
    void apply(double x, double y, double& ox, double& oy) const {
        ox = a * x + c * y + e;
        oy = b * x + d * y + f;
    }
    double scaleAbs() const { return std::max(std::hypot(a, b), std::hypot(c, d)); }
};

bool ieq(const std::string& a, const char* b) {
    if (a.size() != std::strlen(b)) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

std::string unescape(std::string s) {
    auto replace = [&](const char* from, const char* to) {
        size_t pos = 0;
        const size_t n = std::strlen(from);
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, n, to);
            pos += std::strlen(to);
        }
    };
    replace("&amp;", "&");
    replace("&lt;", "<");
    replace("&gt;", ">");
    replace("&quot;", "\"");
    replace("&apos;", "'");
    replace("&#160;", " ");
    return s;
}

void skipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == ',')) ++i;
}

double readNumber(const std::string& s, size_t& i) {
    skipWs(s, i);
    char* end = nullptr;
    const double v = std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) return 0;
    i = static_cast<size_t>(end - s.c_str());
    return v;
}

Xf parseTransform(const std::string& spec) {
    Xf xf;
    size_t i = 0;
    while (i < spec.size()) {
        skipWs(spec, i);
        if (i >= spec.size()) break;
        if (spec.compare(i, 9, "translate") == 0) {
            i += 9;
            skipWs(spec, i);
            if (i < spec.size() && spec[i] == '(') ++i;
            const double tx = readNumber(spec, i);
            skipWs(spec, i);
            double ty = 0;
            if (i < spec.size() && spec[i] != ')') ty = readNumber(spec, i);
            while (i < spec.size() && spec[i] != ')') ++i;
            if (i < spec.size()) ++i;
            xf = xf.then(Xf{1, 0, 0, 1, tx, ty});
        } else if (spec.compare(i, 5, "scale") == 0) {
            i += 5;
            skipWs(spec, i);
            if (i < spec.size() && spec[i] == '(') ++i;
            const double sx = readNumber(spec, i);
            skipWs(spec, i);
            double sy = sx;
            if (i < spec.size() && spec[i] != ')') sy = readNumber(spec, i);
            while (i < spec.size() && spec[i] != ')') ++i;
            if (i < spec.size()) ++i;
            xf = xf.then(Xf{sx, 0, 0, sy, 0, 0});
        } else if (spec.compare(i, 6, "matrix") == 0) {
            i += 6;
            skipWs(spec, i);
            if (i < spec.size() && spec[i] == '(') ++i;
            const double a = readNumber(spec, i), b = readNumber(spec, i), c = readNumber(spec, i);
            const double d = readNumber(spec, i), e = readNumber(spec, i), f = readNumber(spec, i);
            while (i < spec.size() && spec[i] != ')') ++i;
            if (i < spec.size()) ++i;
            xf = xf.then(Xf{a, b, c, d, e, f});
        } else if (spec.compare(i, 6, "rotate") == 0) {
            i += 6;
            skipWs(spec, i);
            if (i < spec.size() && spec[i] == '(') ++i;
            const double deg = readNumber(spec, i);
            const double rad = deg * 3.14159265358979323846 / 180.0;
            const double cs = std::cos(rad), sn = std::sin(rad);
            skipWs(spec, i);
            if (i < spec.size() && spec[i] != ')') {
                const double cx = readNumber(spec, i), cy = readNumber(spec, i);
                xf = xf.then(Xf{1, 0, 0, 1, cx, cy}).then(Xf{cs, sn, -sn, cs, 0, 0}).then(Xf{1, 0, 0, 1, -cx, -cy});
            } else {
                xf = xf.then(Xf{cs, sn, -sn, cs, 0, 0});
            }
            while (i < spec.size() && spec[i] != ')') ++i;
            if (i < spec.size()) ++i;
        } else {
            ++i;
        }
    }
    return xf;
}

bool parseColor(const std::string& s, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    std::string t = s;
    if (t.size() >= 4 && (t[0] == 'r' || t[0] == 'R')) return false;
    if (!t.empty() && t[0] == '#') t.erase(0, 1);
    if (t.size() == 3) {
        r = static_cast<std::uint8_t>(std::strtoul(t.substr(0, 1).c_str(), nullptr, 16) * 17);
        g = static_cast<std::uint8_t>(std::strtoul(t.substr(1, 1).c_str(), nullptr, 16) * 17);
        b = static_cast<std::uint8_t>(std::strtoul(t.substr(2, 1).c_str(), nullptr, 16) * 17);
        return true;
    }
    if (t.size() >= 6) {
        r = static_cast<std::uint8_t>(std::strtoul(t.substr(0, 2).c_str(), nullptr, 16));
        g = static_cast<std::uint8_t>(std::strtoul(t.substr(2, 2).c_str(), nullptr, 16));
        b = static_cast<std::uint8_t>(std::strtoul(t.substr(4, 2).c_str(), nullptr, 16));
        return true;
    }
    return false;
}

std::string attr(const std::string& tag, const char* name) {
    const std::string key = std::string(name) + "=";
    size_t pos = 0;
    while ((pos = tag.find(key, pos)) != std::string::npos) {
        if (pos > 0 && (tag[pos - 1] == ' ' || tag[pos - 1] == '\t' || tag[pos - 1] == '\n' || tag[pos - 1] == ':')) {
            size_t i = pos + key.size();
            if (i >= tag.size()) break;
            const char q = tag[i];
            if (q != '"' && q != '\'') {
                ++pos;
                continue;
            }
            const size_t end = tag.find(q, i + 1);
            if (end == std::string::npos) return {};
            return tag.substr(i + 1, end - i - 1);
        }
        ++pos;
    }
    return {};
}

std::string localName(std::string tag) {
    if (tag.empty() || tag[0] != '<') return {};
    size_t i = 1;
    if (i < tag.size() && tag[i] == '/') ++i;
    while (i < tag.size() && (tag[i] == ' ' || tag[i] == '\t')) ++i;
    const size_t start = i;
    while (i < tag.size() && tag[i] != ' ' && tag[i] != '\t' && tag[i] != '>' && tag[i] != '/') ++i;
    std::string name = tag.substr(start, i - start);
    const size_t colon = name.rfind(':');
    if (colon != std::string::npos) name = name.substr(colon + 1);
    return name;
}

void blend(unsigned char* pixels, int w, int h, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (x < 0 || y < 0 || x >= w || y >= h || a == 0) return;
    const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
    const float sA = a / 255.0f;
    const float dA = pixels[i + 3] / 255.0f;
    const float outA = sA + dA * (1.0f - sA);
    if (outA <= 0) return;
    const float keep = dA * (1.0f - sA);
    pixels[i] = static_cast<unsigned char>(std::lround((r * sA + pixels[i] * keep) / outA));
    pixels[i + 1] = static_cast<unsigned char>(std::lround((g * sA + pixels[i + 1] * keep) / outA));
    pixels[i + 2] = static_cast<unsigned char>(std::lround((b * sA + pixels[i + 2] * keep) / outA));
    pixels[i + 3] = static_cast<unsigned char>(std::lround(outA * 255.0f));
}

void blitGlyph(unsigned char* pixels, int w, int h, double px, double py, double cell, char ch,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (ch < 32 || ch > 126 || cell < 0.6) return;
    const std::uint8_t* rows = kFont[ch - 32];
    const double step = cell / 8.0;
    for (int row = 0; row < 8; ++row) {
        const std::uint8_t bits = rows[row];
        const int y0 = static_cast<int>(std::floor(py + row * step));
        const int y1 = static_cast<int>(std::ceil(py + (row + 1) * step));
        for (int col = 0; col < 8; ++col) {
            if ((bits & (1u << col)) == 0) continue;
            const int x0 = static_cast<int>(std::floor(px + col * step));
            const int x1 = static_cast<int>(std::ceil(px + (col + 1) * step));
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    blend(pixels, w, h, x, y, r, g, b, 255);
                }
            }
        }
    }
}

std::vector<std::uint8_t> decodeBase64(const std::string& in) {
    static const int tbl[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    std::vector<std::uint8_t> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || tbl[c] < 0) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + tbl[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

void blitRgba(unsigned char* dest, int dw, int dh, int dx, int dy, const unsigned char* src, int sw, int sh,
              double scaleX, double scaleY) {
    const int outW = std::max(1, static_cast<int>(std::lround(sw * scaleX)));
    const int outH = std::max(1, static_cast<int>(std::lround(sh * scaleY)));
    for (int y = 0; y < outH; ++y) {
        const int sy = std::min(sh - 1, static_cast<int>(y / std::max(0.001, scaleY)));
        for (int x = 0; x < outW; ++x) {
            const int sx = std::min(sw - 1, static_cast<int>(x / std::max(0.001, scaleX)));
            const size_t i = (static_cast<size_t>(sy) * sw + sx) * 4;
            blend(dest, dw, dh, dx + x, dy + y, src[i], src[i + 1], src[i + 2], src[i + 3]);
        }
    }
}

struct ImageBox {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

struct WalkState {
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    double raster = 1, viewMinX = 0, viewMinY = 0;
    std::vector<Xf> stack{Xf{}};
    std::vector<ImageBox> images;
    Xf current() const { return stack.empty() ? Xf{} : stack.back(); }
};

bool textOverlapsImage(const WalkState& st, const Xf& xf, double localX, double localY) {
    if (st.images.empty()) return false;
    double ox = 0, oy = 0;
    xf.apply(localX, localY, ox, oy);
    for (const ImageBox& b : st.images) {
        const double padX = (b.x1 - b.x0) * 0.04;
        const double padY = (b.y1 - b.y0) * 0.04;
        if (ox >= b.x0 - padX && ox <= b.x1 + padX && oy >= b.y0 - padY && oy <= b.y1 + padY) return true;
    }
    return false;
}

double parseLength(const std::string& s, double em) {
    if (s.empty()) return 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return 0;
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    std::string unit;
    while (end && *end && std::isalpha(static_cast<unsigned char>(*end))) {
        unit.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*end))));
        ++end;
    }
    if (unit == "em") return v * (em > 0 ? em : 16.0);
    if (unit == "%") return v / 100.0 * (em > 0 ? em : 16.0);
    return v;
}

void drawTextLine(WalkState& st, const Xf& xf, double localX, double localY, const std::string& text, double fontSize,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& anchor) {
    if (text.empty()) return;
    const double em = fontSize > 0 ? fontSize : 16.0;
    double originX = 0, originY = 0, topX = 0, topY = 0;
    xf.apply(localX, localY, originX, originY);
    xf.apply(localX, localY - em, topX, topY);
    const double emPx = std::hypot(topX - originX, topY - originY) * st.raster;
    float px = static_cast<float>((originX - st.viewMinX) * st.raster);
    float py = static_cast<float>((originY - st.viewMinY) * st.raster);
    Font& font = Font::instance();
    if (font.ready()) {
        // Paper.js CJK UI fonts are a bit tighter than the em square; 0.82 keeps
        // spaces visible and stops the last glyph clipping off the costume.
        float pixelH = static_cast<float>(std::clamp(emPx * 0.70, 4.0, 96.0));
        float pixelW = font.measure(text, pixelH, true);
        const float maxW = std::max(4.0f, static_cast<float>(st.w) - px - 1.0f);
        if (pixelW > maxW) {
            pixelH *= maxW / pixelW;
            pixelW = maxW;
        }
        if (anchor == "middle") px -= pixelW * 0.5f;
        else if (anchor == "end") px -= pixelW;
        font.blitBaseline(st.pixels, st.w, st.h, px, py, pixelH, text, r, g, b);
        return;
    }
    const double cap = std::clamp(emPx * 0.68, 5.0, 36.0);
    const double advance = std::max(3.0, emPx * 0.50);
    const double pixelW = advance * static_cast<double>(text.size());
    if (anchor == "middle") px -= static_cast<float>(pixelW / 2.0);
    else if (anchor == "end") px -= static_cast<float>(pixelW);
    const double top = static_cast<double>(py) - cap;
    for (size_t i = 0; i < text.size(); ++i) {
        blitGlyph(st.pixels, st.w, st.h, static_cast<double>(px) + static_cast<double>(i) * advance, top, cap, text[i],
                  r, g, b);
    }
}

void drawText(WalkState& st, const Xf& xf, const std::string& inner, double fontSize, const std::string& fill,
              const std::string& anchor, double textX, double textY) {
    std::uint8_t r = 0, g = 0, b = 0;
    parseColor(fill.empty() ? "#000000" : fill, r, g, b);
    struct Span {
        std::string text;
        double dy = 0;
        double x = 0;
        bool hasX = false;
    };
    std::vector<Span> spans;
    size_t i = 0;
    bool sawTspan = false;
    while (i < inner.size()) {
        if (inner.compare(i, 6, "<tspan") == 0 || inner.compare(i, 6, "<TSPAN") == 0) {
            sawTspan = true;
            const size_t gt = inner.find('>', i);
            if (gt == std::string::npos) break;
            const std::string tag = inner.substr(i, gt - i + 1);
            const size_t close = inner.find("</", gt + 1);
            if (close == std::string::npos) break;
            Span span;
            span.dy = parseLength(attr(tag, "dy"), fontSize > 0 ? fontSize : 16.0);
            const std::string tx = attr(tag, "x");
            if (!tx.empty()) {
                span.x = parseLength(tx, fontSize > 0 ? fontSize : 16.0);
                span.hasX = true;
            }
            span.text = unescape(inner.substr(gt + 1, close - (gt + 1)));
            spans.push_back(std::move(span));
            const size_t after = inner.find('>', close);
            i = after == std::string::npos ? inner.size() : after + 1;
            continue;
        }
        if (inner[i] == '<') {
            const size_t gt = inner.find('>', i);
            i = gt == std::string::npos ? inner.size() : gt + 1;
            continue;
        }
        ++i;
    }
    if (!sawTspan) {
        Span span;
        std::string plain;
        bool inTag = false;
        for (char c : inner) {
            if (c == '<') inTag = true;
            else if (c == '>') inTag = false;
            else if (!inTag) plain.push_back(c);
        }
        span.text = unescape(plain);
        spans.push_back(std::move(span));
    }
    double y = textY;
    for (const Span& span : spans) {
        y += span.dy;
        drawTextLine(st, xf, span.hasX ? span.x : textX, y, span.text, fontSize, r, g, b, anchor);
    }
}

void drawImage(WalkState& st, const Xf& xf, const std::string& href, double x, double y, double iw, double ih,
               bool blit) {
    const std::string prefix = "data:image/";
    if (href.compare(0, prefix.size(), prefix) != 0) return;
    const size_t comma = href.find(',');
    if (comma == std::string::npos) return;
    const std::vector<std::uint8_t> bytes = decodeBase64(href.substr(comma + 1));
    if (bytes.empty()) return;
    int sw = 0, sh = 0, ch = 0;
    unsigned char* src = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &sw, &sh, &ch, 4);
    if (!src) return;
    // Paper.js often embeds a tiny raster of the same <text>. Skip those; keep photos/logos.
    if (sh <= 32 || sw * sh <= 2048) {
        stbi_image_free(src);
        return;
    }
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    xf.apply(x, y, x0, y0);
    xf.apply(x + (iw > 0 ? iw : sw), y + (ih > 0 ? ih : sh), x1, y1);
    if (!blit) {
        st.images.push_back({std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1)});
        stbi_image_free(src);
        return;
    }
    const int dx = static_cast<int>(std::lround((x0 - st.viewMinX) * st.raster));
    const int dy = static_cast<int>(std::lround((y0 - st.viewMinY) * st.raster));
    const double sx = std::abs(x1 - x0) * st.raster / std::max(1, sw);
    const double sy = std::abs(y1 - y0) * st.raster / std::max(1, sh);
    blitRgba(st.pixels, st.w, st.h, dx, dy, src, sw, sh, sx, sy);
    stbi_image_free(src);
}

}  // namespace

void overlaySvgExtras(const std::string& svg, unsigned char* pixels, int width, int height,
                      double rasterScale, double viewMinX, double viewMinY) {
    if (!pixels || width <= 0 || height <= 0) return;
    WalkState st;
    st.pixels = pixels;
    st.w = width;
    st.h = height;
    st.raster = rasterScale;
    st.viewMinX = viewMinX;
    st.viewMinY = viewMinY;

    auto walk = [&](bool paint) {
        st.stack.assign(1, Xf{});
        size_t i = 0;
        while (i < svg.size()) {
            if (svg[i] != '<') {
                ++i;
                continue;
            }
            if (svg.compare(i, 4, "<!--") == 0) {
                const size_t end = svg.find("-->", i + 4);
                i = end == std::string::npos ? svg.size() : end + 3;
                continue;
            }
            const size_t gt = svg.find('>', i);
            if (gt == std::string::npos) break;
            const std::string tag = svg.substr(i, gt - i + 1);
            const bool closing = tag.size() > 1 && tag[1] == '/';
            const bool selfClose = tag.size() > 2 && tag[tag.size() - 2] == '/';
            const std::string name = localName(tag);
            i = gt + 1;

            if (closing) {
                if (ieq(name, "g") || ieq(name, "svg") || ieq(name, "text") || ieq(name, "a")) {
                    if (st.stack.size() > 1) st.stack.pop_back();
                }
                continue;
            }

            const std::string tf = attr(tag, "transform");
            const Xf next = tf.empty() ? st.current() : st.current().then(parseTransform(tf));
            const bool group = ieq(name, "g") || ieq(name, "svg") || ieq(name, "a") || ieq(name, "text");
            if (group && !selfClose) st.stack.push_back(next);

            if (ieq(name, "text")) {
                const size_t close = svg.find("</text", i);
                std::string inner;
                if (close != std::string::npos) {
                    inner = svg.substr(i, close - i);
                    const size_t after = svg.find('>', close);
                    i = after == std::string::npos ? svg.size() : after + 1;
                }
                if (paint) {
                    const double fontSize = parseLength(attr(tag, "font-size"), 16.0);
                    std::string fill = attr(tag, "fill");
                    if (fill.empty() || fill == "none") fill = "#000000";
                    const double textX = parseLength(attr(tag, "x"), fontSize > 0 ? fontSize : 16.0);
                    const double textY = parseLength(attr(tag, "y"), fontSize > 0 ? fontSize : 16.0);
                    if (!textOverlapsImage(st, next, textX, textY)) {
                        drawText(st, next, inner, fontSize, fill, attr(tag, "text-anchor"), textX, textY);
                    }
                }
                if (!selfClose && st.stack.size() > 1) st.stack.pop_back();
                continue;
            }

            if (ieq(name, "image")) {
                std::string href = attr(tag, "href");
                if (href.empty()) href = attr(tag, "xlink:href");
                const double x = std::strtod(attr(tag, "x").c_str(), nullptr);
                const double y = std::strtod(attr(tag, "y").c_str(), nullptr);
                const double iw = std::strtod(attr(tag, "width").c_str(), nullptr);
                const double ih = std::strtod(attr(tag, "height").c_str(), nullptr);
                drawImage(st, next, href, x, y, iw, ih, paint);
            }

            if (selfClose && group && st.stack.size() > 1) st.stack.pop_back();
        }
    };
    walk(false);
    walk(true);
}

}  // namespace scratch
