#include "mixer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "debug_log.hpp"

namespace {

/**
 * @brief Counts active voice slots that contain a decoded buffer.
 */
template <std::size_t kInstanceCount>
std::size_t ActiveVoiceCount(
    const std::array<SoundInstance, kInstanceCount>& instances) {
  return static_cast<std::size_t>(std::count_if(
      instances.begin(), instances.end(), [](const SoundInstance& instance) {
        return instance.active && instance.buffer != nullptr;
      }));
}

/**
 * @brief Limits periodic mix callback logging to startup and powers of two.
 */
bool ShouldLogMixCount(std::uint64_t mix_count) {
  return mix_count <= 5 || (mix_count & (mix_count - 1)) == 0;
}

}  // namespace

bool Mixer::Play(const SoundDef& def, const SoundBuffer& buffer) {
  if (!policy_.CanStart(def, active_instances_)) {
    std::ostringstream log;
    log << "mixer play rejected by policy: sound=" << def.name;
    debug_log::Write(log.str());
    return false;
  }

  if (def.loop) {
    // A looping bed should be idempotent: replaying it refreshes its gain
    // target instead of adding another copy of the same loop.
    for (SoundInstance& instance : active_instances_) {
      if (instance.active && instance.id == def.id && instance.loop) {
        instance.target_gain = instance.base_gain;
        std::ostringstream log;
        log << "mixer refreshed loop: sound=" << def.name;
        debug_log::Write(log.str());
        return true;
      }
    }
  }

  for (SoundInstance& instance : active_instances_) {
    if (instance.active) {
      continue;
    }

    instance = SoundInstance{
        .id = def.id,
        .buffer = &buffer,
        .frame_position = 0,
        .base_gain = def.gain,
        .current_gain = def.gain,
        .target_gain = def.gain,
        .loop = def.loop,
        .active = true,
        .priority = def.priority,
        .duck_others = def.duck_others,
    };
    if (debug_log::Enabled()) {
      std::ostringstream log;
      log << "mixer started voice: sound=" << def.name
          << " active_voices=" << ActiveVoiceCount(active_instances_)
          << " frames=" << buffer.FrameCount()
          << " loop=" << (def.loop ? "true" : "false") << " gain=" << def.gain;
      debug_log::Write(log.str());
    }
    return true;
  }

  // Refuse playback when the fixed pool is full. This keeps the render path
  // allocation-free and makes capacity limits explicit.
  std::ostringstream log;
  log << "mixer play rejected: voice pool full for sound=" << def.name;
  debug_log::Write(log.str());
  return false;
}

void Mixer::Stop(SoundId id) {
  std::size_t stopped_count = 0;
  for (SoundInstance& instance : active_instances_) {
    if (instance.id == id) {
      if (instance.active) {
        ++stopped_count;
      }
      instance.active = false;
    }
  }
  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "mixer stopped voices: count=" << stopped_count;
    debug_log::Write(log.str());
  }
}

void Mixer::Clear() {
  for (SoundInstance& instance : active_instances_) {
    instance.active = false;
  }
}

void Mixer::Mix(float* output, int frame_count) {
  ++mix_count_;
  std::fill(output,
            output + static_cast<std::size_t>(frame_count) * kOutputChannels,
            0.0f);

  if (debug_log::Enabled() && ShouldLogMixCount(mix_count_)) {
    std::ostringstream log;
    log << "mixer mix callback: count=" << mix_count_
        << " active_voices=" << ActiveVoiceCount(active_instances_)
        << " frame_count=" << frame_count;
    debug_log::Write(log.str());
  }

  // Gain changes are ramped over a short window to avoid clicks when ducking
  // turns on/off or when loops are stopped.
  constexpr float kRampTimeSeconds = 0.015f;
  const float ramp_step =
      1.0f /
      std::max(1.0f, kRampTimeSeconds * static_cast<float>(kOutputSampleRate));

  for (SoundInstance& instance : active_instances_) {
    if (!instance.active || instance.buffer == nullptr) {
      continue;
    }

    const float duck_factor =
        policy_.DuckFactorFor(instance.priority, active_instances_);
    instance.target_gain = instance.base_gain * duck_factor;

    // Mix one voice into the shared output buffer. The output backend owns the
    // final device format conversion.
    const std::size_t total_frames = instance.buffer->FrameCount();
    if (total_frames == 0) {
      instance.active = false;
      continue;
    }

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

      const std::size_t sample_index =
          instance.frame_position * kOutputChannels;
      output[static_cast<std::size_t>(frame) * kOutputChannels] +=
          instance.buffer->samples[sample_index] * instance.current_gain;
      output[static_cast<std::size_t>(frame) * kOutputChannels + 1] +=
          instance.buffer->samples[sample_index + 1] * instance.current_gain;

      ++instance.frame_position;
    }
  }

  // Clamp after summing all voices so overlapping alerts cannot exceed the
  // normalized output range.
  for (int frame = 0; frame < frame_count; ++frame) {
    const std::size_t index = static_cast<std::size_t>(frame) * kOutputChannels;
    output[index] = std::clamp(output[index], -1.0f, 1.0f);
    output[index + 1] = std::clamp(output[index + 1], -1.0f, 1.0f);
  }
}
