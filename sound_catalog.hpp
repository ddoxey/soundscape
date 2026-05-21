#pragma once

#include <string>
#include <vector>

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
 * @brief Stable identifier for cockpit events that can drive sound behavior.
 */
enum class EventId {
  kAmbientBed,
  kAmbientBedCleared,
  kEngineBed,
  kEngineBedCleared,
  kThrustTexture,
  kThrustTextureCleared,
  kTowerChatter,
  kTowerChatterCleared,
  kAirplanePingRequested,
  kAutopilotDisconnect,
  kMasterCautionSingle,
  kMasterCautionLoop,
  kTerrainPullUp,
  kAutopilotDisconnectSuppressionDemo,
  kWindshear,
  kMasterCautionCleared,
  kFlightAttendantChime,
  kBingoLowFuel,
  kAutopilotDisconnectClassic,
  kBankAngle,
  kOverspeed,
  kLossOfCabinPressure,
  kImproperTakeoffConfiguration,
};

/**
 * @brief Sound operation performed when a cockpit event is consumed.
 */
enum class SoundActionType { kPlay, kStop };

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

/**
 * @brief Runtime sound action associated with a cockpit event.
 */
struct SoundAction {
  SoundActionType type;
  SoundId sound_id;
};

/**
 * @brief Runtime mapping from a cockpit event to one or more sound actions.
 */
struct EventDef {
  EventId id;
  std::string name;
  std::vector<SoundAction> actions;
};
