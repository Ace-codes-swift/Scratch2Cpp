#include "scratch/Pen.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "scratch/Value.hpp"

namespace scratch {

namespace {

double wrap100(double n) {
    n = std::fmod(n, 100.0);
    if (n < 0) n += 100.0;
    return n;
}

void hsvToRgb(double h, double s, double v, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    // h is Scratch "color" 0–100 (full hue circle). s, v are 0–100.
    h = wrap100(h) / 100.0 * 6.0;
    s = std::clamp(s, 0.0, 100.0) / 100.0;
    v = std::clamp(v, 0.0, 100.0) / 100.0;
    const int i = static_cast<int>(std::floor(h)) % 6;
    const double f = h - std::floor(h);
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - f * s);
    const double t = v * (1.0 - (1.0 - f) * s);
    double rf = 0, gf = 0, bf = 0;
    switch (i) {
    case 0: rf = v; gf = t; bf = p; break;
    case 1: rf = q; gf = v; bf = p; break;
    case 2: rf = p; gf = v; bf = t; break;
    case 3: rf = p; gf = q; bf = v; break;
    case 4: rf = t; gf = p; bf = v; break;
    default: rf = v; gf = p; bf = q; break;
    }
    r = static_cast<std::uint8_t>(std::lround(rf * 255.0));
    g = static_cast<std::uint8_t>(std::lround(gf * 255.0));
    b = static_cast<std::uint8_t>(std::lround(bf * 255.0));
}

void rgbToHsv(std::uint8_t r, std::uint8_t g, std::uint8_t b, double& h, double& s, double& v) {
    const double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
    const double max = std::max({rf, gf, bf});
    const double min = std::min({rf, gf, bf});
    const double d = max - min;
    v = max * 100.0;
    s = max <= 0 ? 0 : d / max * 100.0;
    if (d <= 0) {
        h = 0;
        return;
    }
    double hue = 0;
    if (max == rf) hue = (gf - bf) / d + (gf < bf ? 6.0 : 0.0);
    else if (max == gf) hue = (bf - rf) / d + 2.0;
    else hue = (rf - gf) / d + 4.0;
    h = hue / 6.0 * 100.0;
}

}  // namespace

void parseScratchColor(const Value& color, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    unsigned long rgb = 0;
    if (color.isString()) {
        std::string s = color.toString();
        if (!s.empty() && s[0] == '#') s.erase(0, 1);
        rgb = std::strtoul(s.c_str(), nullptr, 16);
    } else {
        rgb = static_cast<unsigned long>(std::max(0.0, color.toNumber()));
    }
    r = static_cast<std::uint8_t>((rgb >> 16) & 0xFF);
    g = static_cast<std::uint8_t>((rgb >> 8) & 0xFF);
    b = static_cast<std::uint8_t>(rgb & 0xFF);
}

void PenStyle::applyRgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    r = red;
    g = green;
    b = blue;
    rgbToHsv(r, g, b, hue, saturation, brightness);
}

void PenStyle::setParam(const std::string& name, double value, bool change) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key == "color") {
        hue = wrap100(change ? hue + value : value);
    } else if (key == "saturation") {
        saturation = std::clamp(change ? saturation + value : value, 0.0, 100.0);
    } else if (key == "brightness") {
        brightness = std::clamp(change ? brightness + value : value, 0.0, 100.0);
    } else if (key == "transparency") {
        transparency = std::clamp(change ? transparency + value : value, 0.0, 100.0);
        a = static_cast<std::uint8_t>(std::lround((1.0 - transparency / 100.0) * 255.0));
        return;
    } else {
        return;
    }
    hsvToRgb(hue, saturation, brightness, r, g, b);
}

void PenStyle::setSize(double size) {
    if (!std::isfinite(size)) return;
    diameter = std::clamp(size, 1.0, 1200.0);
}

void PenStyle::changeSize(double delta) {
    if (!std::isfinite(delta)) return;
    diameter = std::clamp(diameter + delta, 1.0, 1200.0);
}

PenLayer::~PenLayer() { shutdown(); }

void PenLayer::shutdown() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

void PenLayer::resize(int stageWidth, int stageHeight) {
    width_ = std::max(1, stageWidth);
    height_ = std::max(1, stageHeight);
    pixels_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4, 0);
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    dirty_ = true;
}

void PenLayer::clear() {
    std::fill(pixels_.begin(), pixels_.end(), 0);
    dirty_ = true;
}

void PenLayer::stageToPixel(double sx, double sy, int& px, int& py) const {
    px = static_cast<int>(std::floor(width_ / 2.0 + sx));
    py = static_cast<int>(std::floor(height_ / 2.0 - sy));
}

void PenLayer::blendPixel(int px, int py, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (px < 0 || py < 0 || px >= width_ || py >= height_ || a == 0) return;
    const size_t i = (static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4;
    const float sA = a / 255.0f;
    const float dA = pixels_[i + 3] / 255.0f;
    const float outA = sA + dA * (1.0f - sA);
    if (outA <= 0.0f) return;
    const float keep = dA * (1.0f - sA);
    pixels_[i] = static_cast<std::uint8_t>(std::lround((r * sA + pixels_[i] * keep) / outA));
    pixels_[i + 1] = static_cast<std::uint8_t>(std::lround((g * sA + pixels_[i + 1] * keep) / outA));
    pixels_[i + 2] = static_cast<std::uint8_t>(std::lround((b * sA + pixels_[i + 2] * keep) / outA));
    pixels_[i + 3] = static_cast<std::uint8_t>(std::lround(outA * 255.0f));
    dirty_ = true;
}

void PenLayer::blendStage(double sx, double sy, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    int px = 0, py = 0;
    stageToPixel(sx, sy, px, py);
    blendPixel(px, py, r, g, b, a);
}

bool PenLayer::colorAt(double sx, double sy, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
    int px = 0, py = 0;
    stageToPixel(sx, sy, px, py);
    if (px < 0 || py < 0 || px >= width_ || py >= height_) return false;
    const size_t i = (static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4;
    if (pixels_[i + 3] == 0) return false;
    r = pixels_[i];
    g = pixels_[i + 1];
    b = pixels_[i + 2];
    return true;
}

void PenLayer::drawDisk(double sx, double sy, const PenStyle& style) {
    const double radius = std::max(0.5, style.diameter / 2.0);
    const int minX = static_cast<int>(std::floor(width_ / 2.0 + sx - radius));
    const int maxX = static_cast<int>(std::ceil(width_ / 2.0 + sx + radius));
    const int minY = static_cast<int>(std::floor(height_ / 2.0 - sy - radius));
    const int maxY = static_cast<int>(std::ceil(height_ / 2.0 - sy + radius));
    const double r2 = radius * radius;
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const double cx = (px + 0.5) - (width_ / 2.0);
            const double cy = (height_ / 2.0) - (py + 0.5);
            const double dx = cx - sx;
            const double dy = cy - sy;
            if (dx * dx + dy * dy <= r2) blendPixel(px, py, style.r, style.g, style.b, style.a);
        }
    }
}

void PenLayer::fillStageBox(double x0, double y0, double x1, double y1, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                            std::uint8_t a) {
    if (a == 0 || width_ <= 0) return;
    if (x1 < x0) std::swap(x0, x1);
    if (y1 < y0) std::swap(y0, y1);
    int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
    stageToPixel(x0, y1, px0, py0);   // top-left in pixel space (y up)
    stageToPixel(x1, y0, px1, py1);
    if (px1 < px0) std::swap(px0, px1);
    if (py1 < py0) std::swap(py0, py1);
    px0 = std::clamp(px0, 0, width_ - 1);
    px1 = std::clamp(px1, 0, width_);
    py0 = std::clamp(py0, 0, height_ - 1);
    py1 = std::clamp(py1, 0, height_);
    for (int py = py0; py < py1; ++py) {
        for (int px = px0; px < px1; ++px) blendPixel(px, py, r, g, b, a);
    }
}

void PenLayer::drawLine(double x0, double y0, double x1, double y1, const PenStyle& style) {
    const double dist = std::hypot(x1 - x0, y1 - y0);
    if (!std::isfinite(dist)) return;
    const double step = std::max(0.5, style.diameter * 0.25);
    const int n = dist < 1e-9 ? 0 : static_cast<int>(std::ceil(dist / step));
    for (int i = 0; i <= n; ++i) {
        const double t = n == 0 ? 0.0 : static_cast<double>(i) / n;
        drawDisk(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, style);
    }
}

void PenLayer::upload(SDL_Renderer* renderer) {
    if (!renderer || width_ <= 0) return;
    if (!texture_) {
        texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width_, height_);
        if (!texture_) return;
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    }
    SDL_UpdateTexture(texture_, nullptr, pixels_.data(), width_ * 4);
    dirty_ = false;
}

void PenLayer::render(SDL_Renderer* renderer) {
    if (!renderer || pixels_.empty()) return;
    if (dirty_ || !texture_) upload(renderer);
    if (!texture_) return;
    SDL_RenderTexture(renderer, texture_, nullptr, nullptr);
}

}  // namespace scratch
