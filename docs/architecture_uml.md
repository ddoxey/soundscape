# Architecture UML

These diagrams reflect the current code structure in `src/` and the YAML-backed
runtime configuration used by `AudioEngine`.

## Component Diagram

```mermaid
classDiagram
direction LR

class DemoRunner {
  +main(argc, argv)
  +ParseOptions()
  +LoadScript()
}

class SoundControlQueue {
  +TryPush(message) bool
  +WaitAndPop(message*) bool
  +Close()
}

class AudioEngine {
  +LoadCatalog(path, error) bool
  +Initialize(error) bool
  +Run(control_queue)
  +ApplyEvent(event_id) bool
  +Play(sound_id) bool
  +Stop(sound_id)
  +Render(output, frame_count)
}

class SoundCatalog {
  +LoadFromFile(path, error) bool
  +Find(sound_id) SoundDef*
  +Find(event_id) EventDef*
  +ResolveEventActions(event_id) SoundAction[]
}

class AudioAssetManager {
  +LoadCatalog(catalog, error) bool
  +Find(sound_id) SoundBuffer*
  +Clear()
}

class Mixer {
  +Play(def, buffer) bool
  +Stop(sound_id)
  +Mix(output, frame_count)
  +Clear()
}

class SoundPolicyEngine {
  +CanStart(def, active_instances) bool
  +DuckFactorFor(target_priority, active_instances) float
}

class OpenAlAudioOutput {
  +Initialize(render_target, error) bool
  +Shutdown()
  -WorkerLoop()
  -FillBuffer(buffer_id, error) bool
}

class AudioRenderTarget {
  <<interface>>
  +Render(output, frame_count)
}

class SoundCatalogYaml {
  <<file>>
  conf/mock1.yaml
}

class DemoScriptYaml {
  <<file>>
  conf/script.yml
}

class WavAssets {
  <<directory>>
  sounds/*.wav
}

DemoRunner --> SoundControlQueue : pushes host events
DemoRunner --> AudioEngine : setup and lifecycle
DemoRunner --> DemoScriptYaml : loads timeline
AudioEngine ..|> AudioRenderTarget
AudioEngine --> SoundCatalog : owns
AudioEngine --> AudioAssetManager : owns
AudioEngine --> Mixer : owns
AudioEngine --> OpenAlAudioOutput : owns
AudioEngine --> SoundControlQueue : consumes
SoundCatalog --> SoundCatalogYaml : parses
AudioAssetManager --> SoundCatalog : loads definitions from
AudioAssetManager --> WavAssets : decodes
Mixer --> SoundPolicyEngine : evaluates policy
OpenAlAudioOutput --> AudioRenderTarget : pulls audio frames
```

## Core Class Diagram

```mermaid
classDiagram
direction TB

class SoundDef {
  +SoundId id
  +string name
  +string file_path
  +bool loop
  +float gain
  +SoundPriority priority
  +bool duck_others
  +bool drop_if_higher_priority_active
}

class SoundAction {
  +SoundActionType type
  +SoundId sound_id
}

class EventDef {
  +EventId id
  +string name
  +vector~SoundAction~ actions
}

class SoundBuffer {
  +int sample_rate
  +int channels
  +vector~float~ samples
  +FrameCount() size_t
}

class SoundInstance {
  +SoundId id
  +SoundBuffer* buffer
  +size_t frame_position
  +float base_gain
  +float current_gain
  +float target_gain
  +bool loop
  +bool active
  +SoundPriority priority
  +bool duck_others
}

class SoundCatalog {
  -vector~SoundDef~ definitions_
  -vector~EventDef~ events_
}

class AudioAssetManager {
  -unordered_map~SoundId, SoundBuffer~ buffers_
}

class Mixer {
  -SoundPolicyEngine policy_
  -array~SoundInstance, 32~ active_instances_
}

class AudioEngine {
  -SoundCatalog catalog_
  -AudioAssetManager assets_
  -Mixer mixer_
  -OpenAlAudioOutput output_
  -array~ControlCommand, 64~ control_commands_
}

SoundCatalog "1" o-- "*" SoundDef
SoundCatalog "1" o-- "*" EventDef
EventDef "1" o-- "*" SoundAction
AudioAssetManager "1" o-- "*" SoundBuffer
Mixer "1" o-- "*" SoundInstance
Mixer --> SoundPolicyEngine
AudioEngine *-- SoundCatalog
AudioEngine *-- AudioAssetManager
AudioEngine *-- Mixer
AudioEngine *-- OpenAlAudioOutput
SoundInstance --> SoundBuffer : references
```

## Event-To-Audio Sequence

```mermaid
sequenceDiagram
autonumber

participant Host as Host App / Demo Runner
participant Queue as SoundControlQueue
participant Engine as AudioEngine
participant Catalog as SoundCatalog
participant Assets as AudioAssetManager
participant Output as OpenAlAudioOutput
participant Mixer as Mixer
participant Policy as SoundPolicyEngine

Host->>Engine: LoadCatalog("conf/mock1.yaml")
Engine->>Catalog: LoadFromFile(path)
Engine->>Assets: LoadCatalog(catalog)
Assets-->>Engine: decoded SoundBuffer objects
Host->>Engine: Initialize()
Engine->>Output: Initialize(*this)
Output->>Engine: Render(output, frame_count) on worker thread
Engine->>Mixer: Mix(output, frame_count)

Host->>Queue: TryPush(kNotify, event_id)
Engine->>Queue: Run() / WaitAndPop()
Queue-->>Engine: SoundControlMessage
Engine->>Catalog: ResolveEventActions(event_id)

loop for each action
  alt play action
    Engine->>Catalog: Find(sound_id)
    Engine->>Assets: Find(sound_id)
    Engine->>Engine: EnqueueControlCommand(kPlay, def, buffer)
  else stop action
    Engine->>Engine: EnqueueControlCommand(kStop, sound_id)
  end
end

Output->>Engine: Render(output, frame_count)
Engine->>Engine: DrainControlCommands()
Engine->>Mixer: Play(def, buffer) / Stop(sound_id)
Mixer->>Policy: CanStart() / DuckFactorFor()
Policy-->>Mixer: start allowed + duck factor
Mixer-->>Engine: mixed stereo float frames
Engine-->>Output: interleaved float buffer
Output->>Output: convert to PCM16 and queue OpenAL buffers
```

## Threading / Runtime View

```mermaid
flowchart LR
  subgraph HostThread["Host / Demo Thread"]
    A["main() / host app"] --> B["SoundControlQueue::TryPush()"]
  end

  subgraph ControlThread["AudioEngine::Run() Thread"]
    C["WaitAndPop()"] --> D["ResolveEventActions()"]
    D --> E["EnqueueControlCommand()"]
  end

  subgraph AudioThread["OpenAlAudioOutput Worker"]
    F["WorkerLoop()"] --> G["AudioEngine::Render()"]
    G --> H["DrainControlCommands()"]
    H --> I["Mixer::Play()/Stop()"]
    I --> J["Mixer::Mix()"]
    J --> K["PCM conversion + OpenAL queue"]
  end

  B --> C
  E --> H
```

## Notes

- `AudioEngine` is the façade and the only component that spans config loading,
  host event handling, non-blocking control dispatch, mixing, and output.
- `SoundControlQueue` is the host-facing blocking queue.
- `AudioEngine` also owns a separate fixed-size single-producer/single-consumer
  ring buffer for render-thread-safe play/stop commands.
- `Mixer` owns active playback state and stays allocation-free during mix.
- `SoundPolicyEngine` decides suppression and ducking independently of the audio
  backend.
- `OpenAlAudioOutput` pulls audio from `AudioRenderTarget` rather than pushing
  callbacks into `Mixer` directly.
