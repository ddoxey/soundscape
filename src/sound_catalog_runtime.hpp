#pragma once

#include <span>

#include "sound_catalog.hpp"

class SoundCatalog {
 public:
  [[nodiscard]] std::span<const SoundDef> All() const noexcept;
  [[nodiscard]] const SoundDef* Find(SoundId id) const noexcept;
};
