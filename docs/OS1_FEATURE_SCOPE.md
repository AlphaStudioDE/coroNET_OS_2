# coroNET OS 1 Feature Scope Review

This document inventories the product-level screens, cards, controls, and background systems found in the final coroNET OS 1 firmware. It is a migration decision list, not a promise to port every feature.

Status meanings:

- `OS2`: already implemented or explicitly part of the OS 2 direction.
- `DEFERRED`: intentionally postponed until measurements or field testing justify it.
- `REDESIGN`: keep the purpose, but do not port the OS 1 implementation directly.
- `OMIT`: intentionally excluded from coroNET OS 2.

## Shell, Boot, And Setup

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| CORE-01 | Boot screen with progress and firmware version | REDESIGN | Build a deterministic OS 2 boot experience later. |
| CORE-02 | Synchronized LED boot show and `boot.wav` playback | REDESIGN | Retain as a coordinated OS 2 LED/audio sequence with a measured boot budget. |
| CORE-03 | First-run Wi-Fi and printer setup wizard | OS2 | Implemented with validation and discovery. |
| CORE-04 | Activation screen, device ID, license file, and manual activation key | OMIT | OS 2 is MIT licensed and does not use product activation. |
| CORE-05 | Persistent NVS settings and migrations | OS2 | Implemented as versioned settings. |
| CORE-06 | Top status rail for Wi-Fi, printer, ventilation, sound, and overall health | REDESIGN | Retain the information in a lighter shared status header. |
| CORE-07 | Bottom navigation: Home, LED, Heat/Vent, Sound, Settings | OS2 | Lightweight one-screen-at-a-time router is implemented. |
| CORE-08 | Touch activity tracking and global screen wake | OS2 | Foundation exists; saver behavior remains to be implemented. |
| CORE-09 | Touch anywhere to stop active status audio | REDESIGN | Retain and coordinate the stop state with phone/app control. |

## Home

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| HOME-01 | Printer state, filename, progress, print time, and ETA | OS2 | State, filename, and progress are implemented; time and ETA remain. |
| HOME-02 | Active tool and nozzle temperature | OS2 | Implemented. |
| HOME-03 | Material name and filament color swatch | OS2 | Retain after the required printer telemetry fields are available. |
| HOME-04 | Chamber temperature | OS2 | Implemented. |
| HOME-05 | Live fan motion and flap position indicators | OS2 | Add with the ventilation service. |
| HOME-06 | Printer alerts and event text | REDESIGN | Use a compact event/attention treatment instead of the old alert box. |
| HOME-07 | Live miniature LED preview | REDESIGN | Replace the always-visible 60-dot preview with a lightweight section-level representation. |

## Printer And Connectivity

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| NET-01 | Nearby Wi-Fi scan, password entry, and connection validation | OS2 | Implemented. |
| NET-02 | Automatic Snapmaker and Moonraker discovery | OS2 | Implemented. |
| NET-03 | Manual printer host and port configuration | OS2 | Implemented through setup. |
| NET-04 | Moonraker HTTP telemetry polling | OS2 | Initial worker is implemented. |
| NET-05 | Moonraker WebSocket subscription and event updates | OS2 | Retain as a later real-time telemetry milestone after HTTP polling is stable. |
| NET-06 | Fast/medium/slow telemetry polling groups | REDESIGN | Retain only telemetry cadence that is measured and needed. |
| NET-07 | Printer and Wi-Fi watchdog/reconnect behavior | OS2 | Initial resilient lifecycle exists and needs soak testing. |
| NET-08 | BLE companion bridge | OS2 | Implemented with framed protocol V2 and revisioned printer events. |
| NET-09 | Local Wi-Fi companion API and mDNS | OS2 | Implemented with token authentication. |
| NET-10 | Multiple coroNET devices in the Android app | OS2 | Required companion-app behavior. |

## LED Screen And Engine

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| LED-01 | 60 SK6812 RGBW LEDs split into right, center, left, and inside | OS2 | Required hardware mapping for the new engine. |
| LED-02 | Separate animation selection for Idle, Print, Pause, Error, Finish, and Other | REDESIGN | Keep categories, rebuild animations for the layered engine. |
| LED-03 | Ten-second physical animation preview | OS2 | Retain as a non-blocking, deterministic preview. |
| LED-04 | On-screen 60-dot physical LED preview | REDESIGN | Use a cheaper section-level preview instead of recreating 60 persistent UI objects. |
| LED-05 | Per-section brightness for left, center, right, and inside | OS2 | Retain independent section controls. |
| LED-06 | Inside style: fixed white or ambient mapping | OS2 | Retain as a global output policy in the new engine. |
| LED-07 | Per-section inactivity DIMM with independent percentages | OS2 | Retain with one shared inactivity timer and independent target levels. |
| LED-08 | Mirror physical LED layout | OS2 | Retain for alternate assembly direction, including ambient mapping. |
| LED-09 | Color Remix hue offset per printer-state category | OS2 | Retain while protecting colors with semantic meaning. |
| LED-10 | Decorative Other mode independent of printer state | OS2 | Retain as an explicit decorative operating mode. |
| LED-11 | Printer-aware animation data: progress, filament color, tool, bed, and chamber temperature | OS2 | Core goal of the new layered engine. |
| LED-12 | Finish-animation handoff and special completion sequences such as Snake | REDESIGN | Rebuild as explicit event layers/state machines. |
| LED-13 | Silent mode able to suppress LEDs while preserving errors | OS2 | Coordinate with the global quiet-hours policy and always preserve errors. |
| LED-14 | Manual on-screen animation creator, targets, effects, and preset slots | OMIT | Too heavy and difficult to use on the small display. |
| LED-15 | External lighting providers: Philips Hue, WLED, Home Assistant, Nanoleaf, Shelly | OMIT | Not needed by testers; excluded to save firmware, RAM, networking, and UI cost. |

## Sound

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| SOUND-01 | I2S mono audio service with PSRAM staging | OS2 | Foundation implemented and measured. |
| SOUND-02 | SD-backed WAV playback | OS2 | Next audio milestone. |
| SOUND-03 | Scenarios: Start, Finish, Error, Pause, and Idle | OS2 | Retain the five scenario groups. |
| SOUND-04 | Per-scenario file selection | OS2 | Retain with bounded SD indexing. |
| SOUND-05 | Per-scenario volume | OS2 | Retain independent scenario volume. |
| SOUND-06 | Per-scenario repeat/off behavior | OS2 | Retain with an explicit stop policy for repeating errors. |
| SOUND-07 | SD folder browser with paged file list and preview | REDESIGN | Retain as a lightweight paged browser backed by a bounded file index. |
| SOUND-08 | Audio preview, touch stop, and replay cooldown | REDESIGN | Preserve behavior through the new non-blocking player. |
| SOUND-09 | Fade-in, fade-out, click/pop suppression, and output idle shutdown | OS2 | Product-quality requirements for the WAV engine. |
| SOUND-10 | MIDI synthesizer and multi-track playback | OMIT | Already disabled in OS 1 and unnecessary for OS 2. |
| SOUND-11 | Boot audio continuing beyond the visual boot handoff | OS2 | Retain so the full boot track can finish independently of the visual handoff. |

## Ventilation And Heat

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| VENT-01 | Automatic, cavity-target, and manual ventilation modes | OS2 | Retain the three local ventilation modes. |
| VENT-02 | Printer telemetry as the sole temperature input for ventilation | OS2 | There is no source selector. Ventilation consumes printer data and selects the relevant available temperature field internally. |
| VENT-03 | Target-temperature control | OS2 | Retain for automatic and cavity-target modes. |
| VENT-04 | Manual fan PWM and flap position | OS2 | Retain for direct hardware testing and override. |
| VENT-05 | Servo closed/open endpoint calibration | OS2 | Retain for the existing hardware. |
| VENT-06 | Servo direction reverse | OS2 | Retain for assembly variants. |
| VENT-07 | Fan and servo failsafe output behavior | OS2 | Required if ventilation hardware is enabled. |
| VENT-08 | Panda Breath discovery and direct stock-firmware control | OS2 | Retain, improve, and validate against the available physical Panda Breath test unit. |
| VENT-09 | Panda modes: Auto, preheat hold, tempering, forced on, filament drying | REDESIGN | Retain all selected modes in a cleaner, testable state machine. |
| VENT-10 | Panda material profiles and drying presets | REDESIGN | Retain with the improved Panda integration and validate every profile on hardware. |
| VENT-11 | DIY chamber-heater 5 V output mode | OMIT | Excluded until a separately defined and safety-reviewed hardware module exists. |

## Display, Time, And Quiet Behavior

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| UI-01 | Dark, Light, Black, and Creamy color themes | REDESIGN | OS 2 plans dark/light variants for each selected skin. |
| UI-02 | Classic, Modern, Flat, and Retro UI layouts | REDESIGN | OS 2 currently plans Coronet, Graphite, Aurora, and Minimal skins. |
| UI-03 | Accent hue and brightness customization | OS2 | Retain through shared theme tokens. |
| UI-04 | Main display brightness | OS2 | Implemented in Settings. |
| UI-05 | Screen saver disabled, screen off, or clock | OS2 | Retain all three choices. |
| UI-06 | Clock brightness | OS2 | Retain independently of normal display brightness. |
| UI-07 | Digital, Retro, Analog, Linear/Horizon, Bauhaus, Dot Matrix, and Arc clocks | OS2 | Retain the existing clock selection, rebuilding each style with the shared theme model. |
| UI-08 | 12/24-hour format | OS2 | Retain. |
| UI-09 | Time zone selection and NTP synchronization | OS2 | Retain for clocks and scheduled quiet mode. |
| UI-10 | Quiet mode duration and target: sound, LEDs, both, or off | OS2 | Retain as one coherent OS 2 policy. |
| UI-11 | Allow printer errors through quiet mode | OS2 | Retain as required safety behavior. |
| UI-12 | Error-only display wake from printer events | OS2 | Retain for screen saver behavior. |

## Firmware And Product Maintenance

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| SYS-01 | GitHub OTA check, install, and same-version reinstall | OS2 | Required before public release with a new implementation. |
| SYS-02 | OTA resource preparation and suspension of unnecessary services | OS2 | Retain as part of the OTA state machine. |
| SYS-03 | SD-card recovery update | OS2 | Retain as the recovery path. |
| SYS-04 | What's New popup | OS2 | Retain when versioned OS 2 releases begin. |
| SYS-05 | Reopen setup without erasing unrelated preferences | OS2 | Implemented from Settings. |
| SYS-06 | Full factory reset | OS2 | Retain with MIT-era semantics and no activation state. |
| SYS-07 | Runtime heap, DMA, largest-block, PSRAM, and service diagnostics | OS2 | Implemented through `SystemHealth` and Serial. |
| SYS-08 | Display/LVGL render watchdog | DEFERRED | Add only after measured failure modes justify it. |
| SYS-09 | SD asset validation and file indexing | OS2 | Retain with a bounded index and explicit asset validation. |

## Confirmed Omissions

The following OS 1 systems must not be implemented in coroNET OS 2 unless this document is explicitly revised:

1. The on-device manual LED animation creator and its preset slots (`LED-14`).
2. External lighting-provider integrations for Philips Hue, WLED, Home Assistant, Nanoleaf, and Shelly (`LED-15`).
3. The disabled MIDI synthesizer/player (`SOUND-10`).
4. Product activation, license files, and manual activation keys (`CORE-04`).
5. The DIY chamber-heater output mode until a separate safety-reviewed hardware module exists (`VENT-11`).
6. Any runtime printer simulator, demo state, fake progress source, or mock telemetry override. OS 2 consumes only telemetry from the configured real printer.
