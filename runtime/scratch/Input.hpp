// Input.hpp - keyboard and mouse state in Scratch terms, fed by SDL3 events.
#pragma once

#include <SDL3/SDL.h>

#include <set>
#include <string>

#include "scratch/Value.hpp"

namespace scratch {

class Input {
public:
    // Convert an SDL key to the name Scratch uses ("space", "left arrow", "a"...).
    static std::string scratchKeyName(SDL_Keycode key);
    // Normalise a "key pressed?" argument the way scratch-vm does
    // (numbers become characters, single letters become lower-case, ...).
    static std::string normaliseKeyArgument(const Value& key);

    void keyDown(SDL_Keycode key) { keys_.insert(scratchKeyName(key)); }
    void keyUp(SDL_Keycode key) { keys_.erase(scratchKeyName(key)); }
    void setMouse(double stageX, double stageY) { mouseX_ = stageX; mouseY_ = stageY; }
    void setMouseDown(bool down) { mouseDown_ = down; }

    bool isKeyPressed(const Value& key) const;
    bool isKeyNamePressed(const std::string& name) const { return keys_.count(name) > 0; }
    bool anyKeyPressed() const { return !keys_.empty(); }
    bool mouseDown() const { return mouseDown_; }
    double mouseX() const { return mouseX_; }
    double mouseY() const { return mouseY_; }

private:
    std::set<std::string> keys_;
    bool mouseDown_ = false;
    double mouseX_ = 0.0;
    double mouseY_ = 0.0;
};

}  // namespace scratch
