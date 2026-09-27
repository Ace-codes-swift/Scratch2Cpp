#include "scratch/Audio.hpp"

#include <algorithm>

namespace scratch {

Audio::~Audio() { shutdown(); }

bool Audio::init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return false;
    }
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0) {
        SDL_Log("Could not open audio device: %s", SDL_GetError());
        return false;
    }
    return true;
}

void Audio::shutdown() {
    stopAll();
    if (device_) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

std::shared_ptr<SoundData> Audio::load(const std::string& path, const std::string& format) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;
    auto data = std::make_shared<SoundData>();
    if (format == "wav") {
        Uint8* buf = nullptr;
        Uint32 len = 0;
        if (SDL_LoadWAV(path.c_str(), &data->spec, &buf, &len)) {
            data->samples.assign(buf, buf + len);
            SDL_free(buf);
        } else {
            SDL_Log("Could not load sound %s: %s", path.c_str(), SDL_GetError());
        }
    }
    // Other formats (mp3) are left empty; Target reports them as unsupported.
    cache_[path] = data;
    return data;
}

int Audio::play(const SoundData& sound, const std::string& owner, double volumePercent) {
    if (!device_ || !sound.valid()) return 0;
    SDL_AudioStream* stream = SDL_CreateAudioStream(&sound.spec, nullptr);
    if (!stream) return 0;
    if (!SDL_BindAudioStream(device_, stream)) {
        SDL_DestroyAudioStream(stream);
        return 0;
    }
    SDL_SetAudioStreamGain(stream, static_cast<float>(std::clamp(volumePercent, 0.0, 100.0) / 100.0));
    SDL_PutAudioStreamData(stream, sound.samples.data(), static_cast<int>(sound.samples.size()));
    SDL_FlushAudioStream(stream);
    const int handle = nextHandle_++;
    playing_[handle] = Playing{stream, owner};
    return handle;
}

bool Audio::isPlaying(int handle) {
    auto it = playing_.find(handle);
    if (it == playing_.end()) return false;
    if (SDL_GetAudioStreamQueued(it->second.stream) > 0 || SDL_GetAudioStreamAvailable(it->second.stream) > 0) {
        return true;
    }
    SDL_DestroyAudioStream(it->second.stream);
    playing_.erase(it);
    return false;
}

void Audio::stopOwner(const std::string& owner) {
    for (auto it = playing_.begin(); it != playing_.end();) {
        if (it->second.owner == owner) {
            SDL_DestroyAudioStream(it->second.stream);
            it = playing_.erase(it);
        } else {
            ++it;
        }
    }
}

void Audio::stopAll() {
    for (auto& [handle, p] : playing_) SDL_DestroyAudioStream(p.stream);
    playing_.clear();
}

void Audio::setOwnerVolume(const std::string& owner, double volumePercent) {
    const float gain = static_cast<float>(std::clamp(volumePercent, 0.0, 100.0) / 100.0);
    for (auto& [handle, p] : playing_) {
        if (p.owner == owner) SDL_SetAudioStreamGain(p.stream, gain);
    }
}

void Audio::update() {
    for (auto it = playing_.begin(); it != playing_.end();) {
        if (SDL_GetAudioStreamQueued(it->second.stream) == 0 && SDL_GetAudioStreamAvailable(it->second.stream) == 0) {
            SDL_DestroyAudioStream(it->second.stream);
            it = playing_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace scratch
