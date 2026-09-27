#include "scratch/Font.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

namespace scratch {
namespace {

const char* kFontPaths[] = {
#if defined(__APPLE__)
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/Library/Fonts/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/Library/Fonts/Arial.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/AppleSDGothicNeo.ttc",
#elif defined(_WIN32)
    "C:\\Windows\\Fonts\\arialuni.ttf",
    "C:\\Windows\\Fonts\\arial.ttf",
    "C:\\Windows\\Fonts\\segoeui.ttf",
    "C:\\Windows\\Fonts\\msyh.ttc",
    "C:\\Windows\\Fonts\\tahoma.ttf",
#else
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
#endif
    nullptr};

bool readFile(const char* path, std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff n = in.tellg();
    if (n <= 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    in.read(reinterpret_cast<char*>(out.data()), n);
    return static_cast<std::streamoff>(in.gcount()) == n;
}

int nextUtf8(const std::string& s, size_t& i) {
    if (i >= s.size()) return 0;
    const unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80) return c;
    int extra = 0, cp = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else return 0xFFFD;
    for (int k = 0; k < extra && i < s.size(); ++k) {
        const unsigned char n = static_cast<unsigned char>(s[i]);
        if ((n & 0xC0) != 0x80) break;
        ++i;
        cp = (cp << 6) | (n & 0x3F);
    }
    return cp;
}

void blend(unsigned char* pixels, int w, int h, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b,
           std::uint8_t a) {
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

struct Engine {
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool ok = false;

    bool load() {
        for (int i = 0; kFontPaths[i]; ++i) {
            if (!readFile(kFontPaths[i], bytes)) continue;
            const int offset = stbtt_GetFontOffsetForIndex(bytes.data(), 0);
            if (offset < 0) continue;
            if (stbtt_InitFont(&info, bytes.data(), offset)) {
                ok = true;
                SDL_Log("Font: loaded %s", kFontPaths[i]);
                return true;
            }
        }
        SDL_Log("Font: no system TTF found; costume/monitor text will be limited");
        return false;
    }

    float scaleFor(float pixelHeight, bool emSquare) const {
        return emSquare ? stbtt_ScaleForMappingEmToPixels(&info, pixelHeight)
                        : stbtt_ScaleForPixelHeight(&info, pixelHeight);
    }

    float measure(const std::string& utf8, float pixelHeight, bool emSquare) {
        if (!ok || utf8.empty()) return 0;
        const float scale = scaleFor(pixelHeight, emSquare);
        float w = 0;
        int adv = 0, lsb = 0;
        size_t i = 0;
        int prev = 0;
        while (i < utf8.size()) {
            const int cp = nextUtf8(utf8, i);
            if (prev) w += scale * stbtt_GetCodepointKernAdvance(&info, prev, cp);
            stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
            float add = scale * adv;
            if (cp == ' ' && add < pixelHeight * 0.38f) add = pixelHeight * 0.42f;
            w += add;
            prev = cp;
        }
        return w;
    }

    void blit(unsigned char* pixels, int width, int height, float x, float y, float pixelHeight,
              const std::string& utf8, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool yIsBaseline,
              bool emSquare) {
        if (!ok || utf8.empty() || !pixels) return;
        const float scale = scaleFor(pixelHeight, emSquare);
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        const float baseline = yIsBaseline ? y : y + ascent * scale;
        float pen = x;
        int adv = 0, lsb = 0;
        size_t i = 0;
        int prev = 0;
        while (i < utf8.size()) {
            const int cp = nextUtf8(utf8, i);
            if (prev) pen += scale * stbtt_GetCodepointKernAdvance(&info, prev, cp);
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &x0, &y0, &x1, &y1);
            const int gw = x1 - x0, gh = y1 - y0;
            if (gw > 0 && gh > 0) {
                std::vector<unsigned char> bmp(static_cast<size_t>(gw) * static_cast<size_t>(gh));
                stbtt_MakeCodepointBitmap(&info, bmp.data(), gw, gh, gw, scale, scale, cp);
                const int destX = static_cast<int>(std::lround(pen)) + x0;
                const int destY = static_cast<int>(std::lround(baseline)) + y0;
                for (int row = 0; row < gh; ++row) {
                    for (int col = 0; col < gw; ++col) {
                        const unsigned char cover = bmp[static_cast<size_t>(row) * gw + col];
                        if (cover) blend(pixels, width, height, destX + col, destY + row, r, g, b, cover);
                    }
                }
            }
            stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
            float add = scale * adv;
            if (cp == ' ' && add < pixelHeight * 0.38f) add = pixelHeight * 0.42f;
            pen += add;
            prev = cp;
        }
    }
};

Engine& engine() {
    static Engine e;
    static bool once = false;
    if (!once) {
        once = true;
        e.load();
    }
    return e;
}

}  // namespace

Font& Font::instance() {
    static Font f;
    return f;
}

Font::Font() { ready_ = engine().ok; }

float Font::measure(const std::string& utf8, float pixelHeight, bool emSquare) {
    return engine().measure(utf8, pixelHeight, emSquare);
}

void Font::blit(unsigned char* pixels, int width, int height, float x, float y, float pixelHeight,
                const std::string& utf8, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    engine().blit(pixels, width, height, x, y, pixelHeight, utf8, r, g, b, false, false);
}

void Font::blitBaseline(unsigned char* pixels, int width, int height, float x, float baselineY, float pixelHeight,
                        const std::string& utf8, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    engine().blit(pixels, width, height, x, baselineY, pixelHeight, utf8, r, g, b, true, true);
}

void Font::draw(SDL_Renderer* renderer, float x, float y, float pixelHeight, const std::string& utf8,
                std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (!renderer || utf8.empty()) return;
    Engine& e = engine();
    if (!e.ok) {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderDebugText(renderer, x, y, utf8.c_str());
        return;
    }
    const float w = std::max(1.0f, e.measure(utf8, pixelHeight, false) + 2.0f);
    const float h = std::max(1.0f, pixelHeight + 4.0f);
    const int iw = std::max(1, static_cast<int>(std::ceil(w)));
    const int ih = std::max(1, static_cast<int>(std::ceil(h)));
    std::vector<unsigned char> rgba(static_cast<size_t>(iw) * ih * 4, 0);
    e.blit(rgba.data(), iw, ih, 0, 0, pixelHeight, utf8, r, g, b, false, false);
    if (a != 255) {
        for (size_t i = 3; i < rgba.size(); i += 4) {
            rgba[i] = static_cast<unsigned char>(rgba[i] * (a / 255.0f));
        }
    }
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, iw, ih);
    if (!tex) return;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(tex, nullptr, rgba.data(), iw * 4);
    SDL_FRect dst{x, y, static_cast<float>(iw), static_cast<float>(ih)};
    SDL_RenderTexture(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

}  // namespace scratch
