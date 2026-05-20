#include "sdl_audio_output.hpp"

#include <cstring>

SdlAudioOutput::~SdlAudioOutput() { Shutdown(); }

bool SdlAudioOutput::Initialize(AudioRenderTarget& render_target,
                                std::string& error_message) {
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    error_message = SDL_GetError();
    return false;
  }

  render_target_ = &render_target;

  SDL_AudioSpec desired_spec{};
  desired_spec.freq = kOutputSampleRate;
  desired_spec.format = AUDIO_F32SYS;
  desired_spec.channels = kOutputChannels;
  desired_spec.samples = kBufferFrames;
  desired_spec.callback = &SdlAudioOutput::AudioCallback;
  desired_spec.userdata = this;

  device_id_ =
      SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &obtained_spec_, 0);
  if (device_id_ == 0) {
    error_message = SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    render_target_ = nullptr;
    return false;
  }

  if (obtained_spec_.freq != desired_spec.freq ||
      obtained_spec_.format != desired_spec.format ||
      obtained_spec_.channels != desired_spec.channels) {
    error_message = "Audio device did not provide 48 kHz stereo float output";
    Shutdown();
    return false;
  }

  SDL_PauseAudioDevice(device_id_, 0);
  return true;
}

void SdlAudioOutput::Shutdown() {
  if (device_id_ != 0) {
    SDL_CloseAudioDevice(device_id_);
    device_id_ = 0;
  }

  render_target_ = nullptr;

  if (SDL_WasInit(SDL_INIT_AUDIO) != 0U) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

void SdlAudioOutput::AudioCallback(void* userdata, Uint8* stream, int len) {
  auto* output = static_cast<SdlAudioOutput*>(userdata);
  std::memset(stream, 0, static_cast<std::size_t>(len));

  if (output == nullptr || output->render_target_ == nullptr) {
    return;
  }

  output->render_target_->Render(
      reinterpret_cast<float*>(stream),
      len / static_cast<int>(sizeof(float) * kOutputChannels));
}
