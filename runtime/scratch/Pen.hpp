// Pen.hpp - Scratch pen layer (between the backdrop and the sprites).
//
// The canvas is a stage-resolution RGBA buffer. Sprites with the pen down
// stroke a line on every move; stamp copies the current costume. Colour
// parameters follow scratch-vm (hue/saturation/brightness/transparency 0–100).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace scratch {

class Value;

struct PenStyle {
    bool down = false;
    std::uint8_t r = 0, g = 0, b = 255, a = 255;
    double diameter = 1.0;
    double hue = 200.0 / 3.0;   // Scratch "color" for default blue
    double saturation = 100.0;
    double brightness = 100.0;
    double transparency = 0.0;

    void applyRgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
    void setParam(const std::string& name, double value, bool change);
    void setSize(double size);
    void changeSize(double delta);
};

class PenLayer {
public:
    PenLayer() = default;
    ~PenLayer();
    PenLayer(const PenLayer&) = delete;
    PenLayer& operator=(const PenLayer&) = delete;

    void resize(int stageWidth, int stageHeight);
    void clear();
    void shutdown();

    void drawLine(double x0, double y0, double x1, double y1, const PenStyle& style);
    void fillStageBox(double x0, double y0, double x1, double y1, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                      std::uint8_t a);
    void blendStage(double sx, double sy, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
    bool colorAt(double sx, double sy, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const;

    void render(SDL_Renderer* renderer);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    void stageToPixel(double sx, double sy, int& px, int& py) const;
    void blendPixel(int px, int py, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
    void drawDisk(double sx, double sy, const PenStyle& style);
    void upload(SDL_Renderer* renderer);

    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;   // RGBA, row-major
    SDL_Texture* texture_ = nullptr;
    bool dirty_ = false;
};

void parseScratchColor(const Value& color, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b);

}  // namespace scratch
