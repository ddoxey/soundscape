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
  if (!TryParseSoundId(id, sound_id)) {
    throw std::runtime_error("catalog entry " + std::to_string(index) +
                             " has unknown id '" + id + "'");
  }

  return sound_id;
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

}  // namespace

bool TryParseSoundId(std::string_view id, SoundId& sound_id) {
  const auto it = SoundIdByName().find(std::string(id));
  if (it == SoundIdByName().end()) {
    return false;
  }

  sound_id = it->second;
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

    definitions_ = std::move(loaded_definitions);
    return true;
  } catch (const std::exception& error) {
    error_message = "Failed to load " + std::string(path) + ": " + error.what();
    return false;
  }
}

std::span<const SoundDef> SoundCatalog::All() const noexcept {
  return definitions_;
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
