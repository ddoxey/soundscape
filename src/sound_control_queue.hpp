#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

#include "sound_catalog.hpp"

/**
 * @brief Stable host-facing event id mapped through the aircraft catalog.
 */
using SoundEventId = EventId;

/**
 * @brief Host-facing event notification type for the mixer runtime loop.
 */
enum class SoundControlMessageType { kNotify, kShutdown };

/**
 * @brief One host-facing event notification consumed by AudioEngine::Run().
 */
struct SoundControlMessage {
  SoundControlMessageType type = SoundControlMessageType::kShutdown;
  SoundEventId event_id{};
};

/**
 * @brief Thread-safe bounded queue for application-to-mixer event messages.
 *
 * This queue is intended for the calling application's control path, not for
 * the audio render path. Producers can use TryPush() when they must avoid
 * blocking, while AudioEngine::Run() blocks in WaitAndPop() until work arrives
 * or the queue is closed.
 */
class SoundControlQueue {
 public:
  /**
   * @brief Creates a bounded queue with the requested capacity.
   */
  explicit SoundControlQueue(std::size_t capacity = kDefaultCapacity);

  SoundControlQueue(const SoundControlQueue&) = delete;
  SoundControlQueue& operator=(const SoundControlQueue&) = delete;

  /**
   * @brief Attempts to enqueue a message without blocking.
   *
   * @return false when the queue is closed or already at capacity.
   */
  [[nodiscard]] bool TryPush(const SoundControlMessage& message);

  /**
   * @brief Closes the queue and wakes any waiting consumer.
   */
  void Close();

  /**
   * @brief Waits for one message or for the queue to close.
   *
   * @return false when the queue is closed, no messages remain, or message is
   * nullptr.
   */
  [[nodiscard]] bool WaitAndPop(SoundControlMessage* message);

  /**
   * @brief Default maximum number of pending host event messages.
   */
  static constexpr std::size_t kDefaultCapacity = 128;

 private:
  const std::size_t capacity_;
  std::mutex mutex_;
  std::condition_variable message_available_;
  std::deque<SoundControlMessage> messages_;
  bool closed_ = false;
};
