#include "sound_catalog_runtime.hpp"

#include <algorithm>

std::span<const SoundDef> SoundCatalog::All() const noexcept {
  return kSoundCatalog;
}

const SoundDef* SoundCatalog::Find(SoundId id) const noexcept {
  const auto it =
      std::find_if(kSoundCatalog.begin(), kSoundCatalog.end(),
                   [id](const SoundDef& def) { return def.id == id; });
  if (it == kSoundCatalog.end()) {
    return nullptr;
  }

  return &(*it);
}
