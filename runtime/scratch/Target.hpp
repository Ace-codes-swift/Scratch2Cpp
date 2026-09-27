// Target.hpp - base class shared by the Stage and Sprites.
//
// A Scratch "target" owns costumes, sounds, variables, lists and scripts.
// Generated code subclasses Sprite / Stage and registers its scripts (hat
// blocks) in the constructor.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "scratch/Coroutine.hpp"
#include "scratch/List.hpp"
#include "scratch/Pen.hpp"
#include "scratch/Value.hpp"

namespace scratch {

class Runtime;
class Stage;
struct Costume;      // rendered costume (Renderer.hpp)
struct SoundData;    // decoded sound (Audio.hpp)

struct CostumeInfo {
    std::string name;
    std::string file;              // file name inside assets/
    std::string format;            // "svg", "png", ...
    double rotationCenterX = 0.0;
    double rotationCenterY = 0.0;
    double bitmapResolution = 1.0;
    std::shared_ptr<Costume> rendered;  // filled in by Renderer at load time
};

struct SoundInfo {
    std::string name;
    std::string file;
    std::string format;            // "wav", "mp3"
    std::shared_ptr<SoundData> data;    // filled in by Audio at load time
};

struct HatBinding {
    Thread::Hat hat;
    std::string option;            // key name / broadcast name (lower-case) / backdrop name
    ScriptFactory factory;
    // Edge-triggered hats ("when timer > x") run when this goes false -> true.
    std::function<bool(Target&)> predicate;
};

class Target {
public:
    Target(Runtime& runtime, std::string name);
    virtual ~Target();

    virtual bool isStage() const = 0;
    Runtime& runtime() const { return *runtime_; }
    Stage& stage() const;
    const std::string& name() const { return name_; }

    // ---- Setup (called from generated code) -------------------------------
    void addCostume(CostumeInfo info) { costumes_.push_back(std::move(info)); }
    void addSound(SoundInfo info) { sounds_.push_back(std::move(info)); }
    void addVariable(const std::string& name, Value initial);
    void addList(const std::string& name, std::vector<Value> initial);
    void addScript(Thread::Hat hat, const std::string& option, ScriptFactory factory);
    void addEdgeScript(std::function<bool(Target&)> predicate, ScriptFactory factory);
    void setCurrentCostume(int index) { costume_ = index; }
    void setVolume(double v) { volume_ = v; }

    // ---- Data -------------------------------------------------------------
    // Generated code indexes these directly using the constants it emits.
    std::vector<Value> variables_;
    std::vector<List> lists_;
    Value* variableByName(const std::string& name);
    const Value* variableByName(const std::string& name) const;
    List* listByName(const std::string& name);
    const List* listByName(const std::string& name) const;
    // Scratch tolerates scripts referring to variables/lists that were never
    // declared; these create them on first use.
    Value& dynamicVariable(const std::string& name) { return dynamicVariables_[name]; }
    List& dynamicList(const std::string& name) { return dynamicLists_[name]; }
    const std::vector<std::string>& variableNames() const { return variableNames_; }

    // ---- Costumes ---------------------------------------------------------
    std::vector<CostumeInfo>& costumes() { return costumes_; }
    const std::vector<CostumeInfo>& costumes() const { return costumes_; }
    int costumeIndex() const { return costume_; }
    const CostumeInfo* currentCostume() const;
    // Resolve a costume selector like Scratch does ("name", number, "next
    // costume", ...). The result is an unwrapped zero-based index (it may be
    // out of range; setCostumeByIndex wraps it). Empty when nothing matches.
    std::optional<int> resolveCostume(const Value& selector, bool isBackdrop) const;
    virtual void setCostumeByIndex(int index);   // wraps around
    Value costumeNumber() const { return Value(static_cast<double>(costume_ + 1)); }
    Value costumeName() const;

    // Looks shared by sprites and stage.
    void switchBackdropTo(const Value& selector);
    Task switchBackdropToAndWait(const Value& selector);
    void nextBackdrop();
    Value backdropNumber() const;
    Value backdropName() const;
    void setEffect(const std::string& effect, double value);
    void changeEffect(const std::string& effect, double value);
    void clearEffects();
    double effect(const std::string& effect) const;

    // ---- Sound ------------------------------------------------------------
    std::vector<SoundInfo>& sounds() { return sounds_; }
    Task playSoundUntilDone(const Value& selector);
    void startSound(const Value& selector);
    void stopAllSounds();
    void changeVolumeBy(double delta);
    void setVolumeTo(double volume);
    double volume() const { return volume_; }

    // ---- Events / control -------------------------------------------------
    void broadcast(const Value& message);
    Task broadcastAndWait(const Value& message);
    Task wait(double seconds);
    void createCloneOf(const Value& selector);

    // ---- Pen (Scratch pen extension) --------------------------------------
    void penDown();
    void penUp();
    void setPenColorToColor(const Value& color);
    void setPenColorParam(const Value& param, double value);
    void changePenColorParam(const Value& param, double value);
    void setPenSizeTo(double size);
    void changePenSizeBy(double delta);
    void stamp();
    void clearPen();
    void showVariable(const std::string& name);
    void hideVariable(const std::string& name);
    void showList(const std::string& name);
    void hideList(const std::string& name);
    const PenStyle& penStyle() const { return pen_; }

    // ---- Sensing ----------------------------------------------------------
    bool keyPressed(const Value& key) const;
    bool mouseDown() const;
    double mouseX() const;
    double mouseY() const;
    double timer() const;
    void resetTimer();
    Value sensingOf(const std::string& property, const Value& object) const;
    Value current(const std::string& unit) const;
    double daysSince2000() const;
    std::string username() const { return ""; }
    double loudness() const;

    // ---- Scripts ----------------------------------------------------------
    const std::vector<HatBinding>& hats() const { return hats_; }

    // Warn (once) about a block the converter could not translate.
    void unsupported(const std::string& what) const;

protected:
    friend class Runtime;
    Runtime* runtime_;
    std::string name_;
    std::vector<CostumeInfo> costumes_;
    int costume_ = 0;
    std::vector<SoundInfo> sounds_;
    double volume_ = 100.0;
    std::map<std::string, double> effects_;
    std::vector<std::string> variableNames_;
    std::vector<std::string> listNames_;
    std::map<std::string, Value> dynamicVariables_;
    std::map<std::string, List> dynamicLists_;
    std::vector<HatBinding> hats_;
    PenStyle pen_;

    int startSoundInternal(const Value& selector);
};

}  // namespace scratch
