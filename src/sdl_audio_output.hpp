#pragma once

#include <SDL.h>

#include <atomic>
#include <string>

#include "audio_types.hpp"

/**
 * @brief Interface implemented by components that can render audio frames.
 */
class AudioRenderTarget {
 public:
  virtual ~AudioRenderTarget() = default;

  /**
   * @brief Fills an interleaved stereo float buffer.
   */
  virtual void Render(float* output, int frame_count) = 0;
};

/**
 * @brief SDL2 callback-based audio output backend.
 *
 * SDL owns the audio device thread and invokes AudioCallback() whenever it
 * needs another block of stereo float samples. The callback forwards directly
 * to the AudioRenderTarget, which keeps the render path shared with other
 * backends.
 */
class SdlAudioOutput {
 public:
  SdlAudioOutput() = default;
  /** @brief Stops streaming and releases SDL audio resources. */
  ~SdlAudioOutput();

  SdlAudioOutput(const SdlAudioOutput&) = delete;
  SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;

  /**
   * @brief Opens the default SDL audio device and starts streaming.
   *
   * @param render_target Source of mixed stereo float frames.
   * @param error_message Receives a human-readable error on failure.
   * @return true when the SDL audio device is open and unpaused.
   */
  bool Initialize(AudioRenderTarget& render_target, std::string& error_message);

  /**
   * @brief Stops streaming and releases SDL audio resources.
   */
  void Shutdown();

 private:
  /**
   * @brief SDL C callback trampoline for streaming audio.
   */
  static void AudioCallback(void* user_data, Uint8* stream, int byte_count);

  SDL_AudioDeviceID device_id_ = 0;
  AudioRenderTarget* render_target_ = nullptr;
  std::atomic_uint64_t callback_count_ = 0;
  bool sdl_audio_initialized_ = false;
};
