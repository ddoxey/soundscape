#include "sound_catalog_runtime.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

/**
 * @brief Maps external YAML sound keys to stable internal sound ids.
 */
const std::unordered_map<std::string, SoundId>& SoundIdByName() {
  static const std::unordered_map<std::string, SoundId> ids{
      {"airplane_ping", SoundId::kAirplanePing},
      {"airport_tower", SoundId::kAirportTower},
      {"ambient", SoundId::kAmbient},
      {"autopilot_disconnect_classic_boeing",
       SoundId::kAutopilotDisconnectClassicBoeing},
      {"autopilot_disconnect_modern_variant",
       SoundId::kAutopilotDisconnectModernVariant},
      {"autopilot_disconnect_modern", SoundId::kAutopilotDisconnectModern},
      {"master_caution_loop", SoundId::kMasterCautionLoop},
      {"master_caution_single", SoundId::kMasterCautionSingle},
      {"bank_angle", SoundId::kBankAngle},
      {"bingo_low_fuel_f16", SoundId::kBingoLowFuelF16},
      {"configuration_warning_classic_boeing",
       SoundId::kConfigurationWarningClassicBoeing},
      {"engine", SoundId::kEngine},
      {"fire_bell_modern_757_767", SoundId::kFireBellModern757767},
      {"fire_bell_classic", SoundId::kFireBellClassic},
      {"flight_attendant_single", SoundId::kFlightAttendantSingle},
      {"overspeed_clacker", SoundId::kOverspeedClacker},
      {"terrain_pull_up_gws", SoundId::kTerrainPullUpGws},
      {"thrust1", SoundId::kThrust1},
      {"windshear", SoundId::kWindshear},
  };
  return ids;
}

/**
 * @brief Maps external YAML event keys to stable internal event ids.
 */
const std::unordered_map<std::string, EventId>& EventIdByName() {
  static const std::unordered_map<std::string, EventId> ids{
      {"ambient_bed", EventId::kAmbientBed},
      {"ambient_bed_cleared", EventId::kAmbientBedCleared},
      {"engine_bed", EventId::kEngineBed},
      {"engine_bed_cleared", EventId::kEngineBedCleared},
      {"thrust_texture", EventId::kThrustTexture},
      {"thrust_texture_cleared", EventId::kThrustTextureCleared},
      {"tower_chatter", EventId::kTowerChatter},
      {"tower_chatter_cleared", EventId::kTowerChatterCleared},
      {"airplane_ping_requested", EventId::kAirplanePingRequested},
      {"autopilot_disconnect", EventId::kAutopilotDisconnect},
      {"master_caution_single", EventId::kMasterCautionSingle},
      {"master_caution_loop", EventId::kMasterCautionLoop},
      {"terrain_pull_up", EventId::kTerrainPullUp},
      {"autopilot_disconnect_suppression_demo",
       EventId::kAutopilotDisconnectSuppressionDemo},
      {"windshear", EventId::kWindshear},
      {"master_caution_cleared", EventId::kMasterCautionCleared},
      {"flight_attendant_chime", EventId::kFlightAttendantChime},
      {"bingo_low_fuel", EventId::kBingoLowFuel},
      {"autopilot_disconnect_classic", EventId::kAutopilotDisconnectClassic},
      {"bank_angle", EventId::kBankAngle},
      {"overspeed", EventId::kOverspeed},
      {"loss_of_cabin_pressure", EventId::kLossOfCabinPressure},
      {"improper_takeoff_configuration",
       EventId::kImproperTakeoffConfiguration},
  };
  return ids;
}

/**
 * @brief Maps external YAML priority keys to internal priority values.
 */
const std::unordered_map<std::string, SoundPriority>& PriorityByName() {
  static const std::unordered_map<std::string, SoundPriority> priorities{
      {"background", SoundPriority::kBackground},
      {"normal", SoundPriority::kNormal},
      {"alert", SoundPriority::kAlert},
      {"critical", SoundPriority::kCritical},
  };
  return priorities;
}

/**
 * @brief Returns a required catalog YAML field or throws a catalog error.
 */
const YAML::Node RequireField(const YAML::Node& node, std::string_view field,
                              std::size_t index) {
  const YAML::Node value = node[std::string(field)];
  if (!value) {
    throw std::runtime_error("catalog entry " + std::to_string(index) +
                             " is missing required field '" +
                             std::string(field) + "'");
  }

  return value;
}

/**
 * @brief Parses a required catalog sound id.
 */
SoundId ParseSoundId(const std::string& id, std::size_t index) {
  SoundId sound_id{};
  if (!TryParseSoundId(id, &sound_id)) {
    throw std::runtime_error("catalog entry " + std::to_string(index) +
                             " has unknown id '" + id + "'");
  }

  return sound_id;
}

/**
 * @brief Parses a required catalog sound action type.
 */
SoundActionType ParseSoundActionType(const std::string& type,
                                     std::size_t index) {
  if (type == "play") {
    return SoundActionType::kPlay;
  }

  if (type == "stop") {
    return SoundActionType::kStop;
  }

  throw std::runtime_error("event action " + std::to_string(index) +
                           " has unknown type '" + type + "'");
}

/**
 * @brief Parses a required catalog event id.
 */
EventId ParseEventId(const std::string& id, std::size_t index) {
  EventId event_id{};
  if (!TryParseEventId(id, &event_id)) {
    throw std::runtime_error("event entry " + std::to_string(index) +
                             " has unknown id '" + id + "'");
  }

  return event_id;
}

/**
 * @brief Converts one YAML action entry into a SoundAction.
 */
SoundAction ParseSoundAction(const YAML::Node& node, std::size_t index) {
  const std::string type = RequireField(node, "type", index).as<std::string>();
  const std::string sound =
      RequireField(node, "sound", index).as<std::string>();

  return SoundAction{
      .type = ParseSoundActionType(type, index),
      .sound_id = ParseSoundId(sound, index),
  };
}

/**
 * @brief Parses a required catalog priority value.
 */
SoundPriority ParsePriority(const std::string& priority, std::size_t index) {
  const auto it = PriorityByName().find(priority);
  if (it == PriorityByName().end()) {
    throw std::runtime_error("catalog entry " + std::to_string(index) +
                             " has unknown priority '" + priority + "'");
  }

  return it->second;
}

/**
 * @brief Converts one YAML catalog entry into a SoundDef.
 */
SoundDef ParseSoundDef(const YAML::Node& node, std::size_t index) {
  const std::string id = RequireField(node, "id", index).as<std::string>();
  const std::string priority =
      RequireField(node, "priority", index).as<std::string>();

  return SoundDef{
      .id = ParseSoundId(id, index),
      .name = RequireField(node, "name", index).as<std::string>(),
      .file_path = RequireField(node, "file_path", index).as<std::string>(),
      .loop = RequireField(node, "loop", index).as<bool>(),
      .gain = RequireField(node, "gain", index).as<float>(),
      .priority = ParsePriority(priority, index),
      .duck_others = RequireField(node, "duck_others", index).as<bool>(),
      .drop_if_higher_priority_active =
          RequireField(node, "drop_if_higher_priority_active", index)
              .as<bool>(),
  };
}

/**
 * @brief Converts one YAML event entry into an EventDef.
 */
EventDef ParseEventDef(const YAML::Node& node, std::size_t index) {
  const std::string id = RequireField(node, "id", index).as<std::string>();
  const YAML::Node actions = RequireField(node, "actions", index);
  if (!actions.IsSequence() || actions.size() == 0) {
    throw std::runtime_error("event entry " + std::to_string(index) +
                             " must contain at least one action");
  }

  std::vector<SoundAction> parsed_actions;
  parsed_actions.reserve(actions.size());
  for (std::size_t action_index = 0; action_index < actions.size();
       ++action_index) {
    parsed_actions.push_back(
        ParseSoundAction(actions[action_index], action_index));
  }

  return EventDef{
      .id = ParseEventId(id, index),
      .name = RequireField(node, "name", index).as<std::string>(),
      .actions = std::move(parsed_actions),
  };
}

}  // namespace

bool TryParseSoundId(std::string_view id, SoundId* sound_id) {
  if (sound_id == nullptr) {
    return false;
  }

  const auto it = SoundIdByName().find(std::string(id));
  if (it == SoundIdByName().end()) {
    return false;
  }

  *sound_id = it->second;
  return true;
}

bool TryParseEventId(std::string_view id, EventId* event_id) {
  if (event_id == nullptr) {
    return false;
  }

  const auto it = EventIdByName().find(std::string(id));
  if (it == EventIdByName().end()) {
    return false;
  }

  *event_id = it->second;
  return true;
}

bool SoundCatalog::LoadFromFile(std::string_view path,
                                std::string& error_message) {
  try {
    const YAML::Node root = YAML::LoadFile(std::string(path));
    const YAML::Node sounds = root["sounds"];
    if (!sounds || !sounds.IsSequence()) {
      error_message =
          "Sound catalog must contain a top-level 'sounds' sequence";
      return false;
    }

    const YAML::Node events = root["events"];
    if (!events || !events.IsSequence()) {
      error_message =
          "Sound catalog must contain a top-level 'events' sequence";
      return false;
    }

    std::vector<SoundDef> loaded_definitions;
    loaded_definitions.reserve(sounds.size());
    std::set<SoundId> seen_ids;

    for (std::size_t index = 0; index < sounds.size(); ++index) {
      SoundDef def = ParseSoundDef(sounds[index], index);
      if (!seen_ids.insert(def.id).second) {
        error_message = "Sound catalog contains a duplicate sound id";
        return false;
      }
      loaded_definitions.push_back(std::move(def));
    }

    std::vector<EventDef> loaded_events;
    loaded_events.reserve(events.size());
    std::set<EventId> seen_event_ids;

    for (std::size_t index = 0; index < events.size(); ++index) {
      EventDef event = ParseEventDef(events[index], index);
      if (!seen_event_ids.insert(event.id).second) {
        error_message = "Sound catalog contains a duplicate event id";
        return false;
      }

      for (const SoundAction& action : event.actions) {
        const auto sound_it =
            std::find_if(loaded_definitions.begin(), loaded_definitions.end(),
                         [&action](const SoundDef& def) {
                           return def.id == action.sound_id;
                         });
        if (sound_it == loaded_definitions.end()) {
          error_message =
              "Event catalog references a sound missing from sounds";
          return false;
        }
      }

      loaded_events.push_back(std::move(event));
    }

    definitions_ = std::move(loaded_definitions);
    events_ = std::move(loaded_events);
    return true;
  } catch (const std::exception& error) {
    error_message = "Failed to load " + std::string(path) + ": " + error.what();
    return false;
  }
}

std::span<const SoundDef> SoundCatalog::All() const noexcept {
  return definitions_;
}

std::span<const EventDef> SoundCatalog::Events() const noexcept {
  return events_;
}

const SoundDef* SoundCatalog::Find(SoundId id) const noexcept {
  const auto it =
      std::find_if(definitions_.begin(), definitions_.end(),
                   [id](const SoundDef& def) { return def.id == id; });
  if (it == definitions_.end()) {
    return nullptr;
  }

  return &(*it);
}

const EventDef* SoundCatalog::Find(EventId id) const noexcept {
  const auto it =
      std::find_if(events_.begin(), events_.end(),
                   [id](const EventDef& event) { return event.id == id; });
  if (it == events_.end()) {
    return nullptr;
  }

  return &(*it);
}

std::span<const SoundAction> SoundCatalog::ResolveEventActions(
    EventId id) const noexcept {
  const EventDef* event = Find(id);
  if (event == nullptr) {
    return {};
  }

  return event->actions;
}
