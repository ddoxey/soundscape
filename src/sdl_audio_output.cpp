#include "sdl_audio_output.hpp"

#include <cstddef>
#include <cstring>

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

  SDL_PauseAudioDevice(device_id_, 0);
  return true;
}

void SdlAudioOutput::Shutdown() {
  if (device_id_ != 0) {
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
  float* samples = reinterpret_cast<float*>(stream);
  output->render_target_->Render(samples, frame_count);

  if (rendered_byte_count < byte_count) {
    std::memset(stream + rendered_byte_count, 0,
                static_cast<std::size_t>(byte_count - rendered_byte_count));
  }
}
