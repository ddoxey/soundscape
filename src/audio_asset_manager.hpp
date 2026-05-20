#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio_types.hpp"
#include "sound_catalog_runtime.hpp"

class AudioAssetManager {
 public:
  bool LoadCatalog(const SoundCatalog& catalog, std::string& error_message);
  void Clear();

  [[nodiscard]] const SoundBuffer* Find(SoundId id) const noexcept;

 private:
  bool LoadSound(const SoundDef& def, std::string& error_message);
  [[nodiscard]] SoundBuffer ResampleToInternalFormat(
      const std::vector<float>& source_samples, int source_channels,
      int source_sample_rate) const;

  std::unordered_map<SoundId, SoundBuffer> buffers_;
};
