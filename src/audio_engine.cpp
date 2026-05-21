#include "audio_engine.hpp"

#include <sstream>

#include "debug_log.hpp"

namespace {

/**
 * @brief Returns a display name for a sound action type.
 */
std::string SoundActionTypeName(SoundActionType type) {
  switch (type) {
    case SoundActionType::kPlay:
      return "play";
    case SoundActionType::kStop:
      return "stop";
  }

  return "unknown";
}

/**
 * @brief Returns a display name for a sound definition pointer.
 */
std::string SoundName(const SoundDef* def) {
  if (def == nullptr) {
    return "unknown";
  }

  return def->name;
}

/**
 * @brief Returns a display name for an event definition pointer.
 */
std::string EventName(const EventDef* event) {
  if (event == nullptr) {
    return "unknown";
  }

  return event->name;
}

}  // namespace

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

  if (!assets_.LoadCatalog(catalog_, error_message)) {
    return false;
  }

  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "catalog loaded: path=" << catalog_path
        << " sounds=" << catalog_.All().size()
        << " events=" << catalog_.Events().size();
    debug_log::Write(log.str());
  }

  return true;
}

bool AudioEngine::Play(SoundId id) {
  const SoundDef* def = FindSoundDef(id);
  if (def == nullptr) {
    debug_log::Write("play rejected: unknown sound id");
    return false;
  }

  const SoundBuffer* buffer = assets_.Find(id);
  if (buffer == nullptr) {
    std::ostringstream log;
    log << "play rejected: unloaded sound=" << SoundName(def);
    debug_log::Write(log.str());
    return false;
  }

  const bool queued = EnqueueControlCommand(ControlCommand{
      .type = ControlCommandType::kPlay,
      .id = id,
      .def = def,
      .buffer = buffer,
  });
  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "play request " << (queued ? "queued" : "rejected")
        << ": sound=" << SoundName(def) << " frames=" << buffer->FrameCount()
        << " loop=" << (def->loop ? "true" : "false") << " gain=" << def->gain;
    debug_log::Write(log.str());
  }
  return queued;
}

bool AudioEngine::PlayEvent(EventId id) {
  bool queued_any_action = false;
  bool queued_all_actions = true;
  for (const SoundAction& action : catalog_.ResolveEventActions(id)) {
    if (action.type != SoundActionType::kPlay) {
      continue;
    }

    queued_any_action = true;
    queued_all_actions = Play(action.sound_id) && queued_all_actions;
  }

  if (!queued_any_action) {
    return false;
  }

  return queued_all_actions;
}

void AudioEngine::Stop(SoundId id) {
  const SoundDef* def = FindSoundDef(id);
  const bool queued = EnqueueControlCommand(ControlCommand{
      .type = ControlCommandType::kStop,
      .id = id,
  });
  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "stop request " << (queued ? "queued" : "rejected")
        << ": sound=" << SoundName(def);
    debug_log::Write(log.str());
  }
}

void AudioEngine::StopEvent(EventId id) {
  for (const SoundAction& action : catalog_.ResolveEventActions(id)) {
    if (action.type == SoundActionType::kStop) {
      Stop(action.sound_id);
    }
  }
}

bool AudioEngine::ApplyEvent(EventId id) {
  const EventDef* event = FindEventDef(id);
  if (debug_log::Enabled()) {
    std::ostringstream log;
    log << "event received: event=" << EventName(event);
    if (event != nullptr) {
      log << " actions=" << event->actions.size();
    }
    debug_log::Write(log.str());
  }

  bool accepted_any_action = false;
  bool accepted_all_actions = true;
  for (const SoundAction& action : catalog_.ResolveEventActions(id)) {
    accepted_any_action = true;
    if (debug_log::Enabled()) {
      std::ostringstream log;
      log << "event action: event=" << EventName(event)
          << " type=" << SoundActionTypeName(action.type)
          << " sound=" << SoundName(FindSoundDef(action.sound_id));
      debug_log::Write(log.str());
    }

    if (action.type == SoundActionType::kPlay) {
      accepted_all_actions = Play(action.sound_id) && accepted_all_actions;
    } else if (action.type == SoundActionType::kStop) {
      Stop(action.sound_id);
    }
  }

  return accepted_any_action && accepted_all_actions;
}

void AudioEngine::Run(SoundControlQueue& control_queue) {
  SoundControlMessage message;
  while (control_queue.WaitAndPop(&message)) {
    switch (message.type) {
      case SoundControlMessageType::kNotify:
        static_cast<void>(ApplyEvent(message.event_id));
        break;
      case SoundControlMessageType::kShutdown:
        control_queue.Close();
        return;
    }
  }
}

const SoundDef* AudioEngine::FindSoundDef(SoundId id) const noexcept {
  return catalog_.Find(id);
}

const EventDef* AudioEngine::FindEventDef(EventId id) const noexcept {
  return catalog_.Find(id);
}

const SoundCatalog& AudioEngine::Catalog() const noexcept { return catalog_; }

void AudioEngine::Render(float* output, int frame_count) {
  DrainControlCommands();
  mixer_.Mix(output, frame_count);
}

bool AudioEngine::EnqueueControlCommand(const ControlCommand& command) {
  // Single-producer/single-consumer ring buffer: control calls produce
  // commands, and the render thread consumes them before mixing each audio
  // chunk.
  const std::size_t write_index =
      control_write_index_.load(std::memory_order_relaxed);
  const std::size_t next_write_index =
      (write_index + 1) % control_commands_.size();
  if (next_write_index == control_read_index_.load(std::memory_order_acquire)) {
    debug_log::Write("render-control queue full");
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
      const bool started = mixer_.Play(*command.def, *command.buffer);
      if (debug_log::Enabled()) {
        std::ostringstream log;
        log << "render command play " << (started ? "started" : "rejected")
            << ": sound=" << SoundName(command.def);
        debug_log::Write(log.str());
      }
    } else if (command.type == ControlCommandType::kStop) {
      mixer_.Stop(command.id);
      if (debug_log::Enabled()) {
        std::ostringstream log;
        log << "render command stop: sound="
            << SoundName(FindSoundDef(command.id));
        debug_log::Write(log.str());
      }
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
