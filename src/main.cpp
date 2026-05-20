#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "audio_engine.hpp"

namespace {

struct ScheduledEvent {
  double at_seconds = 0.0;
  bool stop = false;
  SoundId id{};
  std::string_view note;
};

std::string_view SoundName(SoundId id) {
  for (const SoundDef& def : kSoundCatalog) {
    if (def.id == id) {
      return def.name;
    }
  }

  return "Unknown";
}

void PrintTimelineEvent(const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << event.at_seconds << "s] " << (event.stop ? "STOP " : "PLAY ")
            << SoundName(event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

void PrintTriggeredEvent(double elapsed_seconds, const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << elapsed_seconds << "s] " << (event.stop ? "STOP " : "PLAY ")
            << SoundName(event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

void PrintSuppressedEvent(double elapsed_seconds, const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << elapsed_seconds << "s] "
            << "DROP " << SoundName(event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

std::vector<ScheduledEvent> BuildTimeline() {
  return {
      {0.0, false, SoundId::kAmbient, "background loop"},
      {0.0, false, SoundId::kEngine, "background loop"},
      {0.0, false, SoundId::kThrust1, "background texture loop"},
      {0.0, false, SoundId::kAirportTower, "tower chatter bed"},
      {6.0, false, SoundId::kAirplanePing, "normal event; no ducking"},
      {10.0, false, SoundId::kAutopilotDisconnectModern,
       "alert; ducks background and normal sounds"},
      {14.0, false, SoundId::kMasterCautionSingle,
       "alert; stacks with other alerts"},
      {18.0, false, SoundId::kMasterCautionLoop, "sustained alert loop"},
      {22.0, false, SoundId::kTerrainPullUpGws,
       "critical; ducks alerts to 70%, normal to 50%, background to 35%"},
      {22.0, false, SoundId::kAutopilotDisconnectModernVariant,
       "suppressed because a critical warning is already active"},
      {30.0, false, SoundId::kWindshear,
       "critical warning; equal priority with pull-up, no mutual ducking"},
      {38.0, true, SoundId::kMasterCautionLoop, "clear sustained alert loop"},
      {42.0, false, SoundId::kFlightAttendantSingle,
       "normal event; should stay under any active warnings"},
      {46.0, false, SoundId::kBingoLowFuelF16, "alert event"},
      {50.0, false, SoundId::kAutopilotDisconnectClassicBoeing,
       "alert reprise with alternate source"},
      {54.0, false, SoundId::kBankAngle, "critical pulse"},
      {57.0, false, SoundId::kOverspeedClacker,
       "critical clacker near program end"},
  };
}

double ParseDuration(int argc, char** argv) {
  if (argc < 2) {
    return 60.0;
  }

  const double duration = std::strtod(argv[1], nullptr);
  if (duration <= 0.0) {
    return 60.0;
  }

  return duration;
}

}  // namespace

int main(int argc, char** argv) {
  const double duration_seconds = ParseDuration(argc, argv);
  std::string error_message;

  AudioEngine engine;
  if (!engine.Initialize(error_message)) {
    std::cerr
        << "Audio initialization failed: " << error_message
        << "\nSet SDL_AUDIODRIVER=dummy to run in a headless environment.\n";
    return 1;
  }

  if (!engine.LoadCatalog(error_message)) {
    std::cerr << "Catalog load failed: " << error_message << '\n';
    return 1;
  }

  std::vector<ScheduledEvent> timeline = BuildTimeline();
  std::erase_if(timeline, [duration_seconds](const ScheduledEvent& event) {
    return event.at_seconds > duration_seconds;
  });

  std::cout << "Cockpit soundscape demo running for " << duration_seconds
            << " seconds.\n";
  std::cout << "Scheduled events:\n";
  for (const ScheduledEvent& event : timeline) {
    PrintTimelineEvent(event);
  }
  std::cout << "----\nTriggering events:\n";

  const auto start = std::chrono::steady_clock::now();
  std::size_t next_event_index = 0;

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_seconds =
        std::chrono::duration<double>(now - start).count();

    while (next_event_index < timeline.size() &&
           timeline[next_event_index].at_seconds <= elapsed_seconds) {
      const ScheduledEvent& event = timeline[next_event_index];
      if (event.stop) {
        engine.Stop(event.id);
        PrintTriggeredEvent(elapsed_seconds, event);
      } else if (engine.Play(event.id)) {
        PrintTriggeredEvent(elapsed_seconds, event);
      } else {
        PrintSuppressedEvent(elapsed_seconds, event);
      }
      ++next_event_index;
    }

    if (elapsed_seconds >= duration_seconds) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  engine.Stop(SoundId::kAmbient);
  engine.Stop(SoundId::kEngine);
  engine.Stop(SoundId::kAirportTower);
  engine.Stop(SoundId::kThrust1);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  engine.Shutdown();

  std::cout << "Demo complete.\n";
  return 0;
}
