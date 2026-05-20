#include "mixer.hpp"

#include <algorithm>
#include <cmath>

bool Mixer::Play(const SoundDef& def, const SoundBuffer& buffer) {
  if (!policy_.CanStart(def, active_instances_)) {
    return false;
  }

  if (def.loop) {
    // A looping bed should be idempotent: replaying it refreshes its gain
    // target instead of adding another copy of the same loop.
    for (SoundInstance& instance : active_instances_) {
      if (instance.active && instance.id == def.id && instance.loop) {
        instance.target_gain = instance.base_gain;
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
    return true;
  }

  // Refuse playback when the fixed pool is full. This keeps the render path
  // allocation-free and makes capacity limits explicit.
  return false;
}

void Mixer::Stop(SoundId id) {
  for (SoundInstance& instance : active_instances_) {
    if (instance.id == id) {
      instance.active = false;
    }
  }
}

void Mixer::Clear() {
  for (SoundInstance& instance : active_instances_) {
    instance.active = false;
  }
}

void Mixer::Mix(float* output, int frame_count) {
  std::fill(output,
            output + static_cast<std::size_t>(frame_count) * kOutputChannels,
            0.0f);

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
