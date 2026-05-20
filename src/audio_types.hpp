#pragma once

#include <cstddef>
#include <vector>

#include "sound_catalog.hpp"

inline constexpr int kOutputSampleRate = 48000;
inline constexpr int kOutputChannels = 2;
inline constexpr int kBufferFrames = 1024;

/**
 * @brief Decoded audio data normalized to the mixer output format.
 *
 * All buffers loaded by the asset manager are converted to 48 kHz stereo float
 * samples so the mixer does not need to handle per-asset format differences.
 */
struct SoundBuffer {
  int sample_rate = 0;
  int channels = 0;
  std::vector<float> samples;

  /**
   * @brief Returns the number of interleaved audio frames in the buffer.
   */
  [[nodiscard]] std::size_t FrameCount() const noexcept {
    if (channels <= 0) {
      return 0;
    }

    return samples.size() / static_cast<std::size_t>(channels);
  }
};

/**
 * @brief Runtime playback state for one active voice in the mixer.
 *
 * A looping catalog entry normally has at most one active instance. One-shot
 * sounds can have multiple instances if triggered repeatedly.
 */
struct SoundInstance {
  SoundId id{};
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
