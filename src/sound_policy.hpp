#pragma once

#include <span>

#include "audio_types.hpp"

/**
 * @brief Encapsulates alert suppression and ducking policy.
 *
 * Keeping policy separate from the mixer makes it easier to test operational
 * behavior without an audio device.
 */
class SoundPolicyEngine {
 public:
  /**
   * @brief Determines whether a sound is allowed to start right now.
   */
  [[nodiscard]] bool CanStart(
      const SoundDef& def,
      std::span<const SoundInstance> active_instances) const;

  /**
   * @brief Returns the gain multiplier for a target priority.
   */
  [[nodiscard]] float DuckFactorFor(
      SoundPriority target,
      std::span<const SoundInstance> active_instances) const noexcept;

 private:
  /**
   * @brief Checks whether any active voice outranks the requested priority.
   */
  [[nodiscard]] bool HigherPrioritySoundActive(
      SoundPriority priority,
      std::span<const SoundInstance> active_instances) const noexcept;

  /**
   * @brief Maps the highest active ducking priority to a target gain.
   */
  [[nodiscard]] float DuckGainFor(SoundPriority active_ducker,
                                  SoundPriority target) const noexcept;
};
