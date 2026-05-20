#pragma once

#include <vector>

#include "audio_types.hpp"
#include "sound_policy.hpp"

class Mixer {
 public:
  [[nodiscard]] bool Play(const SoundDef& def, const SoundBuffer& buffer);
  void Stop(SoundId id);
  void Clear();
  void Mix(float* output, int frame_count);

 private:
  void CompactInactiveVoices();

  SoundPolicyEngine policy_;
  std::vector<SoundInstance> active_instances_;
};
