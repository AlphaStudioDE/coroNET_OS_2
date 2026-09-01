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
- Future large buffers for audio, JSON snapshots, file indexes, icons, cached screens, LED previews, and app protocol queues should use PSRAM-aware allocation helpers instead of raw `malloc`.

## Runtime Concurrency

- NimBLE callbacks enqueue fixed-size commands through a FreeRTOS queue; command parsing and settings mutation remain in the main service loop.
- Moonraker HTTP polling runs in a low-priority Core 0 worker and returns immutable results through a queue.
- A single failed printer poll does not erase the last valid state; three consecutive failures are required before the device is marked offline.
- Settings changes become visible immediately through an in-memory revision, while NVS writes are debounced and bounded by a maximum delay.
