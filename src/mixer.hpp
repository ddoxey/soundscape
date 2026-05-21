#pragma once

#include <array>
#include <cstdint>

#include "audio_types.hpp"
#include "sound_policy.hpp"

/**
 * @brief Real-time-ish voice mixer for decoded sound buffers.
 *
 * Mixer owns the active voice list, applies policy-driven gain changes,
 * advances playback cursors, and writes interleaved stereo float samples for
 * the output backend. Active voices are stored in a fixed-size pool so the
 * render path does not allocate, resize, or compact containers.
 */
class Mixer {
 public:
  /**
   * @brief Starts a new voice or refreshes an existing loop.
   *
   * @return false when SoundPolicyEngine suppresses the request or when the
   * fixed voice pool is full.
   */
  [[nodiscard]] bool Play(const SoundDef& def, const SoundBuffer& buffer);

  /**
   * @brief Marks all active voices with this id inactive.
   */
  void Stop(SoundId id);

  /**
   * @brief Removes all active voices.
   */
  void Clear();

  /**
   * @brief Mixes the active voices into the caller-provided output buffer.
   *
   * @param output Interleaved stereo float output buffer.
   * @param frame_count Number of stereo frames to render.
   */
  void Mix(float* output, int frame_count);

 private:
  /**
   * @brief Maximum number of voices the mixer can render at once.
   */
  static constexpr std::size_t kMaxActiveVoices = 32;

  SoundPolicyEngine policy_;
  std::array<SoundInstance, kMaxActiveVoices> active_instances_{};
  std::uint64_t mix_count_ = 0;
};
