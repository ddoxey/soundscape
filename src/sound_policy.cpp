#include "sound_policy.hpp"

bool SoundPolicyEngine::CanStart(
    const SoundDef& def,
    std::span<const SoundInstance> active_instances) const {
  if (!def.drop_if_higher_priority_active) {
    return true;
  }

  // Suppression is evaluated at trigger time only. Once a sound is active,
  // ducking handles gain relationships while it plays.
  return !HigherPrioritySoundActive(def.priority, active_instances);
}

float SoundPolicyEngine::DuckFactorFor(
    SoundPriority target,
    std::span<const SoundInstance> active_instances) const noexcept {
  SoundPriority highest_ducker = SoundPriority::kBackground;
  bool ducking_active = false;

  // The strongest active ducking sound controls the ducking curve. Equal or
  // higher-priority targets are left untouched by DuckGainFor().
  for (const SoundInstance& instance : active_instances) {
    if (!instance.active || !instance.duck_others) {
      continue;
    }

    if (!ducking_active || static_cast<int>(instance.priority) >
                               static_cast<int>(highest_ducker)) {
      highest_ducker = instance.priority;
      ducking_active = true;
    }
  }

  if (!ducking_active) {
    return 1.0f;
  }

  return DuckGainFor(highest_ducker, target);
}

bool SoundPolicyEngine::HigherPrioritySoundActive(
    SoundPriority priority,
    std::span<const SoundInstance> active_instances) const noexcept {
  for (const SoundInstance& instance : active_instances) {
    if (!instance.active) {
      continue;
    }

    if (static_cast<int>(instance.priority) > static_cast<int>(priority)) {
      return true;
    }
  }

  return false;
}

float SoundPolicyEngine::DuckGainFor(SoundPriority active_ducker,
                                     SoundPriority target) const noexcept {
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
