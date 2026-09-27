// Audio.hpp - minimal SDL3 audio playback for Scratch sounds.
//
// Each "start sound" creates an SDL_AudioStream bound to the default output
// device. WAV (including ADPCM) files are decoded with SDL_LoadWAV; other
// formats (e.g. mp3) are reported as unsupported.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace scratch {

struct SoundData {
    SDL_AudioSpec spec{};
    std::vector<std::uint8_t> samples;
    bool valid() const { return !samples.empty(); }
};

class Audio {
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();
    bool available() const { return device_ != 0; }

    std::shared_ptr<SoundData> load(const std::string& path, const std::string& format);

    // Starts playback; returns a handle (>0) or 0 on failure. `owner` groups
    // sounds so that "stop all sounds" / volume changes apply per sprite.
    int play(const SoundData& sound, const std::string& owner, double volumePercent);
    bool isPlaying(int handle);
    void stopOwner(const std::string& owner);
    void stopAll();
    void setOwnerVolume(const std::string& owner, double volumePercent);
    void update();   // reap finished streams

private:
    struct Playing {
        SDL_AudioStream* stream = nullptr;
        std::string owner;
    };
    SDL_AudioDeviceID device_ = 0;
    std::map<int, Playing> playing_;
    std::map<std::string, std::shared_ptr<SoundData>> cache_;
    int nextHandle_ = 1;
};

}  // namespace scratch
