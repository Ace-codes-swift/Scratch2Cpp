// Runtime.hpp - the Scratch virtual machine: main loop, thread scheduling,
// events, clones and layers.
//
// The scheduler mirrors scratch-vm's Sequencer: every frame (default 30 FPS)
// all active threads are stepped in order until they yield. If no thread
// requested a redraw and there is time left in the frame, the threads are
// stepped again, which is how Scratch runs "tight" loops faster than 30 Hz.
#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "scratch/Audio.hpp"
#include "scratch/Coroutine.hpp"
#include "scratch/Input.hpp"
#include "scratch/Pen.hpp"
#include "scratch/Renderer.hpp"
#include "scratch/Sprite.hpp"
#include "scratch/Stage.hpp"

namespace scratch {

// Input injected at a given frame - used for automated testing/debugging.
struct ScriptedInput {
    enum class Kind { KeyPress, Click };
    Kind kind = Kind::KeyPress;
    int frame = 0;
    std::string key;              // Scratch key name, e.g. "space", "right arrow", "a"
    double x = 0.0, y = 0.0;      // stage coordinates for clicks
};

// Stage watcher (variable / list / slider). Coordinates are Scratch monitor
// pixels: origin at the top-left of the 480×360 stage.
struct Monitor {
    enum class Kind { Variable, List };
    enum class Mode { Default, Large, Slider, ListBox };
    Kind kind = Kind::Variable;
    Mode mode = Mode::Default;
    std::string name;
    std::string targetName;        // empty = stage
    double x = 5;
    double y = 5;
    double width = 0;
    double height = 0;
    bool visible = false;
    double sliderMin = 0;
    double sliderMax = 100;
    bool discrete = true;
};

struct RuntimeConfig {
    std::string title = "Scratch Project";
    int stageWidth = 480;
    int stageHeight = 360;
    int windowScale = 2;          // window = stage * scale pixels
    double fps = 30.0;
    std::string assetsDir;        // resolved at start-up when empty
    bool autoStart = true;        // fire the green flag immediately
    bool gpuAccel = false;        // optional GPU / threaded mesh renderer (--gpu)
    int maxClones = 300;
    // Debug / testing aids (all off by default).
    int exitAfterFrames = 0;      // quit after N frames (0 = run until closed)
    std::string screenshotPath;   // write a BMP of the last frame here
    std::vector<ScriptedInput> scriptedInputs;
};

class Runtime {
public:
    explicit Runtime(RuntimeConfig config);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    // ---- Project setup (generated code) ----------------------------------
    template <class T, class... Args>
    T& createStage(Args&&... args) {
        auto stage = std::make_unique<T>(*this, std::forward<Args>(args)...);
        T& ref = *stage;
        stage_ = std::move(stage);
        return ref;
    }
    template <class T, class... Args>
    T& createSprite(Args&&... args) {
        auto sprite = std::make_unique<T>(*this, std::forward<Args>(args)...);
        T& ref = *sprite;
        sprites_.push_back(std::move(sprite));
        return ref;
    }

    // Runs the project until the window is closed. Returns the exit code.
    int run();

    // ---- Accessors ---------------------------------------------------------
    const RuntimeConfig& config() const { return config_; }
    Stage& stage() { return *stage_; }
    const Stage& stage() const { return *stage_; }
    Input& input() { return input_; }
    const Input& input() const { return input_; }
    Renderer& renderer() { return renderer_; }
    PenLayer& penLayer() { return pen_; }
    const PenLayer& penLayer() const { return pen_; }
    Audio& audio() { return audio_; }
    // Sprites in layer order (back to front), including clones.
    const std::vector<std::unique_ptr<Sprite>>& sprites() const { return sprites_; }
    Sprite* findSprite(const std::string& name) const;        // original only
    std::vector<Sprite*> spritesNamed(const std::string& name) const;   // incl. clones
    Sprite* topSpriteAt(double x, double y) const;
    void addMonitor(Monitor monitor);
    void setMonitorVisible(Monitor::Kind kind, const std::string& name, const std::string& owner, bool visible);
    // Colour visible at a stage point (top-most sprite pixel, else backdrop,
    // else white), ignoring `exclude`. Used by "touching color?".
    void sceneColorAt(double x, double y, const Sprite* exclude, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const;

    // ---- Threads / events ---------------------------------------------------
    std::vector<std::shared_ptr<Thread>> startHats(Thread::Hat hat, const std::string& option = "",
                                                   Target* only = nullptr);
    void greenFlag();
    void stopAll();
    void stopOtherScripts(Target* target, Thread* except);
    void stopThreadsOf(Target* target);
    void requestRedraw() { redrawRequested_ = true; }
    Thread* currentThread() const { return currentThread_; }

    // ---- Clones / layers ---------------------------------------------------
    Sprite* createClone(Sprite& original);
    void deleteClone(Sprite& clone);
    void moveToFront(Sprite& sprite);
    void moveToBack(Sprite& sprite);
    void moveLayers(Sprite& sprite, int delta);

    // ---- Time ----------------------------------------------------------------
    static double now();                 // seconds since start (monotonic)
    // Project timer. Like scratch-vm (whose clock is coarse relative to one
    // scheduler pass) every thread stepped in the same pass sees the same
    // value; this keeps idioms such as "when timer > (var set to timer every
    // frame)" behaving as they do in Scratch.
    double timer() const { return tickTime_ - timerStart_; }
    void resetTimer() { tickTime_ = now(); timerStart_ = tickTime_; }

    // ---- Diagnostics -----------------------------------------------------------
    void warnUnsupported(const std::string& what);
    const std::string& assetsDir() const { return config_.assetsDir; }

private:
    bool initialise();
    void loadAssets(Target& target);
    void processEvents();
    void stepThreads();
    void checkEdgeTriggeredHats();
    void cleanupThreads();
    void render(bool screenshot = false);
    void handleClick(double stageX, double stageY);
    void injectScriptedInputs(int frame);
    void saveScreenshot(const std::string& path);
    void drawMonitors();
    bool handleMonitorPointer(double stageX, double stageY, bool down, bool motion);
    void applySlider(Monitor& monitor, double stageX);
    Target* monitorTarget(const Monitor& monitor) const;
    Value monitorValue(const Monitor& monitor) const;
    void setMonitorValue(Monitor& monitor, double value);
    const Value* findVariable(const std::string& name, const std::string& owner) const;
    const List* findList(const std::string& name, const std::string& owner) const;
    void inferSliderRange(Monitor& monitor, const Value& value);
    float monitorWidth(const Monitor& monitor) const;
    float monitorHeight(const Monitor& monitor) const;

    RuntimeConfig config_;
    std::unique_ptr<Stage> stage_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::vector<std::shared_ptr<Thread>> threads_;
    Input input_;
    Renderer renderer_;
    Audio audio_;
    PenLayer pen_;
    Thread* currentThread_ = nullptr;
    bool redrawRequested_ = false;
    bool running_ = false;
    double timerStart_ = 0.0;
    double tickTime_ = 0.0;
    int cloneCount_ = 0;
    std::set<std::string> warned_;
    std::set<const HatBinding*> edgeState_;   // edge-triggered hats currently "true"
    std::vector<Monitor> monitors_;
    int draggingMonitor_ = -1;
};

}  // namespace scratch
