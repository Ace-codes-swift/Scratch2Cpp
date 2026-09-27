// TrueType text for costume overlays, monitors and speech bubbles.
#pragma once

#include <cstdint>
#include <string>

struct SDL_Renderer;

namespace scratch {

struct Font {
    static Font& instance();

    bool ready() const { return ready_; }
    float measure(const std::string& utf8, float pixelHeight, bool emSquare = false);
    // Top-left draw into an RGBA costume/buffer.
    void blit(unsigned char* pixels, int width, int height, float x, float y, float pixelHeight,
              const std::string& utf8, std::uint8_t r, std::uint8_t g, std::uint8_t b);
    // SVG-style: `baselineY` is the alphabetic baseline; size is the em square.
    void blitBaseline(unsigned char* pixels, int width, int height, float x, float baselineY, float pixelHeight,
                      const std::string& utf8, std::uint8_t r, std::uint8_t g, std::uint8_t b);
    void draw(SDL_Renderer* renderer, float x, float y, float pixelHeight, const std::string& utf8,
              std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

private:
    Font();
    bool ready_ = false;
};

}  // namespace scratch
