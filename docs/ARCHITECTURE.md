# coroNET OS 2 Architecture

coroNET OS 2 is a clean rewrite of coroNET 1. The goal is to keep the product behavior and hardware compatibility while removing the monolithic firmware structure that made late-stage features expensive in RAM, DMA, and maintenance risk.

## Core Principles

- One central system state shared by services.
- Services communicate through small events, not direct cross-module ownership.
- UI displays state and sends commands; it does not own business logic.
- Boot remains deterministic: first impression tasks have priority over background connectivity.
- BLE is designed as the first-setup and fallback API for the Android companion app.
- WiFi exposes the richer local companion API once the device has network credentials.
- The companion protocol supports multiple coroNET devices with a stable ESP32-derived ID and a separate user-visible name.
- BLE notifications use a versioned binary/chunked protocol and commands are parsed as exact JSON fields.
- WiFi control requires a per-device token transferred during the BLE pairing window.
- LED rendering is a layered engine, not a collection of unrelated direct frame writers.
- Production firmware has exactly one printer-telemetry source: the configured real printer connection. It has no simulator, demo state, mock progress, or runtime telemetry override.
- Memory policy is explicit: PSRAM first for large/stateful allocations, DMA only for hardware transfer buffers, internal RAM reserved for small critical objects and stacks.

## Service Boundaries

- `SystemState`: shared runtime state and snapshots.
- `DisplayService`: LVGL display, touch, screens, and UI theme.
- `AudioService`: SD-backed WAV playback and status sounds.
- `WifiService`: connection lifecycle, network scans, and the shared mDNS responder used by printer discovery and local services.
- `BleService`: NimBLE companion protocol.
- `WebControlService`: local HTTP API and conditional HTTP service publication through the shared mDNS responder.
- `LedService`: layered LED engine, ambient, dimming, preview, and boot show.
- `PrinterService`: Moonraker/WebSocket/HTTP integration.
- `SettingsService`: versioned NVS settings, migrations, runtime revisions, and debounced persistence.
- `SystemHealth`: memory, DMA, watchdog, and diagnostics.
- `QuietService`: time-bounded global Sound/LED suppression with Error bypass.
- `PandaBreathService`: optional external vent workflow state machine.
- `OtaService`: GitHub release updates, same-version reinstall, rollback validity, and SD recovery.

## Milestone 0

- Create PlatformIO project.
- Add MIT license.
- Add hardware map inherited from coroNET 1.
- Add minimal service skeletons.
- Compile a bootable firmware.
- Start with display, touch, audio, WiFi, and NimBLE boundaries.

## Platform Baseline

- PlatformIO uses the pioarduino Espressif32 platform pinned to Arduino-ESP32 3.2.1 / ESP-IDF 5.4.x.
- The display BSP is kept as a local hardware layer so the project does not depend on hidden Arduino sketch files.
- LVGL is isolated behind `DisplayService`; application logic should update state first and let the UI render from that state.

## UI Theme Model

- The UI has two independent settings: `UiSkin` for the visual style and `UiColorMode` for dark/light/auto.
- Every skin must support both dark and light rendering from the same screen logic.
- Screens should render from shared state and theme tokens, not hard-coded per-screen color islands.
- The setup wizard and operational Home screen share the same `UiTheme` tokens, geometry language, and restrained cyan/amber visual hierarchy.
- Live screens cache their last rendered values and only invalidate LVGL objects when state changes.
- The UI router keeps only the active operational screen alive; inactive tabs are destroyed after the transition instead of permanently consuming LVGL/internal memory.
- Planned skins start as `Coronet` (the original simple cyan/amber dashboard direction), `Graphite`, `Aurora`, and `Minimal`; names can still change before public OS 2 release.

OS 1 features are reviewed before migration in [OS1_FEATURE_SCOPE.md](OS1_FEATURE_SCOPE.md). The on-device LED animation creator, external lighting-provider integrations, disabled MIDI engine, and product activation are intentionally excluded from OS 2. The optional DIY chamber-heater interface is a guarded 3.3 V logic output for an external driver; Panda Breath remains supported through a testable workflow state machine.

## Memory Policy

- `MemoryService` runs before settings, display, WiFi, BLE, and audio services.
- When PSRAM is available, allocations larger than `PsramMallocThresholdBytes` are allowed to go to external RAM through `heap_caps_malloc_extmem_enable`.
- The LVGL draw canvas is explicitly allocated in PSRAM, while dirty-area refresh avoids full-panel transfers for small changes.
- LCD transfer buffers remain DMA-capable internal memory because the LCD peripheral needs DMA-visible memory.
- Small BSP objects, touch contexts, semaphores, callbacks, and FreeRTOS task stacks stay in internal RAM unless there is a measured reason to move them.
- Audio PCM staging is explicitly allocated in PSRAM. Only the bounded I2S descriptor ring and its hardware transfer buffers use DMA-capable internal memory.
- Future large buffers for WAV input, JSON snapshots, file indexes, icons, cached screens, LED previews, and app protocol queues should use PSRAM-aware allocation helpers instead of raw `malloc`.
- NimBLE host allocations use the library-supported external-memory mode. Controller memory and the host task stack remain internal, while eligible protocol buffers are placed in PSRAM.
- Startup memory checkpoints are emitted after every service initialization so a regression can be assigned to a service instead of inferred from one final heap value.

## Measured Memory Baseline

Measured on the target JC3248W535 with display, touch, WiFi station stack, printer worker, web routes, BLE advertising, and the I2S audio service active:

| Stage | DMA-capable internal memory used | PSRAM used |
| --- | ---: | ---: |
| Display and touch | approximately 63 KB | approximately 315 KB |
| Audio service, task, and balanced I2S ring | approximately 10.5 KB | less than 1 KB plus indexed file data |
| WiFi station stack | approximately 42.3 KB | approximately 10 KB |
| Printer worker and queues | approximately 10 KB | less than 1 KB |
| Web routes | approximately 2.8 KB | negligible |
| BLE with external host allocation | approximately 41 KB | approximately 11 KB |

With all current services enabled, the measured connected Home steady state retains approximately 87 KB of free DMA-capable memory with a 31.7 KB largest contiguous block and approximately 8.04 MB free PSRAM. The Settings screen retains approximately 70.6 KB free DMA. These figures include display, touch, audio, WiFi, web API, printer polling, BLE, LED, local vent, Panda state machine, and OTA service state.

Repeated Home/LED/Vent/Sound/Settings transitions and repeated entry into all seven clock screens return to stable steady-state values. A GitHub OTA metadata check temporarily reduced the recorded DMA minimum to approximately 32 KB, then recovered to the normal Home value. The OTA path therefore enters maintenance mode before firmware download and stops nonessential network/control work. Exact baselines are intentionally re-measured as screens and protocol payloads evolve.

### Audio DMA profiles

The OS 2 audio producer runs in a dedicated Core 0 task. Its 128-frame mono PCM staging buffer is in PSRAM, while the I2S DMA ring stays internal as required by the peripheral.

| Profile | I2S DMA layout | Driver/ring cost | Buffered time at 22.05 kHz | Buffered time at 48 kHz |
| --- | ---: | ---: | ---: | ---: |
| OS 2 balanced | 16 x 128 mono frames | 5,868 B | 92.9 ms | 42.7 ms |
| coroNET 1 comparison | 48 x 128 mono frames | 15,928 B | 278.6 ms | 128.0 ms |

The balanced profile played continuously at both 22.05 kHz and 48 kHz while WiFi, the local web service, BLE, display refresh, and printer discovery were active. No I2S write failures were observed. Sample rate does not change the allocated DMA byte count; it changes how much time the fixed ring can absorb and affects source/decode bandwidth. Mono remains the correct hardware output because coroNET has one physical speaker, while future stereo WAV input can be downmixed before entering the DMA ring.

Development builds expose `audio test`, `audio stop`, `audio status`, `audio rescan`, `audio release`, `audio profile balanced`, `audio profile coronet1`, and `audio rate 22050|44100|48000` on the serial console. Profile switching is intended for controlled measurements; normal startup always selects the balanced profile.

## Runtime Concurrency

- NimBLE callbacks enqueue fixed-size commands through a FreeRTOS queue; command parsing and settings mutation remain in the main service loop.
- Moonraker HTTP polling runs in a low-priority Core 0 worker and returns immutable results through a queue.
- I2S output runs in a dedicated Core 0 producer task so display, network, and setup work cannot starve the audio ring from the main loop.
- WiFi credentials are verified before the setup wizard commits them. Snapmaker mDNS discovery and the Moonraker subnet fallback scan reuse the printer worker, while discovered-device storage remains in PSRAM.
- A single failed printer poll does not erase the last valid state; three consecutive failures are required before the device is marked offline.
- A successful Moonraker `/printer/info` probe proves reachability only. Telemetry becomes valid exclusively after a structurally valid object-query response containing a real printer state.
- Every accepted telemetry snapshot increments one telemetry revision. Connection changes use a separate revision, while genuine state changes during uninterrupted valid telemetry publish one shared transition sequence with `from`, `to`, and timestamp metadata.
- Reconnection establishes a fresh baseline and does not synthesize a printer event. Audio, Panda workflows, display wake, BLE, WiFi, and Android notifications consume the shared transition sequence instead of independently inferring changes.
- Settings changes become visible immediately through an in-memory revision, while NVS writes are debounced and bounded by a maximum delay.
