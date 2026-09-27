#include "scratch/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "third_party/stb_image.h"

#define NANOSVG_IMPLEMENTATION
#include "third_party/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "third_party/nanosvgrast.h"

#include "scratch/Font.hpp"
#include "scratch/SvgOverlay.hpp"
#include "scratch/Sprite.hpp"
#include "scratch/Stage.hpp"
#include "scratch/Target.hpp"

namespace scratch {

namespace {

// SVG costumes are rasterised at this many texture pixels per stage unit so
// they stay crisp when the window is larger than the stage.
constexpr double kSvgScale = 2.0;
constexpr int kMaxTextureSize = 4096;

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// Extract the viewBox origin so rotation centres (expressed in viewBox
// space by Scratch) can be converted to raster pixels.
void parseViewBoxOrigin(const std::string& svg, double& minX, double& minY) {
    minX = minY = 0.0;
    const size_t pos = svg.find("viewBox");
    if (pos == std::string::npos) return;
    const size_t q = svg.find_first_of("\"'", pos);
    if (q == std::string::npos) return;
    const size_t end = svg.find(svg[q], q + 1);
    if (end == std::string::npos) return;
    std::string body = svg.substr(q + 1, end - q - 1);
    std::replace(body.begin(), body.end(), ',', ' ');
    std::istringstream ss(body);
    double a = 0, b = 0;
    if (ss >> a >> b) {
        minX = a;
        minY = b;
    }
}

}  // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(const std::string& title, int stageWidth, int stageHeight, int windowScale) {
    stageWidth_ = stageWidth;
    stageHeight_ = stageHeight;
    const int scale = std::max(1, windowScale);
    if (!SDL_CreateWindowAndRenderer(title.c_str(), stageWidth * scale, stageHeight * scale,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window_, &renderer_)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }
    // Scratch coordinates map 1:1 to this logical space; SDL letterboxes it.
    SDL_SetRenderLogicalPresentation(renderer_, stageWidth, stageHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    return true;
}

void Renderer::shutdown() {
    for (auto& [path, costume] : cache_) {
        if (costume && costume->texture) SDL_DestroyTexture(costume->texture);
        if (costume) costume->texture = nullptr;
    }
    cache_.clear();
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
}

std::shared_ptr<Costume> Renderer::makeCostume(const unsigned char* rgba, int w, int h) {
    auto costume = std::make_shared<Costume>();
    if (w <= 0 || h <= 0) return costume;
    costume->width = w;
    costume->height = h;
    costume->rgba.assign(rgba, rgba + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    int left = w, top = h, right = -1, bottom = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char a = rgba[(static_cast<size_t>(y) * w + x) * 4 + 3];
            if (a > 0) {
                left = std::min(left, x); right = std::max(right, x);
                top = std::min(top, y); bottom = std::max(bottom, y);
            }
        }
    }
    if (right < 0) {  // fully transparent costume
        left = top = 0; right = bottom = 0;
    }
    costume->opaqueLeft = left; costume->opaqueRight = right;
    costume->opaqueTop = top; costume->opaqueBottom = bottom;

    costume->texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!costume->texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return costume;
    }
    SDL_UpdateTexture(costume->texture, nullptr, rgba, w * 4);
    SDL_SetTextureBlendMode(costume->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(costume->texture, SDL_SCALEMODE_LINEAR);
    return costume;
}

std::shared_ptr<Costume> Renderer::decodeSvg(const std::string& path, const CostumeInfo& info) {
    std::string text;
    if (!readFile(path, text)) {
        SDL_Log("Could not read costume %s", path.c_str());
        return std::make_shared<Costume>();
    }
    double viewMinX = 0, viewMinY = 0;
    parseViewBoxOrigin(text, viewMinX, viewMinY);

    std::string mutableCopy = text;   // nsvgParse modifies the buffer
    NSVGimage* image = nsvgParse(mutableCopy.data(), "px", 96.0f);
    if (!image) {
        SDL_Log("Could not parse SVG costume %s", path.c_str());
        return std::make_shared<Costume>();
    }
    double scale = kSvgScale;
    const double maxDim = std::max(image->width, image->height);
    if (maxDim * scale > kMaxTextureSize && maxDim > 0) scale = kMaxTextureSize / maxDim;
    const int w = static_cast<int>(std::ceil(image->width * scale));
    const int h = static_cast<int>(std::ceil(image->height * scale));
    std::shared_ptr<Costume> costume;
    if (w > 0 && h > 0) {
        std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4, 0);
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        nsvgRasterize(rast, image, 0.0f, 0.0f, static_cast<float>(scale), pixels.data(), w, h, w * 4);
        nsvgDeleteRasterizer(rast);
        // nanosvg ignores <text> and raster <image>; paint those onto the same buffer.
        overlaySvgExtras(text, pixels.data(), w, h, scale, viewMinX, viewMinY);
        costume = makeCostume(pixels.data(), w, h);
    } else {
        costume = std::make_shared<Costume>();
    }
    nsvgDelete(image);
    costume->scale = scale;
    costume->centerX = (info.rotationCenterX - viewMinX) * scale;
    costume->centerY = (info.rotationCenterY - viewMinY) * scale;
    return costume;
}

std::shared_ptr<Costume> Renderer::decodeBitmap(const std::string& path, const CostumeInfo& info) {
    std::string bytes;
    if (!readFile(path, bytes)) {
        SDL_Log("Could not read costume %s", path.c_str());
        return std::make_shared<Costume>();
    }
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()),
                                                  static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!pixels) {
        SDL_Log("Could not decode costume %s: %s", path.c_str(), stbi_failure_reason());
        return std::make_shared<Costume>();
    }
    std::shared_ptr<Costume> costume = makeCostume(pixels, w, h);
    stbi_image_free(pixels);
    costume->scale = info.bitmapResolution > 0 ? info.bitmapResolution : 1.0;
    costume->centerX = info.rotationCenterX;
    costume->centerY = info.rotationCenterY;
    return costume;
}

std::shared_ptr<Costume> Renderer::loadCostume(const std::string& path, const CostumeInfo& info) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;
    std::shared_ptr<Costume> costume;
    if (info.format == "svg") {
        costume = decodeSvg(path, info);
    } else {
        costume = decodeBitmap(path, info);
    }
    cache_[path] = costume;
    return costume;
}

void Renderer::beginFrame() {
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderClear(renderer_);
}

void Renderer::drawStage(const Stage& stage) {
    const CostumeInfo* info = stage.currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (!c || !c->valid()) return;
    const double k = 1.0 / c->scale;
    SDL_FRect dst{static_cast<float>(stageWidth_ / 2.0 - c->centerX * k),
                  static_cast<float>(stageHeight_ / 2.0 - c->centerY * k),
                  static_cast<float>(c->width * k), static_cast<float>(c->height * k)};
    const double ghost = std::clamp(stage.effect("ghost"), 0.0, 100.0);
    SDL_SetTextureAlphaModFloat(c->texture, static_cast<float>(1.0 - ghost / 100.0));
    SDL_RenderTexture(renderer_, c->texture, nullptr, &dst);
}

void Renderer::drawSprite(const Sprite& sprite) {
    if (!sprite.visible()) return;
    const CostumeInfo* info = sprite.currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (!c || !c->valid()) return;

    const double k = (sprite.size() / 100.0) / c->scale;   // stage units per texture pixel
    const bool flip = sprite.renderFlipped();
    const double centerX = flip ? (c->width - c->centerX) : c->centerX;
    const double screenX = stageWidth_ / 2.0 + sprite.x();
    const double screenY = stageHeight_ / 2.0 - sprite.y();

    SDL_FRect dst{static_cast<float>(screenX - centerX * k), static_cast<float>(screenY - c->centerY * k),
                  static_cast<float>(c->width * k), static_cast<float>(c->height * k)};
    SDL_FPoint center{static_cast<float>(centerX * k), static_cast<float>(c->centerY * k)};

    // Graphic effects: "ghost" maps to alpha, negative "brightness" darkens.
    const double ghost = std::clamp(sprite.effect("ghost"), 0.0, 100.0);
    SDL_SetTextureAlphaModFloat(c->texture, static_cast<float>(1.0 - ghost / 100.0));
    const double brightness = std::clamp(sprite.effect("brightness"), -100.0, 100.0);
    const float mod = brightness < 0 ? static_cast<float>(1.0 + brightness / 100.0) : 1.0f;
    SDL_SetTextureColorModFloat(c->texture, mod, mod, mod);

    SDL_RenderTextureRotated(renderer_, c->texture, nullptr, &dst, sprite.renderAngle(), &center,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorModFloat(c->texture, 1.0f, 1.0f, 1.0f);
    SDL_SetTextureAlphaModFloat(c->texture, 1.0f);
}

void Renderer::drawBubble(const Sprite& sprite) {
    const SpeechBubble& bubble = sprite.bubble();
    if (!bubble.visible || !sprite.visible() || bubble.text.empty()) return;

    Font& font = Font::instance();
    constexpr float kPx = 12.0f;
    constexpr float kLine = 15.0f;
    constexpr float kMaxW = 168.0f;
    std::vector<std::string> lines;
    {
        std::istringstream words(bubble.text);
        std::string word, line;
        auto widerThan = [&](const std::string& s) { return font.measure(s, kPx) > kMaxW; };
        while (words >> word) {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && widerThan(candidate)) {
                lines.push_back(line);
                line = word;
            } else {
                line = std::move(candidate);
            }
            while (widerThan(line) && line.size() > 1) {
                size_t cut = line.size() / 2;
                lines.push_back(line.substr(0, cut));
                line = line.substr(cut);
            }
        }
        if (!line.empty()) lines.push_back(line);
        if (lines.empty()) lines.push_back(bubble.text);
    }
    float textW = 1.0f;
    for (const auto& l : lines) textW = std::max(textW, font.measure(l, kPx));
    const float pad = 6.0f;
    const float w = textW + pad * 2;
    const float h = static_cast<float>(lines.size()) * kLine + pad * 2;

    // Place the bubble above the sprite's top-right corner, clamped to stage.
    const Rect b = sprite.bounds();
    float x = static_cast<float>(stageWidth_ / 2.0 + b.right) - 8.0f;
    float y = static_cast<float>(stageHeight_ / 2.0 - b.top) - h - 8.0f;
    x = std::clamp(x, 0.0f, static_cast<float>(stageWidth_) - w);
    y = std::clamp(y, 0.0f, static_cast<float>(stageHeight_) - h);

    SDL_FRect box{x, y, w, h};
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 180, 180, 180, 255);
    SDL_RenderRect(renderer_, &box);
    if (bubble.think) {
        // Little trail of dots for "think" bubbles.
        for (int i = 0; i < 3; ++i) {
            const float s = 4.0f - i;
            SDL_FRect dot{x + 8.0f - i * 5.0f, y + h + 2.0f + i * 5.0f, s, s};
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer_, &dot);
            SDL_SetRenderDrawColor(renderer_, 180, 180, 180, 255);
            SDL_RenderRect(renderer_, &dot);
        }
    } else {
        SDL_SetRenderDrawColor(renderer_, 180, 180, 180, 255);
        SDL_RenderLine(renderer_, x + 10.0f, y + h, x + 4.0f, y + h + 8.0f);
        SDL_RenderLine(renderer_, x + 4.0f, y + h + 8.0f, x + 18.0f, y + h);
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        font.draw(renderer_, x + pad, y + pad + static_cast<float>(i) * kLine, kPx, lines[i], 40, 40, 40);
    }
}

void Renderer::present() { SDL_RenderPresent(renderer_); }

void Renderer::windowToStage(float wx, float wy, double& sx, double& sy) const {
    float lx = 0, ly = 0;
    SDL_RenderCoordinatesFromWindow(renderer_, wx, wy, &lx, &ly);
    sx = lx - stageWidth_ / 2.0;
    sy = stageHeight_ / 2.0 - ly;
}

}  // namespace scratch
