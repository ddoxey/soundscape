#include "audio_engine.hpp"

AudioEngine::~AudioEngine() { Shutdown(); }

bool AudioEngine::Initialize(std::string& error_message) {
  return output_.Initialize(*this, error_message);
}

void AudioEngine::Shutdown() {
  output_.Shutdown();
  mixer_.Clear();
  assets_.Clear();
}

bool AudioEngine::LoadCatalog(std::string_view catalog_path,
                              std::string& error_message) {
  if (!catalog_.LoadFromFile(catalog_path, error_message)) {
    return false;
  }

  return assets_.LoadCatalog(catalog_, error_message);
}

bool AudioEngine::Play(SoundId id) {
  const SoundDef* def = FindSoundDef(id);
  if (def == nullptr) {
    return false;
  }

  const SoundBuffer* buffer = assets_.Find(id);
  if (buffer == nullptr) {
    return false;
  }

  bool started = false;
  output_.WithDeviceLock([&] { started = mixer_.Play(*def, *buffer); });
  return started;
}

void AudioEngine::Stop(SoundId id) {
  output_.WithDeviceLock([&] { mixer_.Stop(id); });
}

const SoundDef* AudioEngine::FindSoundDef(SoundId id) const noexcept {
  return catalog_.Find(id);
}

void AudioEngine::Render(float* output, int frame_count) {
  mixer_.Mix(output, frame_count);
}
