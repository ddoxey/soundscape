#pragma once

#include <span>

#include "audio_types.hpp"

class SoundPolicyEngine {
 public:
  [[nodiscard]] bool CanStart(
      const SoundDef& def,
      std::span<const SoundInstance> active_instances) const;
  [[nodiscard]] float DuckFactorFor(
      SoundPriority target,
      std::span<const SoundInstance> active_instances) const noexcept;

 private:
  [[nodiscard]] bool HigherPrioritySoundActive(
      SoundPriority priority,
      std::span<const SoundInstance> active_instances) const noexcept;
  [[nodiscard]] float DuckGainFor(SoundPriority active_ducker,
                                  SoundPriority target) const noexcept;
};
