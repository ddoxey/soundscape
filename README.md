# Cockpit Soundscape POC

This repository contains a small C++ proof of concept for cockpit-style sound
mixing.

The current program is a command-line demo that:

- loads a catalog of WAV assets from `sounds/`
- converts them into a common internal format
- plays looping background beds and one-shot alerts together
- applies priority-based ducking
- suppresses selected lower-priority alerts entirely when a higher-priority
  alert is already active
- runs a scripted timeline for 60 seconds by default and prints the event log as
  it plays

## Current Behavior

The mixer currently demonstrates three distinct behaviors:

1. Background ambience can remain continuously present while other sounds play on
   top.
2. Higher-priority alerts can duck lower-priority material instead of fully
   muting it.
3. Some alerts are configured to be dropped completely if a higher-priority
   sound is already active.

That third case is intentional. The POC is not just showing gain reduction; it is
also modeling the operational reality that certain callouts should not be heard
at all when a more urgent warning is already claiming attention.

## Project Layout

```text
.
|-- CMakeLists.txt
|-- README.md
|-- conf/
|   |-- mock1.yaml
|   `-- script.yml
|-- sound_catalog.hpp
|-- sounds/
`-- src/
    |-- audio_asset_manager.*
    |-- audio_engine.*
    |-- audio_types.hpp
    |-- main.cpp
    |-- mixer.*
    |-- openal_audio_output.*
    |-- sound_catalog_runtime.*
    |-- sound_control_queue.*
    `-- sound_policy.*
```

## Implementation Summary

### `sound_catalog.hpp`

Defines shared sound catalog types.

Each `SoundDef` includes:

- `SoundId`
- display name
- WAV file path
- loop flag
- nominal gain
- priority
- `duck_others`
- `drop_if_higher_priority_active`

### `conf/mock1.yaml`

Defines the default aircraft-specific runtime sound catalog.

Aircraft catalog files own the configurable sound dynamics for the POC:

- display names
- WAV file paths
- loop flags
- nominal gains
- priority levels
- ducking flags
- suppression flags

The catalog is loaded and validated at startup. The `id` values are stable keys
that map to the `SoundId` enum used by the demo script.

`conf/mock1.yaml` is the default catalog when the user does not specify one.

### `conf/script.yml`

Defines the default scripted demo timeline.

Each event includes:

- `at_seconds`
- `action`: `play` or `stop`
- `sound`: a stable sound id from the selected aircraft catalog
- `note`: optional event log text

The script is loaded and validated at startup. `--validate` checks both the
selected aircraft catalog and selected script.

### `src/audio_engine.*`

Provides the public facade used by the demo.

It owns the catalog, asset manager, mixer, and OpenAL output backend, but
delegates the detailed responsibilities to those smaller runtime components.

### `src/audio_asset_manager.*`

Loads and prepares audio assets.

Key details:

- `libsndfile` is used to load WAV files
- all audio is converted into internal `48 kHz`, stereo, float buffers

### `src/mixer.*`

Implements the runtime voice mixer.

Key details:

- active voices are stored in a fixed-size pool
- the render path avoids dynamic allocation and container resizing
- play/stop requests use a fixed-size control queue consumed by the render worker
- active sounds are mixed in real time with sample clamping
- short gain ramps are applied to avoid abrupt gain jumps

### `src/sound_policy.*`

Owns runtime policy decisions.

Key details:

- priority-based ducking is evaluated from the active voice list
- playback can be refused for sounds marked `drop_if_higher_priority_active`

### `src/openal_audio_output.*`

Provides the playback backend.

Key details:

- OpenAL Soft owns the playback device and queued streaming buffers
- a small worker thread asks the mixer for chunks of audio and keeps the OpenAL
  source fed
- internal float samples are converted to stereo 16-bit PCM before queueing

The OpenAL worker drains queued render-control requests before each mix pass.
`Play` reports whether the request was queued, while suppression/drop policy is
applied later by the render worker.

### `src/sound_control_queue.*`

Provides a host-facing event-notification queue for embedded use.

Key details:

- messages are `play`, `stop`, or `shutdown`
- each play/stop message carries an `event_id` related to one sound inventory
  entry
- producers can use `TryPush()` to avoid blocking
- `AudioEngine::Run()` blocks on the queue and dispatches messages into the
  engine's non-blocking render-control path

### `src/main.cpp`

Implements the scripted demo runner.

It loads a scripted timeline of events, starts the audio engine, triggers events
at scheduled times by pushing `SoundControlMessage` values, and prints what
happened:

- `PLAY` means a play event notification was queued
- `STOP` means a stop event notification was queued
- `REJECT` means the request could not be queued or referenced an unavailable
  sound

Suppression policy is applied later by the render worker after queued event
notifications are translated into render-control requests.

## Embedded Control Mode

The command-line program still uses the scripted timeline in `main.cpp`, but a
calling application can also run the mixer from its own thread:

```cpp
AudioEngine engine;
SoundControlQueue control_queue;

std::string error_message;
engine.LoadCatalog("conf/mock1.yaml", error_message);
engine.Initialize(error_message);

std::thread mixer_thread(&AudioEngine::Run, &engine, std::ref(control_queue));

control_queue.TryPush({
    .type = SoundControlMessageType::kPlay,
    .event_id = SoundId::kMasterCautionSingle,
});
control_queue.TryPush({
    .type = SoundControlMessageType::kStop,
    .event_id = SoundId::kMasterCautionLoop,
});
control_queue.TryPush({
    .type = SoundControlMessageType::kShutdown,
});

mixer_thread.join();
engine.Shutdown();
```

This queue is intentionally separate from the fixed render-control queue inside
`AudioEngine`. The host-facing queue lets the application event loop hand off
messages safely, while the render worker still owns mixer-state changes.

## Audio Policy

The mixer uses these priority levels:

- `kBackground`
- `kNormal`
- `kAlert`
- `kCritical`

Current ducking behavior:

- If an alert is active and `duck_others` is enabled, background and normal
  sounds are reduced.
- If a critical warning is active and `duck_others` is enabled, background,
  normal, and alert sounds are reduced more aggressively.
- Sounds of equal or higher priority are not ducked by the active sound.

Current suppression behavior:

- Sounds marked `drop_if_higher_priority_active` are not started if a
  higher-priority sound is already active at trigger time.
- This is used to model situations where a less important alert should not be
  heard at all under a more urgent warning.

## Demo Timeline

The default run is 60 seconds.

The scripted demo currently includes:

- continuous background loops such as ambient, engine, thrust texture, and tower
  chatter
- alert-style tones such as autopilot disconnect and master caution
- critical warnings such as terrain pull up, windshear, bank angle, and
  overspeed clacker
- an explicit suppression example where `Autopilot Disconnect Modern Variant` is
  dropped when `Terrain Pull Up` is triggered at the same moment

## Build

Requirements:

- CMake 3.20+
- a C++20 compiler
- OpenAL Soft
- `libsndfile`
- `yaml-cpp`

On Ubuntu:

```bash
sudo apt install libopenal-dev libsndfile1-dev libyaml-cpp-dev
```

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

Run with an explicit aircraft catalog:

```bash
./build/cockpit_soundscape --config conf/mock1.yaml
```

Run with both duration and catalog:

```bash
./build/cockpit_soundscape --config conf/mock1.yaml 24
```

Run with an explicit script:

```bash
./build/cockpit_soundscape --script conf/script.yml
```

Validate a catalog and script without initializing audio or loading WAV assets:

```bash
./build/cockpit_soundscape --validate --config conf/mock1.yaml --script conf/script.yml
```

Headless or sandboxed run without a real audio device:

```bash
ALSOFT_DRIVERS=null ./build/cockpit_soundscape --config conf/mock1.yaml --script conf/script.yml 24
```

## Notes

- The sound gains and some loop choices are still POC tuning values, not final
  mix decisions.
- The scheduler is intentionally simple and deterministic.
- This is not yet a general-purpose audio engine or simulator integration layer.
- The current focus is proving mixing, ducking, suppression, and catalog-driven
  behavior with real assets.
