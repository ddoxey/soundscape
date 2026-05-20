#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sound_catalog.hpp"

/**
 * @brief Parses a YAML/config sound id key into a SoundId value.
 *
 * @return true when the key is recognized.
 */
[[nodiscard]] bool TryParseSoundId(std::string_view id, SoundId& sound_id);

/**
 * @brief Runtime view of the YAML-backed sound catalog.
 */
class SoundCatalog {
 public:
  /**
   * @brief Loads catalog definitions from a YAML file.
   *
   * @param path YAML catalog path.
   * @param error_message Receives a human-readable error on failure.
   * @return true when the catalog was loaded and validated.
   */
  bool LoadFromFile(std::string_view path, std::string& error_message);

  /**
   * @brief Returns all catalog definitions.
   */
  [[nodiscard]] std::span<const SoundDef> All() const noexcept;

  /**
   * @brief Finds one catalog definition by sound id.
   *
   * @return Pointer to the definition, or nullptr if the id is unknown.
   */
  [[nodiscard]] const SoundDef* Find(SoundId id) const noexcept;

 private:
  std::vector<SoundDef> definitions_;
};
