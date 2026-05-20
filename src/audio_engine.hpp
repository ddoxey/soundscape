#pragma once

#include <string>

#include "audio_asset_manager.hpp"
#include "mixer.hpp"
#include "sdl_audio_output.hpp"
#include "sound_catalog_runtime.hpp"

class AudioEngine : public AudioRenderTarget {
 public:
  AudioEngine() = default;
  ~AudioEngine();

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  bool Initialize(std::string& error_message);
  void Shutdown();

  bool LoadCatalog(std::string& error_message);

  [[nodiscard]] bool Play(SoundId id);
  void Stop(SoundId id);

  [[nodiscard]] const SoundDef* FindSoundDef(SoundId id) const noexcept;

 private:
  void Render(float* output, int frame_count) override;

  SoundCatalog catalog_;
  AudioAssetManager assets_;
  Mixer mixer_;
  SdlAudioOutput output_;
};
