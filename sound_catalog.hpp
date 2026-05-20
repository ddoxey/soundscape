#pragma once

#include <array>
#include <string_view>

enum class SoundPriority {
    kBackground,
    kNormal,
    kAlert,
    kCritical
};

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

struct SoundDef {
    SoundId id;
    std::string_view name;
    std::string_view file_path;
    bool loop;
    float gain;
    SoundPriority priority;
    bool duck_others;
    bool drop_if_higher_priority_active;
};

inline constexpr std::array kSoundCatalog{
    SoundDef {
        SoundId::kAirplanePing,
        "Airplane Ping",
        "sounds/airplane_ping.wav",
        false,
        0.65f,
        SoundPriority::kNormal,
        false,
        false
    },
    SoundDef {
        SoundId::kAirportTower,
        "Airport Tower",
        "sounds/airport__tower.wav",
        true,
        0.22f,
        SoundPriority::kBackground,
        false,
        false
    },
    SoundDef {
        SoundId::kAmbient,
        "Ambient",
        "sounds/ambient.wav",
        true,
        0.30f,
        SoundPriority::kBackground,
        false,
        false
    },
    SoundDef {
        SoundId::kAutopilotDisconnectClassicBoeing,
        "Autopilot Disconnect Classic Boeing",
        "sounds/autopilot_disconnect_classic_boeing.wav",
        false,
        0.90f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kAutopilotDisconnectModernVariant,
        "Autopilot Disconnect Modern Variant",
        "sounds/autopilot_disconnect_modern (1).wav",
        false,
        0.90f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kAutopilotDisconnectModern,
        "Autopilot Disconnect",
        "sounds/autopilot_disconnect_modern.wav",
        false,
        0.90f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kMasterCautionLoop,
        "Master Caution Loop",
        "sounds/b757_767_master_caution.wav",
        true,
        0.85f,
        SoundPriority::kAlert,
        true,
        false
    },
    SoundDef {
        SoundId::kMasterCautionSingle,
        "Master Caution Single",
        "sounds/b757_767_master_caution_single.wav",
        false,
        0.90f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kBankAngle,
        "Bank Angle",
        "sounds/bank_angle.wav",
        false,
        0.95f,
        SoundPriority::kCritical,
        true,
        false
    },
    SoundDef {
        SoundId::kBingoLowFuelF16,
        "Bingo Low Fuel F-16",
        "sounds/bingo_low_fuel_f16.wav",
        false,
        0.85f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kConfigurationWarningClassicBoeing,
        "Configuration Warning Classic Boeing",
        "sounds/configuration_warning_classic_boeing.wav",
        true,
        0.85f,
        SoundPriority::kAlert,
        true,
        true
    },
    SoundDef {
        SoundId::kEngine,
        "Engine",
        "sounds/engine.wav",
        true,
        0.45f,
        SoundPriority::kBackground,
        false,
        false
    },
    SoundDef {
        SoundId::kFireBellModern757767,
        "Fire Bell Modern 757/767",
        "sounds/fire_bell_modern_757_767.wav",
        false,
        0.95f,
        SoundPriority::kCritical,
        true,
        false
    },
    SoundDef {
        SoundId::kFireBellClassic,
        "Fire Bell Classic",
        "sounds/firebell_classic.wav",
        false,
        0.95f,
        SoundPriority::kCritical,
        true,
        false
    },
    SoundDef {
        SoundId::kFlightAttendantSingle,
        "Flight Attendant Single",
        "sounds/flight_attendant_single.wav",
        false,
        0.60f,
        SoundPriority::kNormal,
        false,
        true
    },
    SoundDef {
        SoundId::kOverspeedClacker,
        "Overspeed Clacker",
        "sounds/overspeed_clacker.wav",
        true,
        1.00f,
        SoundPriority::kCritical,
        true,
        false
    },
    SoundDef {
        SoundId::kTerrainPullUpGws,
        "Terrain Pull Up",
        "sounds/terrain_pull_up_gws.wav",
        false,
        1.00f,
        SoundPriority::kCritical,
        true,
        false
    },
    SoundDef {
        SoundId::kThrust1,
        "Thrust 1",
        "sounds/thrust1.wav",
        true,
        0.35f,
        SoundPriority::kBackground,
        false,
        false
    },
    SoundDef {
        SoundId::kWindshear,
        "Windshear",
        "sounds/windshear.wav",
        false,
        1.00f,
        SoundPriority::kCritical,
        true,
        false
    },
};
