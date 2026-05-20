#include "sound_control_queue.hpp"

SoundControlQueue::SoundControlQueue(std::size_t capacity)
    : capacity_(capacity) {}

bool SoundControlQueue::TryPush(const SoundControlMessage& message) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_ || messages_.size() >= capacity_) {
      return false;
    }

    messages_.push_back(message);
  }

  message_available_.notify_one();
  return true;
}

void SoundControlQueue::Close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }

  message_available_.notify_all();
}

bool SoundControlQueue::WaitAndPop(SoundControlMessage* message) {
  if (message == nullptr) {
    return false;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  message_available_.wait(lock,
                          [this] { return closed_ || !messages_.empty(); });

  if (messages_.empty()) {
    return false;
  }

  *message = messages_.front();
  messages_.pop_front();
  return true;
}
