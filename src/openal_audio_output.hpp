#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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
 * @brief OpenAL Soft streaming output backend.
 *
 * OpenAL does not call directly into the mixer like SDL did. Instead, this
 * backend runs a worker thread that asks an AudioRenderTarget for chunks of
 * audio, converts them to stereo PCM, and queues them on an OpenAL source.
 */
class OpenAlAudioOutput {
 public:
  OpenAlAudioOutput() = default;
  /** @brief Stops streaming and releases OpenAL resources. */
  ~OpenAlAudioOutput();

  OpenAlAudioOutput(const OpenAlAudioOutput&) = delete;
  OpenAlAudioOutput& operator=(const OpenAlAudioOutput&) = delete;

  /**
   * @brief Opens the default OpenAL device and starts streaming.
   *
   * @param render_target Source of mixed stereo float frames.
   * @param error_message Receives a human-readable error on failure.
   * @return true when the OpenAL source is playing.
   */
  bool Initialize(AudioRenderTarget& render_target, std::string& error_message);

  /**
   * @brief Stops streaming and releases OpenAL resources.
   */
  void Shutdown();

  /**
   * @brief Serializes modifications that can race with the streaming worker.
   */
  template <typename Fn>
  void WithDeviceLock(const Fn& fn) {
    const std::lock_guard<std::mutex> lock(render_mutex_);
    fn();
  }

 private:
  static constexpr int kQueuedBufferCount = 4;
  static constexpr int kBufferFramesPerChunk = kBufferFrames;

  /**
   * @brief Refills processed OpenAL buffers until shutdown is requested.
   */
  void WorkerLoop();

  /**
   * @brief Renders one chunk and uploads it into an OpenAL buffer.
   */
  bool FillBuffer(ALuint buffer_id, std::string& error_message);

  /**
   * @brief Builds an error message for an OpenAL API error code.
   */
  [[nodiscard]] std::string BuildAlError(std::string_view operation,
                                         ALenum error) const;

  /**
   * @brief Builds an error message for an OpenAL context/device error.
   */
  [[nodiscard]] std::string BuildAlcError(std::string_view operation) const;

  ALCdevice* device_ = nullptr;
  ALCcontext* context_ = nullptr;
  ALuint source_ = 0;
  std::array<ALuint, kQueuedBufferCount> buffers_{};
  std::vector<float> scratch_buffer_;
  std::vector<std::int16_t> pcm_buffer_;
  AudioRenderTarget* render_target_ = nullptr;
  std::mutex render_mutex_;
  std::thread worker_thread_;
  std::atomic_bool running_ = false;
};
