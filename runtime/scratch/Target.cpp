#include "scratch/Target.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <random>

#include "scratch/Runtime.hpp"
#include "scratch/Sprite.hpp"
#include "scratch/Stage.hpp"

namespace scratch {

namespace {

// MathUtil.wrapClamp from scratch-vm.
double wrapClamp(double n, double min, double max) {
    const double range = (max - min) + 1;
    return n - std::floor((n - min) / range) * range;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

Target::Target(Runtime& runtime, std::string name) : runtime_(&runtime), name_(std::move(name)) {}
Target::~Target() = default;

Stage& Target::stage() const { return runtime_->stage(); }

void Target::addVariable(const std::string& name, Value initial) {
    variableNames_.push_back(name);
    variables_.push_back(std::move(initial));
}

void Target::addList(const std::string& name, std::vector<Value> initial) {
    listNames_.push_back(name);
    lists_.emplace_back(std::move(initial));
}

void Target::addScript(Thread::Hat hat, const std::string& option, ScriptFactory factory) {
    hats_.push_back(HatBinding{hat, option, std::move(factory), nullptr});
}

void Target::addEdgeScript(std::function<bool(Target&)> predicate, ScriptFactory factory) {
    hats_.push_back(HatBinding{Thread::Hat::TimerGreaterThan, "", std::move(factory), std::move(predicate)});
}

const Value* Target::variableByName(const std::string& name) const {
    for (size_t i = 0; i < variableNames_.size(); ++i) {
        if (variableNames_[i] == name) return &variables_[i];
    }
    return nullptr;
}
Value* Target::variableByName(const std::string& name) {
    return const_cast<Value*>(static_cast<const Target*>(this)->variableByName(name));
}

const List* Target::listByName(const std::string& name) const {
    for (size_t i = 0; i < listNames_.size(); ++i) {
        if (listNames_[i] == name) return &lists_[i];
    }
    return nullptr;
}
List* Target::listByName(const std::string& name) {
    return const_cast<List*>(static_cast<const Target*>(this)->listByName(name));
}

// ---- Costumes ---------------------------------------------------------------

const CostumeInfo* Target::currentCostume() const {
    if (costumes_.empty()) return nullptr;
    return &costumes_[static_cast<size_t>(std::clamp<int>(costume_, 0, static_cast<int>(costumes_.size()) - 1))];
}

Value Target::costumeName() const {
    const CostumeInfo* c = currentCostume();
    return Value(c ? c->name : "");
}

std::optional<int> Target::resolveCostume(const Value& selector, bool isBackdrop) const {
    const int count = static_cast<int>(costumes_.size());
    if (count == 0) return std::nullopt;
    // Numbers are 1-based indices (wrapping is applied by setCostumeByIndex).
    if (selector.isNumber()) {
        return static_cast<int>(std::round(selector.toNumber())) - 1;
    }
    const std::string name = selector.toString();
    for (int i = 0; i < count; ++i) {
        if (costumes_[static_cast<size_t>(i)].name == name) return i;
    }
    if (name == (isBackdrop ? "next backdrop" : "next costume")) return costume_ + 1;
    if (name == (isBackdrop ? "previous backdrop" : "previous costume")) return costume_ - 1;
    if (isBackdrop && name == "random backdrop") {
        if (count <= 1) return costume_;
        static std::mt19937 engine{std::random_device{}()};
        int pick = std::uniform_int_distribution<int>(0, count - 2)(engine);
        if (pick >= costume_) ++pick;   // never pick the current backdrop
        return pick;
    }
    // Numeric strings ("3") act like numbers; anything else is ignored.
    const double n = Value::parseNumber(name);
    if (!std::isnan(n) && !selector.isWhitespace()) {
        return static_cast<int>(std::round(n)) - 1;
    }
    return std::nullopt;
}

void Target::setCostumeByIndex(int index) {
    const int count = static_cast<int>(costumes_.size());
    if (count == 0) return;
    costume_ = ((index % count) + count) % count;
    runtime_->requestRedraw();
}

void Target::switchBackdropTo(const Value& selector) {
    Stage& s = stage();
    if (std::optional<int> idx = s.resolveCostume(selector, /*isBackdrop=*/true)) {
        s.setCostumeByIndex(*idx);
    }
}

Task Target::switchBackdropToAndWait(const Value& selector) {
    Stage& s = stage();
    const std::optional<int> idx = s.resolveCostume(selector, true);
    if (!idx) co_return;
    // Bypass Stage::setCostumeByIndex (which fires the hats itself) so we can
    // capture the started threads and wait for them.
    s.Target::setCostumeByIndex(*idx);
    const CostumeInfo* c = s.currentCostume();
    std::vector<std::shared_ptr<Thread>> started =
        runtime_->startHats(Thread::Hat::BackdropSwitched, c ? c->name : "");
    for (;;) {
        bool anyActive = false;
        for (const auto& t : started) {
            if (t && t->active()) { anyActive = true; break; }
        }
        if (!anyActive) break;
        co_await Yield{};
    }
}

void Target::nextBackdrop() {
    Stage& s = stage();
    s.setCostumeByIndex(s.costumeIndex() + 1);
}

Value Target::backdropNumber() const { return stage().costumeNumber(); }
Value Target::backdropName() const { return stage().costumeName(); }

void Target::setEffect(const std::string& effect, double value) {
    effects_[toLower(effect)] = value;
    runtime_->requestRedraw();
}

void Target::changeEffect(const std::string& effect, double value) {
    effects_[toLower(effect)] += value;
    runtime_->requestRedraw();
}

void Target::clearEffects() {
    effects_.clear();
    runtime_->requestRedraw();
}

double Target::effect(const std::string& effect) const {
    auto it = effects_.find(effect);
    return it == effects_.end() ? 0.0 : it->second;
}

// ---- Sound ----------------------------------------------------------------------

int Target::startSoundInternal(const Value& selector) {
    const int count = static_cast<int>(sounds_.size());
    if (count == 0) return 0;
    int index = -1;
    if (selector.isNumber()) {
        index = static_cast<int>(wrapClamp(selector.toNumber(), 1, count)) - 1;
    } else {
        const std::string name = selector.toString();
        for (int i = 0; i < count; ++i) {
            if (sounds_[static_cast<size_t>(i)].name == name) { index = i; break; }
        }
        if (index < 0) {
            const double n = Value::parseNumber(name);
            if (std::isnan(n)) return 0;
            index = static_cast<int>(wrapClamp(n, 1, count)) - 1;
        }
    }
    const SoundInfo& s = sounds_[static_cast<size_t>(index)];
    if (!s.data || !s.data->valid()) {
        unsupported("sound \"" + s.name + "\" (" + s.format + " format not decodable)");
        return 0;
    }
    return runtime_->audio().play(*s.data, name_, volume_);
}

Task Target::playSoundUntilDone(const Value& selector) {
    const int handle = startSoundInternal(selector);
    if (handle == 0) co_return;
    while (runtime_->audio().isPlaying(handle)) {
        co_await Yield{};
    }
}

void Target::startSound(const Value& selector) { startSoundInternal(selector); }

void Target::stopAllSounds() { runtime_->audio().stopOwner(name_); }

void Target::changeVolumeBy(double delta) { setVolumeTo(volume_ + delta); }

void Target::setVolumeTo(double volume) {
    volume_ = std::clamp(volume, 0.0, 100.0);
    runtime_->audio().setOwnerVolume(name_, volume_);
}

// ---- Events / control ------------------------------------------------------------

void Target::broadcast(const Value& message) {
    runtime_->startHats(Thread::Hat::BroadcastReceived, toLower(message.toString()));
}

Task Target::broadcastAndWait(const Value& message) {
    std::vector<std::shared_ptr<Thread>> started =
        runtime_->startHats(Thread::Hat::BroadcastReceived, toLower(message.toString()));
    for (;;) {
        bool anyActive = false;
        for (const auto& t : started) {
            if (t && t->active()) { anyActive = true; break; }
        }
        if (!anyActive) break;
        co_await Yield{};
    }
}

Task Target::wait(double seconds) {
    // scratch-vm always yields at least once, even for "wait 0".
    const double end = Runtime::now() + std::max(0.0, seconds);
    runtime_->requestRedraw();
    co_await Yield{};
    while (Runtime::now() < end) {
        co_await Yield{};
    }
}

void Target::createCloneOf(const Value& selector) {
    const std::string which = selector.toString();
    Sprite* original = nullptr;
    if (which == "_myself_") {
        original = isStage() ? nullptr : static_cast<Sprite*>(this);
        // Cloning a clone clones the clone's current state (like Scratch).
    } else {
        original = runtime_->findSprite(which);
    }
    if (!original) return;
    runtime_->createClone(*original);
}

// ---- Pen ---------------------------------------------------------------------------

void Target::penDown() { pen_.down = true; }
void Target::penUp() { pen_.down = false; }

void Target::setPenColorToColor(const Value& color) {
    std::uint8_t r = 0, g = 0, b = 0;
    parseScratchColor(color, r, g, b);
    pen_.applyRgb(r, g, b);
}

void Target::setPenColorParam(const Value& param, double value) {
    pen_.setParam(param.toString(), value, false);
}

void Target::changePenColorParam(const Value& param, double value) {
    pen_.setParam(param.toString(), value, true);
}

void Target::setPenSizeTo(double size) { pen_.setSize(size); }
void Target::changePenSizeBy(double delta) { pen_.changeSize(delta); }

void Target::clearPen() {
    runtime_->penLayer().clear();
    runtime_->requestRedraw();
}

void Target::stamp() {
    if (isStage()) return;
    static_cast<Sprite*>(this)->stampCostume();
}

void Target::showVariable(const std::string& name) {
    runtime_->setMonitorVisible(Monitor::Kind::Variable, name, isStage() ? std::string() : name_, true);
}
void Target::hideVariable(const std::string& name) {
    runtime_->setMonitorVisible(Monitor::Kind::Variable, name, isStage() ? std::string() : name_, false);
}
void Target::showList(const std::string& name) {
    runtime_->setMonitorVisible(Monitor::Kind::List, name, isStage() ? std::string() : name_, true);
}
void Target::hideList(const std::string& name) {
    runtime_->setMonitorVisible(Monitor::Kind::List, name, isStage() ? std::string() : name_, false);
}

// ---- Sensing -----------------------------------------------------------------------

bool Target::keyPressed(const Value& key) const { return runtime_->input().isKeyPressed(key); }
bool Target::mouseDown() const { return runtime_->input().mouseDown(); }
double Target::mouseX() const { return runtime_->input().mouseX(); }
double Target::mouseY() const { return runtime_->input().mouseY(); }
double Target::timer() const { return runtime_->timer(); }
void Target::resetTimer() { runtime_->resetTimer(); }

Value Target::sensingOf(const std::string& property, const Value& object) const {
    const std::string which = object.toString();
    if (which == "_stage_") {
        Stage& s = stage();
        if (property == "backdrop #") return s.costumeNumber();
        if (property == "backdrop name") return s.costumeName();
        if (property == "volume") return Value(s.volume());
        if (Value* v = s.variableByName(property)) return *v;
        return Value(0.0);
    }
    Sprite* sprite = runtime_->findSprite(which);
    if (!sprite) return Value(0.0);
    if (property == "x position") return Value(sprite->x());
    if (property == "y position") return Value(sprite->y());
    if (property == "direction") return Value(sprite->direction());
    if (property == "costume #") return sprite->costumeNumber();
    if (property == "costume name") return sprite->costumeName();
    if (property == "size") return Value(sprite->size());
    if (property == "volume") return Value(sprite->volume());
    if (Value* v = sprite->variableByName(property)) return *v;
    return Value(0.0);
}

Value Target::current(const std::string& unit) const {
    const std::time_t t = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    if (unit == "YEAR") return Value(static_cast<double>(local.tm_year + 1900));
    if (unit == "MONTH") return Value(static_cast<double>(local.tm_mon + 1));
    if (unit == "DATE") return Value(static_cast<double>(local.tm_mday));
    if (unit == "DAYOFWEEK") return Value(static_cast<double>(local.tm_wday + 1));
    if (unit == "HOUR") return Value(static_cast<double>(local.tm_hour));
    if (unit == "MINUTE") return Value(static_cast<double>(local.tm_min));
    if (unit == "SECOND") return Value(static_cast<double>(local.tm_sec));
    return Value(0.0);
}

double Target::daysSince2000() const {
    using namespace std::chrono;
    const auto nowMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    // 2000-01-01T00:00:00Z in milliseconds since the Unix epoch.
    const double epoch2000 = 946684800000.0;
    return (static_cast<double>(nowMs) - epoch2000) / (24.0 * 60.0 * 60.0 * 1000.0);
}

double Target::loudness() const {
    unsupported("loudness (microphone input)");
    return -1.0;
}

void Target::unsupported(const std::string& what) const {
    runtime_->warnUnsupported(what + " in \"" + name_ + "\"");
}

}  // namespace scratch
