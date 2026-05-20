#pragma once

#include <string>
#include <string_view>

#include "audio_asset_manager.hpp"
#include "mixer.hpp"
#include "openal_audio_output.hpp"
#include "sound_catalog_runtime.hpp"

/**
 * @brief Public facade for the cockpit soundscape runtime.
 *
 * AudioEngine wires together catalog lookup, asset loading, policy-aware
 * mixing, and the OpenAL output backend. The demo runner uses this class rather
 * than reaching into the lower-level components directly.
 */
class AudioEngine : public AudioRenderTarget {
 public:
  AudioEngine() = default;
  /** @brief Stops playback and releases runtime resources. */
  ~AudioEngine();

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  /**
   * @brief Initializes the audio output backend.
   *
   * @param error_message Receives a human-readable error on failure.
   * @return true when an output device and streaming context are ready.
   */
  bool Initialize(std::string& error_message);

  /**
   * @brief Stops playback and releases loaded assets and output resources.
   */
  void Shutdown();

  /**
   * @brief Loads all sounds from the configured catalog into memory.
   *
   * @param catalog_path YAML catalog path.
   * @param error_message Receives a human-readable error on failure.
   * @return true when every catalog entry was loaded successfully.
   */
  bool LoadCatalog(std::string_view catalog_path, std::string& error_message);

  /**
   * @brief Requests playback of a catalog sound.
   *
   * @return true if playback started or an existing loop was refreshed; false
   * if the sound was unknown, unloaded, or suppressed by policy.
   */
  [[nodiscard]] bool Play(SoundId id);

  /**
   * @brief Stops all active instances of a catalog sound.
   */
  void Stop(SoundId id);

  /**
   * @brief Finds the loaded catalog definition for a sound id.
   */
  [[nodiscard]] const SoundDef* FindSoundDef(SoundId id) const noexcept;

  /**
   * @brief Returns the loaded catalog.
   */
  [[nodiscard]] const SoundCatalog& Catalog() const noexcept;

 private:
  /**
   * @brief AudioRenderTarget implementation called by the output backend.
   */
  void Render(float* output, int frame_count) override;

  SoundCatalog catalog_;
  AudioAssetManager assets_;
  Mixer mixer_;
  OpenAlAudioOutput output_;
};
