#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio_types.hpp"
#include "sound_catalog_runtime.hpp"

/**
 * @brief Loads, decodes, and stores audio assets for playback.
 *
 * This component is responsible for file I/O and format normalization. The
 * mixer receives only ready-to-mix SoundBuffer objects and does not know about
 * WAV files or libsndfile.
 */
class AudioAssetManager {
 public:
  /**
   * @brief Loads every sound described by the catalog.
   *
   * @param catalog Catalog entries to load.
   * @param error_message Receives a human-readable error on failure.
   * @return true when every asset was loaded and converted successfully.
   */
  bool LoadCatalog(const SoundCatalog& catalog, std::string& error_message);

  /**
   * @brief Releases all decoded audio buffers.
   */
  void Clear();

  /**
   * @brief Looks up a decoded buffer by sound id.
   *
   * @return Pointer to the loaded buffer, or nullptr if it is not loaded.
   */
  [[nodiscard]] const SoundBuffer* Find(SoundId id) const noexcept;

 private:
  /**
   * @brief Loads and converts one catalog entry.
   */
  bool LoadSound(const SoundDef& def, std::string& error_message);

  /**
   * @brief Converts decoded samples into the engine's 48 kHz stereo format.
   */
  [[nodiscard]] SoundBuffer ResampleToInternalFormat(
      const std::vector<float>& source_samples, int source_channels,
      int source_sample_rate) const;

  std::unordered_map<SoundId, SoundBuffer> buffers_;
};
