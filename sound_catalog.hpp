#pragma once

#include <string>

/**
 * @brief Relative urgency level used by ducking and suppression policy.
 */
enum class SoundPriority { kBackground, kNormal, kAlert, kCritical };

/**
 * @brief Stable identifier for each cataloged cockpit sound.
 */
enum class SoundId {
  kAirplanePing,
  kAirportTower,
  kAmbient,
  kAutopilotDisconnectClassicBoeing,
  kAutopilotDisconnectModernVariant,
  kAutopilotDisconnectModern,
  kMasterCautionLoop,
  kMasterCautionSingle,
  kBankAngle,
  kBingoLowFuelF16,
  kConfigurationWarningClassicBoeing,
  kEngine,
  kFireBellModern757767,
  kFireBellClassic,
  kFlightAttendantSingle,
  kOverspeedClacker,
  kTerrainPullUpGws,
  kThrust1,
  kWindshear,
};

/**
 * @brief Runtime metadata for one sound asset and its playback policy.
 */
struct SoundDef {
  SoundId id;
  std::string name;
  std::string file_path;
  bool loop;
  float gain;
  SoundPriority priority;
  bool duck_others;
  bool drop_if_higher_priority_active;
};
