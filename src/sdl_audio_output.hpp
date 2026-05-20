#pragma once

#include <SDL.h>

#include <string>

#include "audio_types.hpp"

class AudioRenderTarget {
 public:
  virtual ~AudioRenderTarget() = default;
  virtual void Render(float* output, int frame_count) = 0;
};

class SdlAudioOutput {
 public:
  SdlAudioOutput() = default;
  ~SdlAudioOutput();

  SdlAudioOutput(const SdlAudioOutput&) = delete;
  SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;

  bool Initialize(AudioRenderTarget& render_target, std::string& error_message);
  void Shutdown();

  template <typename Fn>
  void WithDeviceLock(const Fn& fn) {
    if (device_id_ == 0) {
      return;
    }

    SDL_LockAudioDevice(device_id_);
    fn();
    SDL_UnlockAudioDevice(device_id_);
  }

 private:
  static void AudioCallback(void* userdata, Uint8* stream, int len);

  SDL_AudioDeviceID device_id_ = 0;
  SDL_AudioSpec obtained_spec_{};
  AudioRenderTarget* render_target_ = nullptr;
};
