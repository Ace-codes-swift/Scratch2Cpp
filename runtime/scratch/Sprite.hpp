// Sprite.hpp - a Scratch sprite (or clone) with Scratch's motion/looks model.
//
// Coordinates are Scratch stage coordinates: x grows to the right, y grows
// upwards, (0, 0) is the centre of the stage. Direction 90 points right and
// 0 points up, increasing clockwise. The Renderer converts to SDL space.
#pragma once

#include <memory>
#include <string>

#include "scratch/Target.hpp"

namespace scratch {

enum class RotationStyle { AllAround, LeftRight, DontRotate };

struct Rect {
    double left = 0, right = 0, bottom = 0, top = 0;   // stage coordinates
    double width() const { return right - left; }
    double height() const { return top - bottom; }
    bool intersects(const Rect& o) const {
        return left <= o.right && right >= o.left && bottom <= o.top && top >= o.bottom;
    }
};

struct SpeechBubble {
    std::string text;
    bool think = false;
    bool visible = false;
    int token = 0;   // identifies the "say for secs" that created the bubble
};

class Sprite : public Target {
public:
    Sprite(Runtime& runtime, std::string name);

    bool isStage() const override { return false; }

    // Generated subclasses implement this so clones get the right scripts.
    virtual std::unique_ptr<Sprite> cloneInstance() const = 0;

    // ---- Initial state (generated code) ----------------------------------
    void setPosition(double x, double y) { x_ = x; y_ = y; }
    void setDirectionRaw(double d) { direction_ = d; }
    void setSizeRaw(double s) { size_ = s; }
    void setVisible(bool v) { visible_ = v; }
    void setRotationStyle(RotationStyle s) { rotationStyle_ = s; }
    void setDraggable(bool d) { draggable_ = d; }

    // ---- Motion -----------------------------------------------------------
    void moveSteps(double steps);
    void turnRight(double degrees);
    void turnLeft(double degrees);
    void goToXY(double x, double y);
    void goTo(const Value& target);              // "_random_", "_mouse_" or sprite name
    Task glideTo(double seconds, double x, double y);
    Task glideToTarget(double seconds, const Value& target);
    void pointInDirection(double direction);
    void pointTowards(const Value& target);
    void changeX(double dx) { goToXY(x_ + dx, y_); }
    void setX(double x) { goToXY(x, y_); }
    void changeY(double dy) { goToXY(x_, y_ + dy); }
    void setY(double y) { goToXY(x_, y); }
    void ifOnEdgeBounce();
    void setRotationStyle(const Value& style);
    double x() const { return x_; }
    double y() const { return y_; }
    double direction() const { return direction_; }

    // ---- Looks ------------------------------------------------------------
    void say(const Value& text);
    Task sayForSecs(const Value& text, double seconds);
    void think(const Value& text);
    Task thinkForSecs(const Value& text, double seconds);
    void switchCostumeTo(const Value& selector);
    void nextCostume();
    void changeSizeBy(double delta) { setSize(size_ + delta); }
    void setSize(double size);
    void show();
    void hide();
    void goToFrontLayer();
    void goToBackLayer();
    void goForwardLayers(int n);
    void goBackwardLayers(int n);
    double size() const { return size_; }
    bool visible() const { return visible_; }
    RotationStyle rotationStyle() const { return rotationStyle_; }
    const SpeechBubble& bubble() const { return bubble_; }

    // ---- Control ----------------------------------------------------------
    void deleteThisClone();
    bool isClone() const { return isClone_; }
    bool deleted() const { return deleted_; }
    void stampCostume();

    // ---- Sensing ----------------------------------------------------------
    bool touching(const Value& target) const;     // "_mouse_", "_edge_" or sprite name
    bool touchingColor(const Value& color) const;
    bool colorIsTouchingColor(const Value& own, const Value& other) const;
    double distanceTo(const Value& target) const;

    // ---- Geometry (used by renderer/sensing) ------------------------------
    // Axis aligned bounds of the current costume in stage coordinates.
    Rect bounds() const;
    // True if the given stage point hits an opaque pixel of the costume.
    bool containsPoint(double px, double py) const;
    // Colour of the costume pixel at a stage point (false if transparent).
    bool colorAt(double px, double py, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const;
    bool touchingSprite(const Sprite& other) const;
    // Rotation applied on screen (clockwise degrees) and horizontal flip.
    double renderAngle() const;
    bool renderFlipped() const;

private:
    friend class Runtime;
    // Maps a stage point to costume texture coordinates; false if no costume.
    bool stageToTexture(double px, double py, int& tx, int& ty) const;
    // Calls fn(px, py) for stage points covered by opaque costume pixels
    // until fn returns true. Returns whether fn ever returned true.
    template <class Fn>
    bool forEachOpaquePoint(Fn fn) const;
    void keepInFence();
    void requestRedrawIfVisible() const;
    bool resolveTargetPoint(const Value& target, double& outX, double& outY) const;
    void setBubble(const Value& text, bool think);

    double x_ = 0.0;
    double y_ = 0.0;
    double direction_ = 90.0;
    double size_ = 100.0;
    bool visible_ = true;
    bool draggable_ = false;
    RotationStyle rotationStyle_ = RotationStyle::AllAround;
    SpeechBubble bubble_;
    bool isClone_ = false;
    bool deleted_ = false;
    int bubbleCounter_ = 0;
};

}  // namespace scratch
