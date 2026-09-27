#include "scratch/Runtime.hpp"

#include "scratch/Font.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace scratch {

namespace {
std::chrono::steady_clock::time_point startTime() {
    static const auto start = std::chrono::steady_clock::now();
    return start;
}
}  // namespace

Runtime::Runtime(RuntimeConfig config) : config_(std::move(config)) {
    startTime();
    if (!config_.gpuAccel) {
        if (const char* env = std::getenv("SCRATCH_GPU")) {
            if (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' || env[0] == 'Y') {
                config_.gpuAccel = true;
            }
        }
    }
}

Runtime::~Runtime() {
    threads_.clear();
    sprites_.clear();
    stage_.reset();
    audio_.shutdown();
    pen_.shutdown();
    renderer_.shutdown();
    SDL_Quit();
}

double Runtime::now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime()).count();
}

// ---- Setup ---------------------------------------------------------------------

bool Runtime::initialise() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!renderer_.init(config_.title, config_.stageWidth, config_.stageHeight, config_.windowScale)) {
        return false;
    }
    pen_.resize(config_.stageWidth, config_.stageHeight);
    audio_.init();   // audio is optional

    if (config_.assetsDir.empty()) {
        // Look next to the executable first (build output), then the source
        // tree location baked in at compile time, then the working directory.
        std::vector<std::string> candidates;
        if (const char* base = SDL_GetBasePath()) candidates.push_back(std::string(base) + "assets/");
#ifdef SCRATCH_ASSETS_DIR
        candidates.push_back(std::string(SCRATCH_ASSETS_DIR) + "/");
#endif
        candidates.push_back("assets/");
        for (const std::string& c : candidates) {
            if (std::filesystem::is_directory(c)) {
                config_.assetsDir = c;
                break;
            }
        }
        if (config_.assetsDir.empty()) {
            SDL_Log("Warning: assets directory not found; costumes will not render");
            config_.assetsDir = "assets/";
        }
    }
    return true;
}

void Runtime::loadAssets(Target& target) {
    for (CostumeInfo& c : target.costumes()) {
        c.rendered = renderer_.loadCostume(config_.assetsDir + c.file, c);
    }
    for (SoundInfo& s : target.sounds()) {
        s.data = audio_.load(config_.assetsDir + s.file, s.format);
    }
}

// ---- Main loop -------------------------------------------------------------------

int Runtime::run() {
    if (!stage_) {
        SDL_Log("No stage registered");
        return 1;
    }
    if (!initialise()) return 1;
    loadAssets(*stage_);
    for (auto& s : sprites_) loadAssets(*s);

    running_ = true;
    resetTimer();
    if (config_.autoStart) greenFlag();

    const double frameSeconds = 1.0 / std::max(1.0, config_.fps);
    int frame = 0;
    while (running_) {
        const double frameStart = now();
        injectScriptedInputs(frame);
        processEvents();
        if (!running_) break;
        stepThreads();
        audio_.update();
        const bool lastFrame = config_.exitAfterFrames > 0 && frame + 1 >= config_.exitAfterFrames;
        render(lastFrame && !config_.screenshotPath.empty());

        ++frame;
        if (lastFrame) {
            running_ = false;
            break;
        }
        const double elapsed = now() - frameStart;
        if (elapsed < frameSeconds) {
            SDL_DelayNS(static_cast<Uint64>((frameSeconds - elapsed) * 1e9));
        }
    }
    stopAll();
    cleanupThreads();
    return 0;
}

void Runtime::injectScriptedInputs(int frame) {
    for (const ScriptedInput& in : config_.scriptedInputs) {
        if (in.kind == ScriptedInput::Kind::KeyPress) {
            // Key down at `frame`, key up one frame later.
            if (in.frame != frame && in.frame + 1 != frame) continue;
            SDL_Keycode key = SDL_GetKeyFromName(in.key.c_str());
            if (in.key == "space") key = SDLK_SPACE;
            else if (in.key == "left arrow") key = SDLK_LEFT;
            else if (in.key == "right arrow") key = SDLK_RIGHT;
            else if (in.key == "up arrow") key = SDLK_UP;
            else if (in.key == "down arrow") key = SDLK_DOWN;
            else if (in.key == "enter") key = SDLK_RETURN;
            if (key == SDLK_UNKNOWN) continue;
            SDL_Event e{};
            e.type = in.frame == frame ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            e.key.key = key;
            e.key.down = in.frame == frame;
            SDL_PushEvent(&e);
        } else {
            if (in.frame != frame && in.frame + 1 != frame) continue;
            float wx = 0, wy = 0;
            SDL_RenderCoordinatesToWindow(renderer_.sdl(), static_cast<float>(config_.stageWidth / 2.0 + in.x),
                                          static_cast<float>(config_.stageHeight / 2.0 - in.y), &wx, &wy);
            SDL_Event e{};
            e.type = in.frame == frame ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.down = in.frame == frame;
            e.button.x = wx;
            e.button.y = wy;
            SDL_PushEvent(&e);
        }
    }
}

void Runtime::saveScreenshot(const std::string& path) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer_.sdl(), nullptr);
    if (!surface) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        return;
    }
    if (!SDL_SaveBMP(surface, path.c_str())) {
        SDL_Log("Could not write screenshot %s: %s", path.c_str(), SDL_GetError());
    } else {
        SDL_Log("Screenshot written to %s", path.c_str());
    }
    SDL_DestroySurface(surface);
}

void Runtime::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_KEY_DOWN: {
            input_.keyDown(event.key.key);
            const std::string name = Input::scratchKeyName(event.key.key);
            startHats(Thread::Hat::KeyPressed, name);
            startHats(Thread::Hat::KeyPressed, "any");
            break;
        }
        case SDL_EVENT_KEY_UP:
            input_.keyUp(event.key.key);
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            double sx = 0, sy = 0;
            renderer_.windowToStage(event.motion.x, event.motion.y, sx, sy);
            input_.setMouse(sx, sy);
            if (draggingMonitor_ >= 0) handleMonitorPointer(sx, sy, false, true);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            double sx = 0, sy = 0;
            renderer_.windowToStage(event.button.x, event.button.y, sx, sy);
            input_.setMouse(sx, sy);
            input_.setMouseDown(true);
            if (!handleMonitorPointer(sx, sy, true, false)) handleClick(sx, sy);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            input_.setMouseDown(false);
            draggingMonitor_ = -1;
            break;
        default:
            break;
        }
    }
}

void Runtime::handleClick(double stageX, double stageY) {
    if (Sprite* s = topSpriteAt(stageX, stageY)) {
        startHats(Thread::Hat::SpriteClicked, "", s);
    } else {
        startHats(Thread::Hat::StageClicked, "", stage_.get());
    }
}

void Runtime::checkEdgeTriggeredHats() {
    auto check = [this](Target& target) {
        for (const HatBinding& b : target.hats()) {
            if (!b.predicate) continue;
            const bool value = b.predicate(target);
            const bool wasTrue = edgeState_.count(&b) > 0;
            if (value && !wasTrue) {
                startHats(Thread::Hat::TimerGreaterThan, "", &target);
                edgeState_.insert(&b);
            } else if (!value && wasTrue) {
                edgeState_.erase(&b);
            }
        }
    };
    check(*stage_);
    for (size_t i = 0; i < sprites_.size(); ++i) check(*sprites_[i]);
}

void Runtime::stepThreads() {
    redrawRequested_ = false;
    // scratch-vm's Sequencer works for at most 75% of the frame.
    const double budget = 0.75 / std::max(1.0, config_.fps);
    const double start = now();
    do {
        tickTime_ = now();   // one timer sample per pass (see Runtime::timer)
        for (size_t i = 0; i < threads_.size(); ++i) {
            std::shared_ptr<Thread> t = threads_[i];   // copy: list may grow during step
            if (!t->active()) continue;
            currentThread_ = t.get();
            t->step();
        }
        currentThread_ = nullptr;
        cleanupThreads();
        // Edge-triggered hats ("when timer > ...") are evaluated after the
        // scripts of this pass, as scratch-vm does; new threads run next pass.
        checkEdgeTriggeredHats();
        bool anyActive = false;
        for (const auto& t : threads_) {
            if (t->active()) { anyActive = true; break; }
        }
        if (!anyActive) break;
    } while (!redrawRequested_ && running_ && now() - start < budget);
}

void Runtime::cleanupThreads() {
    threads_.erase(std::remove_if(threads_.begin(), threads_.end(),
                                  [](const std::shared_ptr<Thread>& t) { return !t->active(); }),
                   threads_.end());
    // Deleted clones are freed once none of their threads can run again.
    for (auto it = sprites_.begin(); it != sprites_.end();) {
        if ((*it)->deleted()) {
            Sprite* s = it->get();
            for (const HatBinding& b : s->hats()) edgeState_.erase(&b);
            it = sprites_.erase(it);
            --cloneCount_;
        } else {
            ++it;
        }
    }
}

void Runtime::render(bool screenshot) {
    renderer_.beginFrame();
    renderer_.drawStage(*stage_);
    pen_.render(renderer_.sdl());
    for (const auto& s : sprites_) renderer_.drawSprite(*s);
    for (const auto& s : sprites_) renderer_.drawBubble(*s);
    drawMonitors();
    // Pixels must be read back before presenting.
    if (screenshot) saveScreenshot(config_.screenshotPath);
    renderer_.present();
}

// ---- Threads / events ------------------------------------------------------------

std::vector<std::shared_ptr<Thread>> Runtime::startHats(Thread::Hat hat, const std::string& option, Target* only) {
    std::vector<std::shared_ptr<Thread>> started;
    auto startFor = [&](Target& target) {
        for (const HatBinding& b : target.hats()) {
            if (b.hat != hat) continue;
            if (hat == Thread::Hat::KeyPressed || hat == Thread::Hat::BroadcastReceived ||
                hat == Thread::Hat::BackdropSwitched) {
                if (b.option != option) continue;
            }
            // Scratch restarts a script that is already running for this hat.
            for (auto& existing : threads_) {
                if (existing->target == &target && existing->binding == &b && existing->active()) {
                    existing->stopped = true;
                }
            }
            auto t = std::make_shared<Thread>();
            t->runtime = this;
            t->target = &target;
            t->hat = hat;
            t->option = option;
            t->binding = &b;
            t->task = b.factory(target, *t);
            if (!t->task.valid()) continue;
            t->task.handle().promise().thread = t.get();
            t->leaf = t->task.handle();
            threads_.push_back(t);
            started.push_back(t);
        }
    };
    if (only) {
        startFor(*only);
    } else {
        startFor(*stage_);
        // Iterate by index: starting scripts never adds sprites, but be safe.
        for (size_t i = 0; i < sprites_.size(); ++i) {
            if (!sprites_[i]->deleted()) startFor(*sprites_[i]);
        }
    }
    return started;
}

void Runtime::greenFlag() {
    stopAll();
    resetTimer();
    edgeState_.clear();
    startHats(Thread::Hat::GreenFlag);
}

void Runtime::stopAll() {
    for (auto& t : threads_) t->stopped = true;
    for (auto& s : sprites_) {
        if (s->isClone()) s->deleted_ = true;
        s->bubble_ = SpeechBubble{};
        s->clearEffects();
    }
    stage_->clearEffects();
    audio_.stopAll();
    requestRedraw();
}

void Runtime::stopOtherScripts(Target* target, Thread* except) {
    for (auto& t : threads_) {
        if (t->target == target && t.get() != except) t->stopped = true;
    }
}

void Runtime::stopThreadsOf(Target* target) {
    for (auto& t : threads_) {
        if (t->target == target) t->stopped = true;
    }
}

// ---- Sprites / clones / layers ------------------------------------------------------

Sprite* Runtime::findSprite(const std::string& name) const {
    for (const auto& s : sprites_) {
        if (!s->isClone() && !s->deleted() && s->name() == name) return s.get();
    }
    return nullptr;
}

std::vector<Sprite*> Runtime::spritesNamed(const std::string& name) const {
    std::vector<Sprite*> out;
    for (const auto& s : sprites_) {
        if (!s->deleted() && s->name() == name) out.push_back(s.get());
    }
    return out;
}

Sprite* Runtime::topSpriteAt(double x, double y) const {
    for (auto it = sprites_.rbegin(); it != sprites_.rend(); ++it) {
        Sprite* s = it->get();
        if (s->visible() && !s->deleted() && s->containsPoint(x, y)) return s;
    }
    return nullptr;
}

void Runtime::sceneColorAt(double x, double y, const Sprite* exclude, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
    for (auto it = sprites_.rbegin(); it != sprites_.rend(); ++it) {
        const Sprite* s = it->get();
        if (s == exclude || !s->visible() || s->deleted()) continue;
        if (s->colorAt(x, y, r, g, b)) return;
    }
    if (pen_.colorAt(x, y, r, g, b)) return;
    // Backdrop: drawn with its rotation centre at the stage centre.
    const CostumeInfo* info = stage_->currentCostume();
    const Costume* c = info ? info->rendered.get() : nullptr;
    if (c && c->valid()) {
        const int tx = static_cast<int>(std::floor(c->centerX + x * c->scale));
        const int ty = static_cast<int>(std::floor(c->centerY - y * c->scale));
        if (c->colorAt(tx, ty, r, g, b)) return;
    }
    r = g = b = 255;
}

Sprite* Runtime::createClone(Sprite& original) {
    if (cloneCount_ >= config_.maxClones) return nullptr;
    std::unique_ptr<Sprite> clone = original.cloneInstance();
    clone->isClone_ = true;
    clone->deleted_ = false;
    clone->bubble_ = SpeechBubble{};
    Sprite* ptr = clone.get();
    // Clones are placed directly behind the sprite they were cloned from.
    auto pos = std::find_if(sprites_.begin(), sprites_.end(),
                            [&](const std::unique_ptr<Sprite>& s) { return s.get() == &original; });
    sprites_.insert(pos, std::move(clone));
    ++cloneCount_;
    loadAssets(*ptr);   // costumes are cached, so this only wires pointers
    startHats(Thread::Hat::StartAsClone, "", ptr);
    requestRedraw();
    return ptr;
}

void Runtime::deleteClone(Sprite& clone) {
    if (!clone.isClone()) return;
    clone.deleted_ = true;
    stopThreadsOf(&clone);
    requestRedraw();
}

void Runtime::moveToFront(Sprite& sprite) {
    auto it = std::find_if(sprites_.begin(), sprites_.end(),
                           [&](const std::unique_ptr<Sprite>& s) { return s.get() == &sprite; });
    if (it == sprites_.end()) return;
    std::rotate(it, it + 1, sprites_.end());
    requestRedraw();
}

void Runtime::moveToBack(Sprite& sprite) {
    auto it = std::find_if(sprites_.begin(), sprites_.end(),
                           [&](const std::unique_ptr<Sprite>& s) { return s.get() == &sprite; });
    if (it == sprites_.end()) return;
    std::rotate(sprites_.begin(), it, it + 1);
    requestRedraw();
}

void Runtime::moveLayers(Sprite& sprite, int delta) {
    auto it = std::find_if(sprites_.begin(), sprites_.end(),
                           [&](const std::unique_ptr<Sprite>& s) { return s.get() == &sprite; });
    if (it == sprites_.end() || delta == 0) return;
    const int index = static_cast<int>(it - sprites_.begin());
    const int target = std::clamp(index + delta, 0, static_cast<int>(sprites_.size()) - 1);
    if (target == index) return;
    if (target > index) {
        std::rotate(it, it + 1, sprites_.begin() + target + 1);
    } else {
        std::rotate(sprites_.begin() + target, it, it + 1);
    }
    requestRedraw();
}

// ---- Monitors / sliders -----------------------------------------------------------

void Runtime::addMonitor(Monitor monitor) { monitors_.push_back(std::move(monitor)); }

Target* Runtime::monitorTarget(const Monitor& monitor) const {
    if (monitor.targetName.empty()) return stage_.get();
    if (Sprite* s = findSprite(monitor.targetName)) return s;
    return stage_.get();
}

const Value* Runtime::findVariable(const std::string& name, const std::string& owner) const {
    if (!owner.empty()) {
        if (Sprite* s = findSprite(owner)) {
            if (const Value* v = s->variableByName(name)) return v;
        }
    }
    if (stage_) {
        if (const Value* v = stage_->variableByName(name)) return v;
    }
    for (const auto& s : sprites_) {
        if (const Value* v = s->variableByName(name)) return v;
    }
    return nullptr;
}

const List* Runtime::findList(const std::string& name, const std::string& owner) const {
    if (!owner.empty()) {
        if (Sprite* s = findSprite(owner)) {
            if (const List* list = s->listByName(name)) return list;
        }
    }
    if (stage_) {
        if (const List* list = stage_->listByName(name)) return list;
    }
    for (const auto& s : sprites_) {
        if (const List* list = s->listByName(name)) return list;
    }
    return nullptr;
}

void Runtime::inferSliderRange(Monitor& monitor, const Value& value) {
    const double n = value.toNumber();
    if (std::fabs(n) > 0 && std::fabs(n) < 3.2 && std::fabs(n - std::round(n)) > 1e-6) {
        monitor.sliderMin = 0;
        monitor.sliderMax = 3.141592653589793;
        monitor.discrete = false;
        return;
    }
    if (n > monitor.sliderMax) {
        monitor.sliderMax = std::max(monitor.sliderMax, std::ceil(n / 10.0) * 10.0);
    }
    if (n < monitor.sliderMin) monitor.sliderMin = std::min(0.0, std::floor(n));
    monitor.discrete = std::fabs(n - std::round(n)) < 1e-6;
}

Value Runtime::monitorValue(const Monitor& monitor) const {
    if (monitor.kind == Monitor::Kind::List) {
        if (const List* list = findList(monitor.name, monitor.targetName)) return Value(list->contents());
        return Value(std::string());
    }
    if (const Value* v = findVariable(monitor.name, monitor.targetName)) return *v;
    return {};
}

void Runtime::setMonitorValue(Monitor& monitor, double value) {
    if (Value* v = const_cast<Value*>(findVariable(monitor.name, monitor.targetName))) {
        *v = Value(value);
        requestRedraw();
    }
}

void Runtime::setMonitorVisible(Monitor::Kind kind, const std::string& name, const std::string& owner, bool visible) {
    bool found = false;
    for (Monitor& m : monitors_) {
        if (m.kind != kind || m.name != name) continue;
        if (!m.targetName.empty() && !owner.empty() && m.targetName != owner) continue;
        m.visible = visible;
        found = true;
    }
    if (!found && visible) {
        Monitor m;
        m.kind = kind;
        m.mode = kind == Monitor::Kind::List ? Monitor::Mode::ListBox : Monitor::Mode::Slider;
        m.name = name;
        m.targetName = owner;
        m.visible = true;
        m.width = kind == Monitor::Kind::List ? 140 : 168;
        m.height = kind == Monitor::Kind::List ? 90 : 44;
        m.x = 8;
        m.y = 8 + static_cast<double>(monitors_.size()) * (m.height + 6.0);
        if (kind == Monitor::Kind::Variable) inferSliderRange(m, monitorValue(m));
        monitors_.push_back(std::move(m));
    }
    requestRedraw();
}

void Runtime::applySlider(Monitor& monitor, double stageX) {
    const float screenX = static_cast<float>(config_.stageWidth / 2.0 + stageX);
    const float trackX = static_cast<float>(monitor.x) + 8.0f;
    const float trackW = static_cast<float>(monitor.width > 0 ? monitor.width : 140.0) - 16.0f;
    if (trackW <= 0) return;
    double t = (screenX - trackX) / trackW;
    t = std::clamp(t, 0.0, 1.0);
    double lo = monitor.sliderMin, hi = monitor.sliderMax;
    if (hi <= lo) {
        inferSliderRange(monitor, monitorValue(monitor));
        lo = monitor.sliderMin;
        hi = monitor.sliderMax;
        if (hi <= lo) hi = lo + 1;
    }
    double value = lo + t * (hi - lo);
    if (monitor.discrete) value = std::round(value);
    setMonitorValue(monitor, value);
}

float Runtime::monitorWidth(const Monitor& m) const {
    if (m.width > 0) return static_cast<float>(m.width);
    if (m.mode == Monitor::Mode::Slider) return 168.0f;
    if (m.mode == Monitor::Mode::ListBox || m.kind == Monitor::Kind::List) return 140.0f;
    return 100.0f;
}

float Runtime::monitorHeight(const Monitor& m) const {
    if (m.height > 0) return static_cast<float>(m.height);
    if (m.mode == Monitor::Mode::Slider) return 44.0f;
    if (m.mode == Monitor::Mode::ListBox || m.kind == Monitor::Kind::List) return 90.0f;
    if (m.mode == Monitor::Mode::Large) return 22.0f;
    return 20.0f;
}

bool Runtime::handleMonitorPointer(double stageX, double stageY, bool down, bool motion) {
    const float mx = static_cast<float>(config_.stageWidth / 2.0 + stageX);
    const float my = static_cast<float>(config_.stageHeight / 2.0 - stageY);
    if (motion && draggingMonitor_ >= 0 && draggingMonitor_ < static_cast<int>(monitors_.size())) {
        applySlider(monitors_[static_cast<size_t>(draggingMonitor_)], stageX);
        return true;
    }
    if (!down) return false;
    for (int i = static_cast<int>(monitors_.size()) - 1; i >= 0; --i) {
        Monitor& m = monitors_[static_cast<size_t>(i)];
        if (!m.visible) continue;
        const float w = monitorWidth(m);
        const float h = monitorHeight(m);
        if (mx < m.x || my < m.y || mx > m.x + w || my > m.y + h) continue;
        if (m.mode == Monitor::Mode::Slider) {
            draggingMonitor_ = i;
            applySlider(m, stageX);
        }
        return true;
    }
    return false;
}

void Runtime::drawMonitors() {
    SDL_Renderer* r = renderer_.sdl();
    if (!r) return;
    Font& font = Font::instance();
    constexpr float kLabelPx = 11.0f;
    constexpr float kLargePx = 16.0f;
    constexpr float kLine = 13.0f;
    auto fill = [&](float x, float y, float w, float h, int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, static_cast<Uint8>(cr), static_cast<Uint8>(cg), static_cast<Uint8>(cb),
                               static_cast<Uint8>(ca));
        SDL_FRect box{x, y, w, h};
        SDL_RenderFillRect(r, &box);
    };
    auto text = [&](float x, float y, const std::string& s, float px = kLabelPx) {
        font.draw(r, x, y, px, s, 255, 255, 255);
    };
    for (const Monitor& m : monitors_) {
        if (!m.visible) continue;
        const bool list = m.kind == Monitor::Kind::List || m.mode == Monitor::Mode::ListBox;
        const int orangeR = list ? 207 : 255;
        const int orangeG = list ? 99 : 140;
        const int orangeB = list ? 207 : 26;
        const Value value = monitorValue(m);
        const std::string valueText = value.toString();
        float w = static_cast<float>(m.width > 0 ? m.width : 0);
        float h = static_cast<float>(m.height > 0 ? m.height : 0);
        const float x = static_cast<float>(m.x);
        const float y = static_cast<float>(m.y);

        if (m.mode == Monitor::Mode::Large) {
            if (w <= 0) w = std::max(24.0f, font.measure(valueText, kLargePx) + 10.0f);
            if (h <= 0) h = 24.0f;
            fill(x, y, w, h, orangeR, orangeG, orangeB);
            text(x + 4, y + 4, valueText, kLargePx);
            continue;
        }

        if (m.mode == Monitor::Mode::ListBox || list) {
            if (w <= 0) w = 110.0f;
            if (h <= 0) h = 80.0f;
            fill(x, y, w, h, orangeR, orangeG, orangeB);
            text(x + 4, y + 2, m.name);
            Target* t = monitorTarget(m);
            List* lst = t ? t->listByName(m.name) : nullptr;
            const int rows = std::max(0, static_cast<int>((h - 16) / kLine));
            const size_t n = lst ? lst->length() : 0;
            for (int i = 0; i < rows && static_cast<size_t>(i) < n; ++i) {
                const std::string line = std::to_string(i + 1) + ". " + lst->items()[static_cast<size_t>(i)].toString();
                text(x + 4, y + 15.0f + static_cast<float>(i) * kLine, line);
            }
            continue;
        }

        if (w <= 0) w = monitorWidth(m);
        if (h <= 0) h = monitorHeight(m);
        fill(x, y, w, h, orangeR, orangeG, orangeB);
        const std::string label = m.name + ": " + valueText;
        text(x + 4, y + 3, label);
        if (m.mode == Monitor::Mode::Slider) {
            const float trackX = x + 8, trackY = y + 22, trackW = w - 16, trackH = 6;
            fill(trackX, trackY, trackW, trackH, 80, 50, 20);
            double lo = m.sliderMin, hi = m.sliderMax;
            if (hi < lo) std::swap(lo, hi);
            const double span = hi - lo;
            double t = span == 0 ? 0 : (value.toNumber() - lo) / span;
            t = std::clamp(t, 0.0, 1.0);
            const float knobX = trackX + static_cast<float>(t) * trackW - 4.0f;
            fill(knobX, trackY - 3, 8, 12, 255, 255, 255);
        }
    }
}

// ---- Diagnostics ------------------------------------------------------------------

void Runtime::warnUnsupported(const std::string& what) {
    if (warned_.insert(what).second) {
        SDL_Log("Warning: unsupported Scratch feature used at runtime: %s", what.c_str());
    }
}

}  // namespace scratch
