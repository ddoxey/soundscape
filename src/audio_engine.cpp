#include "audio_engine.hpp"

AudioEngine::~AudioEngine() { Shutdown(); }

bool AudioEngine::Initialize(std::string& error_message) {
  return output_.Initialize(*this, error_message);
}

void AudioEngine::Shutdown() {
  output_.Shutdown();
  ClearControlCommands();
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

  return EnqueueControlCommand(ControlCommand{
      .type = ControlCommandType::kPlay,
      .id = id,
      .def = def,
      .buffer = buffer,
  });
}

void AudioEngine::Stop(SoundId id) {
  static_cast<void>(EnqueueControlCommand(ControlCommand{
      .type = ControlCommandType::kStop,
      .id = id,
  }));
}

const SoundDef* AudioEngine::FindSoundDef(SoundId id) const noexcept {
  return catalog_.Find(id);
}

const SoundCatalog& AudioEngine::Catalog() const noexcept { return catalog_; }

void AudioEngine::Render(float* output, int frame_count) {
  DrainControlCommands();
  mixer_.Mix(output, frame_count);
}

bool AudioEngine::EnqueueControlCommand(const ControlCommand& command) {
  // Single-producer/single-consumer ring buffer: control calls produce
  // commands, and the render worker consumes them before mixing each audio
  // chunk.
  const std::size_t write_index =
      control_write_index_.load(std::memory_order_relaxed);
  const std::size_t next_write_index =
      (write_index + 1) % control_commands_.size();
  if (next_write_index == control_read_index_.load(std::memory_order_acquire)) {
    return false;
  }

  control_commands_[write_index] = command;
  control_write_index_.store(next_write_index, std::memory_order_release);
  return true;
}

void AudioEngine::DrainControlCommands() {
  std::size_t read_index = control_read_index_.load(std::memory_order_relaxed);
  const std::size_t write_index =
      control_write_index_.load(std::memory_order_acquire);

  while (read_index != write_index) {
    const ControlCommand& command = control_commands_[read_index];
    if (command.type == ControlCommandType::kPlay && command.def != nullptr &&
        command.buffer != nullptr) {
      static_cast<void>(mixer_.Play(*command.def, *command.buffer));
    } else if (command.type == ControlCommandType::kStop) {
      mixer_.Stop(command.id);
    }

    read_index = (read_index + 1) % control_commands_.size();
  }

  control_read_index_.store(read_index, std::memory_order_release);
}

void AudioEngine::ClearControlCommands() {
  const std::size_t write_index =
      control_write_index_.load(std::memory_order_relaxed);
  control_read_index_.store(write_index, std::memory_order_relaxed);
}
