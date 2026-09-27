#include "scratch/Input.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace scratch {

std::string Input::scratchKeyName(SDL_Keycode key) {
    switch (key) {
    case SDLK_SPACE: return "space";
    case SDLK_LEFT: return "left arrow";
    case SDLK_RIGHT: return "right arrow";
    case SDLK_UP: return "up arrow";
    case SDLK_DOWN: return "down arrow";
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return "enter";
    case SDLK_BACKSPACE: return "backspace";
    case SDLK_DELETE: return "delete";
    case SDLK_TAB: return "tab";
    case SDLK_ESCAPE: return "escape";
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return "shift";
    case SDLK_LCTRL:
    case SDLK_RCTRL: return "control";
    case SDLK_LALT:
    case SDLK_RALT: return "alt";
    case SDLK_INSERT: return "insert";
    case SDLK_HOME: return "home";
    case SDLK_END: return "end";
    case SDLK_PAGEUP: return "page up";
    case SDLK_PAGEDOWN: return "page down";
    default: break;
    }
    if (key >= SDLK_KP_0 && key <= SDLK_KP_9) {
        return std::string(1, static_cast<char>('0' + (key - SDLK_KP_0)));
    }
    if (key < 128 && std::isprint(static_cast<int>(key))) {
        return std::string(1, static_cast<char>(std::tolower(static_cast<int>(key))));
    }
    std::string name = SDL_GetKeyName(key);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

std::string Input::normaliseKeyArgument(const Value& key) {
    // Mirrors scratch-vm's Keyboard._keyStringToScratchKey.
    if (key.isNumber()) {
        const double n = key.toNumber();
        // Numbers 48..90 are treated as key codes for '0'..'9' and 'A'..'Z'.
        if (n >= 48 && n <= 90 && std::floor(n) == n) {
            return std::string(1, static_cast<char>(std::tolower(static_cast<int>(n))));
        }
    }
    std::string s = key.toString();
    if (s.size() == 1) {
        return std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(s[0]))));
    }
    // Multi-character names ("space", "left arrow", "Enter"...) are lower-cased.
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (s == " ") return "space";
    if (s == "arrowleft") return "left arrow";
    if (s == "arrowright") return "right arrow";
    if (s == "arrowup") return "up arrow";
    if (s == "arrowdown") return "down arrow";
    return s;
}

bool Input::isKeyPressed(const Value& key) const {
    const std::string name = normaliseKeyArgument(key);
    if (name == "any") return anyKeyPressed();
    return isKeyNamePressed(name);
}

}  // namespace scratch
