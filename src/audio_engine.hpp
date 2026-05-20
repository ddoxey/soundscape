#pragma once

#include "sound_catalog.hpp"

#include <SDL.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct SoundBuffer {
    int sample_rate = 0;
    int channels = 0;
    std::vector<float> samples;

    [[nodiscard]] std::size_t FrameCount() const noexcept {
        if (channels <= 0) {
            return 0;
        }

        return samples.size() / static_cast<std::size_t>(channels);
    }
};

struct SoundInstance {
    SoundId id {};
    const SoundBuffer* buffer = nullptr;
    std::size_t frame_position = 0;
    float base_gain = 1.0f;
    float current_gain = 1.0f;
    float target_gain = 1.0f;
    bool loop = false;
    bool active = true;
    SoundPriority priority = SoundPriority::kNormal;
    bool duck_others = false;
};

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Initialize(std::string& error_message);
    void Shutdown();

    bool LoadCatalog(std::string& error_message);

    [[nodiscard]] bool Play(SoundId id);
    void Stop(SoundId id);

    [[nodiscard]] const SoundDef* FindSoundDef(SoundId id) const noexcept;

private:
    static constexpr int kOutputSampleRate = 48000;
    static constexpr int kOutputChannels = 2;
    static constexpr int kBufferFrames = 1024;

    static void AudioCallback(void* userdata, Uint8* stream, int len);
    void Mix(float* output, int frame_count);

    bool LoadSound(const SoundDef& def, std::string& error_message);
    [[nodiscard]] bool HigherPrioritySoundActive(SoundPriority priority) const;
    SoundBuffer ResampleToInternalFormat(const std::vector<float>& source_samples,
                                        int source_channels,
                                        int source_sample_rate) const;
    void CompactInactiveVoices();

    template <typename Fn>
    void WithDeviceLock(const Fn& fn) {
        if (device_id_ == 0) {
            return;
        }

        SDL_LockAudioDevice(device_id_);
        fn();
        SDL_UnlockAudioDevice(device_id_);
    }

    SDL_AudioDeviceID device_id_ = 0;
    SDL_AudioSpec obtained_spec_ {};
    std::unordered_map<SoundId, SoundBuffer> buffers_;
    std::vector<SoundInstance> active_instances_;
};
