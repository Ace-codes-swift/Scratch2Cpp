// Renderer.hpp - SDL3 rendering of the stage, sprites and speech bubbles.
//
// The stage is rendered in a fixed logical resolution (480x360 by default)
// and SDL's logical presentation letterboxes it into the window, so Scratch
// coordinates map 1:1 onto logical render coordinates:
//     renderX = stageWidth / 2 + x
//     renderY = stageHeight / 2 - y
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace scratch {

class Sprite;
class Stage;
struct CostumeInfo;

// A costume decoded into an SDL texture plus an alpha mask for hit-testing.
struct Costume {
    SDL_Texture* texture = nullptr;
    int width = 0;                 // texture pixels
    int height = 0;
    double scale = 1.0;            // texture pixels per stage unit (at size 100%)
    double centerX = 0.0;          // rotation centre in texture pixels
    double centerY = 0.0;
    std::vector<std::uint8_t> rgba;    // width*height*4 pixels (for hit tests / colour sensing)
    // Bounding box (inclusive, texture pixels) of the non-transparent pixels.
    int opaqueLeft = 0, opaqueTop = 0, opaqueRight = 0, opaqueBottom = 0;
    bool valid() const { return texture != nullptr && width > 0 && height > 0; }
    bool inside(int px, int py) const { return px >= 0 && py >= 0 && px < width && py < height; }
    std::uint8_t alphaAt(int px, int py) const {
        if (!inside(px, py)) return 0;
        return rgba[(static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * 4 + 3];
    }
    // Returns false for transparent pixels.
    bool colorAt(int px, int py, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
        if (!inside(px, py)) return false;
        const size_t i = (static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * 4;
        if (rgba[i + 3] == 0) return false;
        r = rgba[i]; g = rgba[i + 1]; b = rgba[i + 2];
        return true;
    }
};

// Scratch compares colours with reduced precision (scratch-render colorMatches).
inline bool colorMatches(std::uint8_t r1, std::uint8_t g1, std::uint8_t b1, std::uint8_t r2, std::uint8_t g2, std::uint8_t b2) {
    return (r1 & 0xF8) == (r2 & 0xF8) && (g1 & 0xF8) == (g2 & 0xF8) && (b1 & 0xF0) == (b2 & 0xF0);
}

class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(const std::string& title, int stageWidth, int stageHeight, int windowScale);
    void shutdown();

    // Loads (and caches) a costume file from the assets directory.
    std::shared_ptr<Costume> loadCostume(const std::string& path, const CostumeInfo& info);

    void beginFrame();
    void drawStage(const Stage& stage);
    void drawSprite(const Sprite& sprite);
    void drawBubble(const Sprite& sprite);
    void present();

    // Converts window pixel coordinates to Scratch stage coordinates.
    void windowToStage(float wx, float wy, double& sx, double& sy) const;

    SDL_Window* window() const { return window_; }
    SDL_Renderer* sdl() const { return renderer_; }
    int stageWidth() const { return stageWidth_; }
    int stageHeight() const { return stageHeight_; }

private:
    std::shared_ptr<Costume> decodeSvg(const std::string& path, const CostumeInfo& info);
    std::shared_ptr<Costume> decodeBitmap(const std::string& path, const CostumeInfo& info);
    std::shared_ptr<Costume> makeCostume(const unsigned char* rgba, int w, int h);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int stageWidth_ = 480;
    int stageHeight_ = 360;
    std::map<std::string, std::shared_ptr<Costume>> cache_;
};

}  // namespace scratch
