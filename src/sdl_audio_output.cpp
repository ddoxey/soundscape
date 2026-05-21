#include "sdl_audio_output.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>

#include "debug_log.hpp"

namespace {

/**
 * @brief Limits callback logging to startup and powers of two.
 */
bool ShouldLogCallback(std::uint64_t callback_count) {
  return callback_count <= 5 || (callback_count & (callback_count - 1)) == 0;
}

}  // namespace

SdlAudioOutput::~SdlAudioOutput() { Shutdown(); }

bool SdlAudioOutput::Initialize(AudioRenderTarget& render_target,
                                std::string& error_message) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    error_message = "Failed to initialize SDL audio: ";
    error_message += SDL_GetError();
    return false;
  }

  sdl_audio_initialized_ = true;
  render_target_ = &render_target;

  SDL_AudioSpec desired{};
  desired.freq = kOutputSampleRate;
  desired.format = AUDIO_F32SYS;
  desired.channels = kOutputChannels;
  desired.samples = kBufferFrames;
  desired.callback = &SdlAudioOutput::AudioCallback;
  desired.userdata = this;

  SDL_AudioSpec obtained{};
  device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if (device_id_ == 0) {
    error_message = "Failed to open SDL audio device: ";
    error_message += SDL_GetError();
    Shutdown();
    return false;
  }

  if (obtained.freq != desired.freq || obtained.format != desired.format ||
      obtained.channels != desired.channels) {
    error_message =
        "SDL audio device did not provide the requested 48 kHz stereo float "
        "format";
    Shutdown();
    return false;
  }

  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "SDL audio opened: device_id=" << device_id_
        << " freq=" << obtained.freq
        << " channels=" << static_cast<int>(obtained.channels)
        << " samples=" << obtained.samples << " format=0x" << std::hex
        << obtained.format;
    debug_log::Write(log.str());
  }

  SDL_PauseAudioDevice(device_id_, 0);
  debug_log::Write("SDL audio device unpaused");
  return true;
}

void SdlAudioOutput::Shutdown() {
  if (device_id_ != 0) {
    debug_log::Write("SDL audio device closing");
    SDL_PauseAudioDevice(device_id_, 1);
    SDL_CloseAudioDevice(device_id_);
    device_id_ = 0;
  }

  render_target_ = nullptr;

  if (sdl_audio_initialized_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sdl_audio_initialized_ = false;
  }
}

void SdlAudioOutput::AudioCallback(void* user_data, Uint8* stream,
                                   int byte_count) {
  SdlAudioOutput* output = static_cast<SdlAudioOutput*>(user_data);
  if (output == nullptr || output->render_target_ == nullptr ||
      byte_count <= 0) {
    if (byte_count > 0) {
      std::memset(stream, 0, static_cast<std::size_t>(byte_count));
    }
    return;
  }

  const int frame_count =
      byte_count / static_cast<int>(sizeof(float) * kOutputChannels);
  const int rendered_byte_count =
      frame_count * static_cast<int>(sizeof(float) * kOutputChannels);
  const std::uint64_t callback_count =
      output->callback_count_.fetch_add(1, std::memory_order_relaxed) + 1;
  if (debug_log::Enabled() && ShouldLogCallback(callback_count)) {
    std::ostringstream log;
    log << "SDL audio callback count=" << callback_count
        << " byte_count=" << byte_count << " frame_count=" << frame_count;
    debug_log::Write(log.str());
  }

  float* samples = reinterpret_cast<float*>(stream);
  output->render_target_->Render(samples, frame_count);

  if (rendered_byte_count < byte_count) {
    std::memset(stream + rendered_byte_count, 0,
                static_cast<std::size_t>(byte_count - rendered_byte_count));
  }
}
