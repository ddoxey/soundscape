#include "mixer.hpp"

#include <algorithm>
#include <cmath>

bool Mixer::Play(const SoundDef& def, const SoundBuffer& buffer) {
  if (!policy_.CanStart(def, active_instances_)) {
    return false;
  }

  if (def.loop) {
    for (SoundInstance& instance : active_instances_) {
      if (instance.active && instance.id == def.id && instance.loop) {
        instance.target_gain = instance.base_gain;
        return true;
      }
    }
  }

  active_instances_.push_back(SoundInstance{
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
  });
  return true;
}

void Mixer::Stop(SoundId id) {
  for (SoundInstance& instance : active_instances_) {
    if (instance.id == id) {
      instance.active = false;
    }
  }
}

void Mixer::Clear() { active_instances_.clear(); }

void Mixer::Mix(float* output, int frame_count) {
  std::fill(output,
            output + static_cast<std::size_t>(frame_count) * kOutputChannels,
            0.0f);

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

      const std::size_t sample_index =
          instance.frame_position * kOutputChannels;
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

void Mixer::CompactInactiveVoices() {
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
