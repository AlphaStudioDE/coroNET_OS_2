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
- Memory policy is explicit: PSRAM first for large/stateful allocations, DMA only for hardware transfer buffers, internal RAM reserved for small critical objects and stacks.

## Planned Services

- `SystemState`: shared runtime state and snapshots.
- `EventBus`: lightweight event delivery between modules.
- `DisplayService`: LVGL display, touch, screens, and UI theme.
- `AudioService`: SD-backed WAV playback and status sounds.
- `WifiService`: connection lifecycle and network state.
- `BleService`: NimBLE companion protocol.
- `WebControlService`: local HTTP API and mDNS discovery for WiFi companion control.
- `LedService`: layered LED engine, ambient, dimming, preview, and boot show.
- `PrinterService`: Moonraker/WebSocket/HTTP integration.
- `SettingsService`: versioned NVS settings, migrations, runtime revisions, and debounced persistence.
- `SystemHealth`: memory, DMA, watchdog, and diagnostics.

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
- Planned skins start as `Coronet` (the original simple cyan/amber dashboard direction), `Graphite`, `Aurora`, and `Minimal`; names can still change before public OS 2 release.

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
| Display and touch | 55,016 B | 314,680 B |
| Audio service, task, and balanced I2S ring | 9,708 B | 276 B |
| WiFi station stack | 42,084 B | 9,880 B |
| Printer worker and queues | 9,964 B | 564 B |
| Web routes | 1,268 B | 0 B |
| BLE with external host allocation | 39,980 B | 10,384 B |

The measured connected steady state retained approximately 109.8 KB of free DMA-capable memory with a 53.2 KB largest contiguous block. A full printer-discovery scan temporarily reduced free DMA to approximately 106.6 KB without a restart or audio write failure.

### Audio DMA profiles

The OS 2 audio producer runs in a dedicated Core 0 task. Its 128-frame mono PCM staging buffer is in PSRAM, while the I2S DMA ring stays internal as required by the peripheral.

| Profile | I2S DMA layout | Driver/ring cost | Buffered time at 22.05 kHz | Buffered time at 48 kHz |
| --- | ---: | ---: | ---: | ---: |
| OS 2 balanced | 16 x 128 mono frames | 5,868 B | 92.9 ms | 42.7 ms |
| coroNET 1 comparison | 48 x 128 mono frames | 15,928 B | 278.6 ms | 128.0 ms |

The balanced profile played continuously at both 22.05 kHz and 48 kHz while WiFi, the local web service, BLE, display refresh, and printer discovery were active. No I2S write failures were observed. Sample rate does not change the allocated DMA byte count; it changes how much time the fixed ring can absorb and affects source/decode bandwidth. Mono remains the correct hardware output because coroNET has one physical speaker, while future stereo WAV input can be downmixed before entering the DMA ring.

Development builds expose `audio test`, `audio stop`, `audio status`, `audio release`, `audio profile balanced`, `audio profile coronet1`, and `audio rate 22050|44100|48000` on the serial console. Profile switching is intended for controlled measurements; normal startup always selects the balanced profile.

## Runtime Concurrency

- NimBLE callbacks enqueue fixed-size commands through a FreeRTOS queue; command parsing and settings mutation remain in the main service loop.
- Moonraker HTTP polling runs in a low-priority Core 0 worker and returns immutable results through a queue.
- I2S output runs in a dedicated Core 0 producer task so display, network, and setup work cannot starve the audio ring from the main loop.
- WiFi credentials are verified before the setup wizard commits them. Snapmaker mDNS discovery and the Moonraker subnet fallback scan reuse the printer worker, while discovered-device storage remains in PSRAM.
- A single failed printer poll does not erase the last valid state; three consecutive failures are required before the device is marked offline.
- Settings changes become visible immediately through an in-memory revision, while NVS writes are debounced and bounded by a maximum delay.
