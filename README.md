# Cockpit Soundscape POC

This repository contains a small C++ proof of concept for cockpit-style sound mixing.

The current program is a command-line demo that:

- loads a catalog of WAV assets from `sounds/`
- converts them into a common internal format
- plays looping background beds and one-shot alerts together
- applies priority-based ducking
- suppresses selected lower-priority alerts entirely when a higher-priority alert is already active
- runs a scripted timeline for 60 seconds by default and prints the event log as it plays

## Current Behavior

The mixer currently demonstrates three distinct behaviors:

1. Background ambience can remain continuously present while other sounds play on top.
2. Higher-priority alerts can duck lower-priority material instead of fully muting it.
3. Some alerts are configured to be dropped completely if a higher-priority sound is already active.

That third case is intentional. The POC is not just showing gain reduction; it is also modeling the operational reality that certain callouts should not be heard at all when a more urgent warning is already claiming attention.

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── sound_catalog.hpp
├── sounds/
└── src/
    ├── audio_engine.cpp
    ├── audio_engine.hpp
    └── main.cpp
```

## Implementation Summary

### `sound_catalog.hpp`

Defines the static sound catalog.

Each `SoundDef` includes:

- `SoundId`
- display name
- WAV file path
- loop flag
- nominal gain
- priority
- `duck_others`
- `drop_if_higher_priority_active`

The catalog is currently tailored to the WAV files present in `sounds/`.

### `src/audio_engine.*`

Implements the runtime mixer.

Key details:

- `libsndfile` is used to load WAV files
- all audio is converted into internal `48 kHz`, stereo, float buffers
- `SDL2` provides the playback device and audio callback
- active sounds are mixed in real time with sample clamping
- short gain ramps are applied to avoid abrupt gain jumps
- playback can be refused for sounds marked `drop_if_higher_priority_active`

### `src/main.cpp`

Implements the scripted demo runner.

It builds a fixed timeline of events, starts the audio engine, triggers events at scheduled times, and prints what happened:

- `PLAY` means the sound was started
- `STOP` means a looping sound was stopped
- `DROP` means the sound was intentionally suppressed by policy

## Audio Policy

The mixer uses these priority levels:

- `kBackground`
- `kNormal`
- `kAlert`
- `kCritical`

Current ducking behavior:

- If an alert is active and `duck_others` is enabled, background and normal sounds are reduced.
- If a critical warning is active and `duck_others` is enabled, background, normal, and alert sounds are reduced more aggressively.
- Sounds of equal or higher priority are not ducked by the active sound.

Current suppression behavior:

- Sounds marked `drop_if_higher_priority_active` are not started if a higher-priority sound is already active at trigger time.
- This is used to model situations where a less important alert should not be heard at all under a more urgent warning.

## Demo Timeline

The default run is 60 seconds.

The scripted demo currently includes:

- continuous background loops such as ambient, engine, thrust texture, and tower chatter
- alert-style tones such as autopilot disconnect and master caution
- critical warnings such as terrain pull up, windshear, bank angle, and overspeed clacker
- an explicit suppression example where `Autopilot Disconnect Modern Variant` is dropped when `Terrain Pull Up` is triggered at the same moment

## Build

Requirements:

- CMake 3.20+
- a C++20 compiler
- `SDL2`
- `libsndfile`

Build:

```bash
cmake -S . -B build
cmake --build build
```

## Run

Default 60-second demo:

```bash
./build/cockpit_soundscape
```

Shorter run:

```bash
./build/cockpit_soundscape 24
```

Headless or sandboxed run without a real audio device:

```bash
SDL_AUDIODRIVER=dummy ./build/cockpit_soundscape 24
```

## Notes

- The sound gains and some loop choices are still POC tuning values, not final mix decisions.
- The scheduler is intentionally simple and deterministic.
- This is not yet a general-purpose audio engine or simulator integration layer.
- The current focus is proving mixing, ducking, suppression, and catalog-driven behavior with real assets.
