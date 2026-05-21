#pragma once

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string_view>

/**
 * @brief Debug logging helpers enabled by SOUNDSCAPE_DEBUG.
 */
namespace debug_log {

/**
 * @brief Returns true when diagnostic logging is enabled.
 */
inline bool Enabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("SOUNDSCAPE_DEBUG");
    if (value == nullptr) {
      return false;
    }

    const std::string_view flag(value);
    return !flag.empty() && flag != "0";
  }();
  return enabled;
}

/**
 * @brief Writes one diagnostic log line when SOUNDSCAPE_DEBUG is enabled.
 */
inline void Write(std::string_view message) {
  if (!Enabled()) {
    return;
  }

  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  std::cerr << "[soundscape] " << message << '\n';
}

}  // namespace debug_log
