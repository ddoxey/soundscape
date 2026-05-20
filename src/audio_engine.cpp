#include "audio_engine.hpp"

#include <sndfile.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string_view>

namespace {

float DuckGainFor(SoundPriority active_ducker, SoundPriority target) {
    if (static_cast<int>(target) >= static_cast<int>(active_ducker)) {
        return 1.0f;
    }

    if (active_ducker == SoundPriority::kCritical) {
        switch (target) {
        case SoundPriority::kBackground:
            return 0.35f;
        case SoundPriority::kNormal:
            return 0.50f;
        case SoundPriority::kAlert:
            return 0.70f;
        case SoundPriority::kCritical:
            return 1.0f;
        }
    }

    if (active_ducker == SoundPriority::kAlert) {
        switch (target) {
        case SoundPriority::kBackground:
            return 0.60f;
        case SoundPriority::kNormal:
            return 0.75f;
        case SoundPriority::kAlert:
        case SoundPriority::kCritical:
            return 1.0f;
        }
    }

    return 1.0f;
}

std::string BuildSndfileError(std::string_view path) {
    return "Failed to load " + std::string(path) + ": " + sf_strerror(nullptr);
}

} // namespace

AudioEngine::~AudioEngine() {
    Shutdown();
}

bool AudioEngine::Initialize(std::string& error_message) {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        error_message = SDL_GetError();
        return false;
    }

    SDL_AudioSpec desired_spec{};
    desired_spec.freq = kOutputSampleRate;
    desired_spec.format = AUDIO_F32SYS;
    desired_spec.channels = kOutputChannels;
    desired_spec.samples = kBufferFrames;
    desired_spec.callback = &AudioEngine::AudioCallback;
    desired_spec.userdata = this;

    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &obtained_spec_, 0);
    if (device_id_ == 0) {
        error_message = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    if (obtained_spec_.freq != desired_spec.freq ||
        obtained_spec_.format != desired_spec.format ||
        obtained_spec_.channels != desired_spec.channels) {
        error_message = "Audio device did not provide 48 kHz stereo float output";
        Shutdown();
        return false;
    }

    SDL_PauseAudioDevice(device_id_, 0);
    return true;
}

void AudioEngine::Shutdown() {
    if (device_id_ != 0) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }

    active_instances_.clear();
    buffers_.clear();

    if (SDL_WasInit(SDL_INIT_AUDIO) != 0U) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

bool AudioEngine::LoadCatalog(std::string& error_message) {
    for (const SoundDef& def : kSoundCatalog) {
        if (!LoadSound(def, error_message)) {
            return false;
        }
    }

    return true;
}

bool AudioEngine::Play(SoundId id) {
    const SoundDef* def = FindSoundDef(id);
    if (def == nullptr) {
        return false;
    }

    bool started = false;

    WithDeviceLock([&] {
        if (def->drop_if_higher_priority_active &&
            HigherPrioritySoundActive(def->priority)) {
            return;
        }

        const auto buffer_it = buffers_.find(id);
        if (buffer_it == buffers_.end()) {
            return;
        }

        if (def->loop) {
            for (SoundInstance& instance : active_instances_) {
                if (instance.active && instance.id == id && instance.loop) {
                    instance.target_gain = instance.base_gain;
                    started = true;
                    return;
                }
            }
        }

        active_instances_.push_back(SoundInstance{
            .id = id,
            .buffer = &buffer_it->second,
            .frame_position = 0,
            .base_gain = def->gain,
            .current_gain = def->gain,
            .target_gain = def->gain,
            .loop = def->loop,
            .active = true,
            .priority = def->priority,
            .duck_others = def->duck_others,
        });
        started = true;
    });

    return started;
}

void AudioEngine::Stop(SoundId id) {
    WithDeviceLock([&] {
        for (SoundInstance& instance : active_instances_) {
            if (instance.id == id) {
                instance.active = false;
            }
        }
    });
}

const SoundDef* AudioEngine::FindSoundDef(SoundId id) const noexcept {
    const auto it = std::find_if(kSoundCatalog.begin(), kSoundCatalog.end(),
                                 [id](const SoundDef& def) { return def.id == id; });
    if (it == kSoundCatalog.end()) {
        return nullptr;
    }

    return &(*it);
}

void AudioEngine::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* engine = static_cast<AudioEngine*>(userdata);
    std::memset(stream, 0, static_cast<std::size_t>(len));

    if (engine == nullptr) {
        return;
    }

    engine->Mix(reinterpret_cast<float*>(stream),
                len / static_cast<int>(sizeof(float) * kOutputChannels));
}

void AudioEngine::Mix(float* output, int frame_count) {
    std::fill(output,
              output + static_cast<std::size_t>(frame_count) * kOutputChannels,
              0.0f);

    SoundPriority highest_ducker = SoundPriority::kBackground;
    bool ducking_active = false;
    for (const SoundInstance& instance : active_instances_) {
        if (!instance.active || !instance.duck_others) {
            continue;
        }

        if (!ducking_active ||
            static_cast<int>(instance.priority) > static_cast<int>(highest_ducker)) {
            highest_ducker = instance.priority;
            ducking_active = true;
        }
    }

    constexpr float kRampTimeSeconds = 0.015f;
    const float ramp_step =
        1.0f / std::max(1.0f, kRampTimeSeconds * static_cast<float>(kOutputSampleRate));

    for (SoundInstance& instance : active_instances_) {
        if (!instance.active || instance.buffer == nullptr) {
            continue;
        }

        const float duck_factor =
            ducking_active ? DuckGainFor(highest_ducker, instance.priority) : 1.0f;
        instance.target_gain = instance.base_gain * duck_factor;

        const std::size_t total_frames = instance.buffer->FrameCount();
        for (int frame = 0; frame < frame_count; ++frame) {
            if (instance.frame_position >= total_frames) {
                if (!instance.loop) {
                    instance.active = false;
                    break;
                }

                instance.frame_position = 0;
            }

            const float gain_delta = instance.target_gain - instance.current_gain;
            if (std::fabs(gain_delta) <= ramp_step) {
                instance.current_gain = instance.target_gain;
            } else {
                instance.current_gain += (gain_delta > 0.0f ? ramp_step : -ramp_step);
            }

            const std::size_t sample_index = instance.frame_position * kOutputChannels;
            output[static_cast<std::size_t>(frame) * kOutputChannels] +=
                instance.buffer->samples[sample_index] * instance.current_gain;
            output[static_cast<std::size_t>(frame) * kOutputChannels + 1] +=
                instance.buffer->samples[sample_index + 1] * instance.current_gain;

            ++instance.frame_position;
        }
    }

    CompactInactiveVoices();

    for (int frame = 0; frame < frame_count; ++frame) {
        const std::size_t index = static_cast<std::size_t>(frame) * kOutputChannels;
        output[index] = std::clamp(output[index], -1.0f, 1.0f);
        output[index + 1] = std::clamp(output[index + 1], -1.0f, 1.0f);
    }
}

bool AudioEngine::LoadSound(const SoundDef& def, std::string& error_message) {
    SF_INFO info{};
    SNDFILE* file = sf_open(def.file_path.data(), SFM_READ, &info);
    if (file == nullptr) {
        error_message = BuildSndfileError(def.file_path);
        return false;
    }

    std::vector<float> source_samples(static_cast<std::size_t>(info.frames) *
                                      info.channels);
    const sf_count_t read_frames =
        sf_readf_float(file, source_samples.data(), info.frames);
    sf_close(file);

    if (read_frames != info.frames) {
        error_message = "Short read while loading " + std::string(def.file_path);
        return false;
    }

    buffers_.emplace(
        def.id,
        ResampleToInternalFormat(source_samples, info.channels, info.samplerate));
    return true;
}

bool AudioEngine::HigherPrioritySoundActive(SoundPriority priority) const {
    for (const SoundInstance& instance : active_instances_) {
        if (!instance.active) {
            continue;
        }

        if (static_cast<int>(instance.priority) > static_cast<int>(priority)) {
            return true;
        }
    }

    return false;
}

SoundBuffer AudioEngine::ResampleToInternalFormat(
    const std::vector<float>& source_samples, int source_channels,
    int source_sample_rate) const {
    const std::size_t source_frames =
        source_samples.size() / static_cast<std::size_t>(source_channels);
    const double ratio = static_cast<double>(kOutputSampleRate) /
                         static_cast<double>(source_sample_rate);
    const std::size_t output_frames = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(static_cast<double>(source_frames) *
                                                 ratio)));

    SoundBuffer output;
    output.sample_rate = kOutputSampleRate;
    output.channels = kOutputChannels;
    output.samples.resize(output_frames * kOutputChannels);

    for (std::size_t frame = 0; frame < output_frames; ++frame) {
        const double source_position = static_cast<double>(frame) / ratio;
        const std::size_t left_index = std::min<std::size_t>(
            source_frames - 1, static_cast<std::size_t>(source_position));
        const std::size_t right_index =
            std::min(source_frames - 1, left_index + 1);
        const float interpolation =
            static_cast<float>(source_position - static_cast<double>(left_index));

        for (int channel = 0; channel < kOutputChannels; ++channel) {
            const int source_channel = std::min(channel, source_channels - 1);
            const float left_sample =
                source_samples[left_index * static_cast<std::size_t>(source_channels) +
                               source_channel];
            const float right_sample =
                source_samples[right_index * static_cast<std::size_t>(source_channels) +
                               source_channel];
            output.samples[frame * kOutputChannels + channel] =
                left_sample + (right_sample - left_sample) * interpolation;
        }
    }

    return output;
}

void AudioEngine::CompactInactiveVoices() {
    std::size_t write_index = 0;
    for (std::size_t read_index = 0; read_index < active_instances_.size();
         ++read_index) {
        if (!active_instances_[read_index].active) {
            continue;
        }

        if (write_index != read_index) {
            active_instances_[write_index] = active_instances_[read_index];
        }
        ++write_index;
    }

    active_instances_.resize(write_index);
}
