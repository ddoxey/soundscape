#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "audio_engine.hpp"
#include "sound_catalog_runtime.hpp"

namespace {

constexpr std::string_view kDefaultCatalogPath = "conf/mock1.yaml";
constexpr std::string_view kDefaultScriptPath = "conf/script.yml";

/**
 * @brief One event loaded from a demo script file.
 */
struct ScheduledEvent {
  double at_seconds = 0.0;
  bool stop = false;
  SoundId id{};
  std::string note;
};

std::string_view SoundName(const AudioEngine& engine, SoundId id) {
  const SoundDef* def = engine.FindSoundDef(id);
  if (def != nullptr) {
    return std::string_view(def->name);
  }

  return "Unknown";
}

void PrintTimelineEvent(const AudioEngine& engine,
                        const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << event.at_seconds << "s] " << (event.stop ? "STOP " : "PLAY ")
            << SoundName(engine, event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

void PrintTriggeredEvent(const AudioEngine& engine, double elapsed_seconds,
                         const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << elapsed_seconds << "s] " << (event.stop ? "STOP " : "PLAY ")
            << SoundName(engine, event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

void PrintRejectedEvent(const AudioEngine& engine, double elapsed_seconds,
                        const ScheduledEvent& event) {
  std::cout << std::fixed << std::setprecision(1) << "[" << std::setw(5)
            << elapsed_seconds << "s] "
            << "REJECT " << SoundName(engine, event.id);
  if (!event.note.empty()) {
    std::cout << "  " << event.note;
  }
  std::cout << '\n';
}

/**
 * @brief Parsed command-line options for one demo run.
 */
struct RuntimeOptions {
  double duration_seconds = 60.0;
  std::string catalog_path = std::string(kDefaultCatalogPath);
  std::string script_path = std::string(kDefaultScriptPath);
  bool validate_only = false;
  bool parse_error = false;
  std::string parse_error_message;
};

/**
 * @brief Parses a positive floating-point duration.
 */
bool TryParsePositiveDouble(std::string_view text, double& value) {
  std::string owned_text(text);
  char* parse_end = nullptr;
  const double parsed_value = std::strtod(owned_text.c_str(), &parse_end);
  if (parse_end == owned_text.c_str() || *parse_end != '\0' ||
      parsed_value <= 0.0) {
    return false;
  }

  value = parsed_value;
  return true;
}

/**
 * @brief Parses command-line options while preserving legacy positional usage.
 */
RuntimeOptions ParseOptions(int argc, char** argv) {
  RuntimeOptions options;
  if (argc < 2) {
    return options;
  }

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--validate") {
      options.validate_only = true;
      continue;
    }

    if (argument == "--config") {
      if (index + 1 >= argc) {
        options.parse_error = true;
        options.parse_error_message = "--config requires a catalog path";
        return options;
      }

      options.catalog_path = argv[++index];
      continue;
    }

    if (argument == "--script") {
      if (index + 1 >= argc) {
        options.parse_error = true;
        options.parse_error_message = "--script requires a script path";
        return options;
      }

      options.script_path = argv[++index];
      continue;
    }

    if (argument.starts_with("--")) {
      options.parse_error = true;
      options.parse_error_message =
          "Unknown option '" + std::string(argument) + "'";
      return options;
    }

    double parsed_duration = 0.0;
    if (TryParsePositiveDouble(argument, parsed_duration)) {
      options.duration_seconds = parsed_duration;
    } else {
      options.catalog_path = argument;
    }
  }

  return options;
}

/**
 * @brief Returns a required YAML script field or throws a script-specific
 * error.
 */
YAML::Node RequireScriptField(const YAML::Node& node, std::string_view field,
                              std::size_t index) {
  const YAML::Node value = node[std::string(field)];
  if (!value) {
    throw std::runtime_error("script event " + std::to_string(index) +
                             " is missing required field '" +
                             std::string(field) + "'");
  }

  return value;
}

/**
 * @brief Converts one YAML script event into the runtime event type.
 */
ScheduledEvent ParseScriptEvent(const YAML::Node& node,
                                const SoundCatalog& catalog,
                                std::size_t index) {
  const std::string action =
      RequireScriptField(node, "action", index).as<std::string>();
  if (action != "play" && action != "stop") {
    throw std::runtime_error("script event " + std::to_string(index) +
                             " has unknown action '" + action + "'");
  }

  const std::string sound =
      RequireScriptField(node, "sound", index).as<std::string>();
  SoundId sound_id{};
  if (!TryParseSoundId(sound, sound_id)) {
    throw std::runtime_error("script event " + std::to_string(index) +
                             " has unknown sound id '" + sound + "'");
  }

  if (catalog.Find(sound_id) == nullptr) {
    throw std::runtime_error("script event " + std::to_string(index) +
                             " references sound id '" + sound +
                             "' that is not present in the selected catalog");
  }

  return ScheduledEvent{
      .at_seconds = RequireScriptField(node, "at_seconds", index).as<double>(),
      .stop = action == "stop",
      .id = sound_id,
      .note = node["note"] ? node["note"].as<std::string>() : std::string(),
  };
}

/**
 * @brief Loads and validates the demo timeline script.
 */
bool LoadScript(std::string_view script_path, const SoundCatalog& catalog,
                std::vector<ScheduledEvent>& timeline,
                std::string& error_message) {
  try {
    const YAML::Node root = YAML::LoadFile(std::string(script_path));
    const YAML::Node events = root["events"];
    if (!events || !events.IsSequence()) {
      error_message = "Script must contain a top-level 'events' sequence";
      return false;
    }

    std::vector<ScheduledEvent> loaded_timeline;
    loaded_timeline.reserve(events.size());
    for (std::size_t index = 0; index < events.size(); ++index) {
      loaded_timeline.push_back(
          ParseScriptEvent(events[index], catalog, index));
    }

    // Preserve file order for equal-time events so scripted suppression demos
    // can rely on one event being processed before another at the same time.
    std::stable_sort(loaded_timeline.begin(), loaded_timeline.end(),
                     [](const ScheduledEvent& lhs, const ScheduledEvent& rhs) {
                       return lhs.at_seconds < rhs.at_seconds;
                     });

    timeline = std::move(loaded_timeline);
    return true;
  } catch (const std::exception& error) {
    error_message =
        "Failed to load " + std::string(script_path) + ": " + error.what();
    return false;
  }
}

/**
 * @brief Validates the selected catalog and script without starting audio.
 */
int ValidateConfiguration(const RuntimeOptions& options) {
  std::string error_message;
  SoundCatalog catalog;
  if (!catalog.LoadFromFile(options.catalog_path, error_message)) {
    std::cerr << "Catalog validation failed: " << error_message << '\n';
    return 1;
  }

  std::vector<ScheduledEvent> timeline;
  if (!LoadScript(options.script_path, catalog, timeline, error_message)) {
    std::cerr << "Script validation failed: " << error_message << '\n';
    return 1;
  }

  std::cout << "Catalog validation succeeded for " << options.catalog_path
            << " (" << catalog.All().size() << " sounds).\n";
  std::cout << "Script validation succeeded for " << options.script_path << " ("
            << timeline.size() << " events).\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const RuntimeOptions options = ParseOptions(argc, argv);
  if (options.parse_error) {
    std::cerr << "Argument error: " << options.parse_error_message << '\n';
    return 1;
  }

  if (options.validate_only) {
    return ValidateConfiguration(options);
  }

  std::string error_message;

  AudioEngine engine;
  if (!engine.LoadCatalog(options.catalog_path, error_message)) {
    std::cerr << "Catalog load failed: " << error_message << '\n';
    return 1;
  }

  std::vector<ScheduledEvent> timeline;
  if (!LoadScript(options.script_path, engine.Catalog(), timeline,
                  error_message)) {
    std::cerr << "Script load failed: " << error_message << '\n';
    return 1;
  }

  if (!engine.Initialize(error_message)) {
    std::cerr
        << "Audio initialization failed: " << error_message
        << "\nSet ALSOFT_DRIVERS=null to run OpenAL Soft without a real audio "
           "device.\n";
    return 1;
  }

  std::erase_if(timeline, [&options](const ScheduledEvent& event) {
    return event.at_seconds > options.duration_seconds;
  });

  std::cout << "Cockpit soundscape demo using catalog " << options.catalog_path
            << ".\n";
  std::cout << "Cockpit soundscape demo using script " << options.script_path
            << ".\n";
  std::cout << "Cockpit soundscape demo running for "
            << options.duration_seconds << " seconds.\n";
  std::cout << "Scheduled events:\n";
  for (const ScheduledEvent& event : timeline) {
    PrintTimelineEvent(engine, event);
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
        PrintTriggeredEvent(engine, elapsed_seconds, event);
      } else if (engine.Play(event.id)) {
        PrintTriggeredEvent(engine, elapsed_seconds, event);
      } else {
        PrintRejectedEvent(engine, elapsed_seconds, event);
      }
      ++next_event_index;
    }

    if (elapsed_seconds >= options.duration_seconds) {
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
