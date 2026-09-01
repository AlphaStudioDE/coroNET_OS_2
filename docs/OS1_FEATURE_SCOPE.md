# coroNET OS 1 Feature Scope Review

This document inventories the product-level screens, cards, controls, and background systems found in the final coroNET OS 1 firmware. It is a migration decision list, not a promise to port every feature.

Status meanings:

- `OS2`: already implemented or explicitly part of the OS 2 direction.
- `DECIDE`: requires a product decision before implementation.
- `REDESIGN`: keep the purpose, but do not port the OS 1 implementation directly.
- `OMIT`: intentionally excluded from coroNET OS 2.

## Shell, Boot, And Setup

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| CORE-01 | Boot screen with progress and firmware version | REDESIGN | Build a deterministic OS 2 boot experience later. |
| CORE-02 | Synchronized LED boot show and `boot.wav` playback | DECIDE | Keep only if it fits the new LED/audio architecture and boot budget. |
| CORE-03 | First-run Wi-Fi and printer setup wizard | OS2 | Implemented with validation and discovery. |
| CORE-04 | Activation screen, device ID, license file, and manual activation key | DECIDE | OS 2 is MIT licensed, so activation is not assumed. |
| CORE-05 | Persistent NVS settings and migrations | OS2 | Implemented as versioned settings. |
| CORE-06 | Top status rail for Wi-Fi, printer, ventilation, sound, and overall health | DECIDE | Home currently uses a lighter connectivity header. |
| CORE-07 | Bottom navigation: Home, LED, Heat/Vent, Sound, Settings | OS2 | Lightweight one-screen-at-a-time router is implemented. |
| CORE-08 | Touch activity tracking and global screen wake | OS2 | Foundation exists; saver behavior remains to be implemented. |
| CORE-09 | Touch anywhere to stop active status audio | DECIDE | Useful behavior, but should be reviewed with phone/app control. |

## Home

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| HOME-01 | Printer state, filename, progress, print time, and ETA | OS2 | State, filename, and progress are implemented; time and ETA remain. |
| HOME-02 | Active tool and nozzle temperature | OS2 | Implemented. |
| HOME-03 | Material name and filament color swatch | DECIDE | Telemetry support must be added before the card. |
| HOME-04 | Chamber temperature | OS2 | Implemented. |
| HOME-05 | Live fan motion and flap position indicators | DECIDE | Add with the ventilation service if retained. |
| HOME-06 | Printer alerts and event text | REDESIGN | Use a compact event/attention treatment instead of the old alert box. |
| HOME-07 | Live miniature LED preview | DECIDE | May cost too much for an always-visible Home element. |

## Printer And Connectivity

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| NET-01 | Nearby Wi-Fi scan, password entry, and connection validation | OS2 | Implemented. |
| NET-02 | Automatic Snapmaker and Moonraker discovery | OS2 | Implemented. |
| NET-03 | Manual printer host and port configuration | OS2 | Implemented through setup. |
| NET-04 | Moonraker HTTP telemetry polling | OS2 | Initial worker is implemented. |
| NET-05 | Moonraker WebSocket subscription and event updates | DECIDE | Potentially useful, but adds connection and JSON complexity. |
| NET-06 | Fast/medium/slow telemetry polling groups | REDESIGN | Retain only telemetry cadence that is measured and needed. |
| NET-07 | Printer and Wi-Fi watchdog/reconnect behavior | OS2 | Initial resilient lifecycle exists and needs soak testing. |
| NET-08 | BLE companion bridge | OS2 | Implemented with framed protocol V1. |
| NET-09 | Local Wi-Fi companion API and mDNS | OS2 | Implemented with token authentication. |
| NET-10 | Multiple coroNET devices in the Android app | OS2 | Required companion-app behavior. |

## LED Screen And Engine

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| LED-01 | 60 SK6812 RGBW LEDs split into right, center, left, and inside | OS2 | Required hardware mapping for the new engine. |
| LED-02 | Separate animation selection for Idle, Print, Pause, Error, Finish, and Other | REDESIGN | Keep categories, rebuild animations for the layered engine. |
| LED-03 | Ten-second physical animation preview | DECIDE | Useful, but must be non-blocking and predictable. |
| LED-04 | On-screen 60-dot physical LED preview | DECIDE | Consider a cheaper section preview instead. |
| LED-05 | Per-section brightness for left, center, right, and inside | DECIDE | Likely useful; confirm desired control model. |
| LED-06 | Inside style: fixed white or ambient mapping | DECIDE | Strong candidate for the new engine. |
| LED-07 | Per-section inactivity DIMM with independent percentages | DECIDE | Useful but adds state and settings surface. |
| LED-08 | Mirror physical LED layout | DECIDE | Useful for alternate assembly direction. |
| LED-09 | Color Remix hue offset per printer-state category | DECIDE | Lightweight customization if semantic colors remain protected. |
| LED-10 | Decorative Other mode independent of printer state | DECIDE | Confirm whether OS 2 still needs an always-on decorative mode. |
| LED-11 | Printer-aware animation data: progress, filament color, tool, bed, and chamber temperature | OS2 | Core goal of the new layered engine. |
| LED-12 | Finish-animation handoff and special completion sequences such as Snake | REDESIGN | Rebuild as explicit event layers/state machines. |
| LED-13 | Silent mode able to suppress LEDs while preserving errors | DECIDE | Coordinate with the global quiet-hours design. |
| LED-14 | Manual on-screen animation creator, targets, effects, and preset slots | OMIT | Too heavy and difficult to use on the small display. |
| LED-15 | External lighting providers: Philips Hue, WLED, Home Assistant, Nanoleaf, Shelly | OMIT | Not needed by testers; excluded to save firmware, RAM, networking, and UI cost. |

## Sound

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| SOUND-01 | I2S mono audio service with PSRAM staging | OS2 | Foundation implemented and measured. |
| SOUND-02 | SD-backed WAV playback | OS2 | Next audio milestone. |
| SOUND-03 | Scenarios: Start, Finish, Error, Pause, and Idle | DECIDE | Confirm final scenario set before building the UI. |
| SOUND-04 | Per-scenario file selection | DECIDE | Likely useful if the SD browser remains lightweight. |
| SOUND-05 | Per-scenario volume | DECIDE | Strong candidate. |
| SOUND-06 | Per-scenario repeat/off behavior | DECIDE | Error repetition needs a clear stop policy. |
| SOUND-07 | SD folder browser with paged file list and preview | DECIDE | Useful but one of the heavier OS 1 UI components. |
| SOUND-08 | Audio preview, touch stop, and replay cooldown | REDESIGN | Preserve behavior through the new non-blocking player. |
| SOUND-09 | Fade-in, fade-out, click/pop suppression, and output idle shutdown | OS2 | Product-quality requirements for the WAV engine. |
| SOUND-10 | MIDI synthesizer and multi-track playback | OMIT | Already disabled in OS 1 and unnecessary for OS 2. |
| SOUND-11 | Boot audio continuing beyond the visual boot handoff | DECIDE | Keep only if the OS 2 boot show is approved. |

## Ventilation And Heat

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| VENT-01 | Automatic, cavity-target, and manual ventilation modes | DECIDE | Core local ventilation candidate. |
| VENT-02 | Temperature source selection: auto, chamber, enclosure, bed, extruder | DECIDE | Keep only sources reliably exposed by supported printers. |
| VENT-03 | Target-temperature control | DECIDE | Depends on retained automatic mode. |
| VENT-04 | Manual fan PWM and flap position | DECIDE | Required for direct hardware testing and override. |
| VENT-05 | Servo closed/open endpoint calibration | DECIDE | Strong candidate for identical hardware. |
| VENT-06 | Servo direction reverse | DECIDE | Useful for assembly variants. |
| VENT-07 | Fan and servo failsafe output behavior | OS2 | Required if ventilation hardware is enabled. |
| VENT-08 | Panda Breath discovery and direct stock-firmware control | DECIDE | Separate external-device integration; not assumed for OS 2. |
| VENT-09 | Panda modes: Auto, preheat hold, tempering, forced on, filament drying | DECIDE | High complexity and should be selected mode by mode. |
| VENT-10 | Panda material profiles and drying presets | DECIDE | Depends on Panda integration. |
| VENT-11 | DIY chamber-heater 5 V output mode | DECIDE | Safety-sensitive; retain only with explicit hardware definition. |

## Display, Time, And Quiet Behavior

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| UI-01 | Dark, Light, Black, and Creamy color themes | REDESIGN | OS 2 plans dark/light variants for each selected skin. |
| UI-02 | Classic, Modern, Flat, and Retro UI layouts | REDESIGN | OS 2 currently plans Coronet, Graphite, Aurora, and Minimal skins. |
| UI-03 | Accent hue and brightness customization | DECIDE | Lightweight if implemented through shared theme tokens. |
| UI-04 | Main display brightness | OS2 | Implemented in Settings. |
| UI-05 | Screen saver disabled, screen off, or clock | DECIDE | Confirm desired choices. |
| UI-06 | Clock brightness | DECIDE | Relevant only if clock saver is retained. |
| UI-07 | Digital, Retro, Analog, Linear/Horizon, Bauhaus, Dot Matrix, and Arc clocks | DECIDE | Select individual clock styles instead of porting all seven. |
| UI-08 | 12/24-hour format | DECIDE | Low cost if clock functionality remains. |
| UI-09 | Time zone selection and NTP synchronization | DECIDE | Needed for clocks and scheduled quiet mode. |
| UI-10 | Quiet mode duration and target: sound, LEDs, both, or off | DECIDE | Useful, but should become one coherent OS 2 policy. |
| UI-11 | Allow printer errors through quiet mode | DECIDE | Recommended safety behavior if quiet mode remains. |
| UI-12 | Error-only display wake from printer events | DECIDE | Strong candidate for screen saver behavior. |

## Firmware And Product Maintenance

| ID | OS 1 capability | Status | OS 2 direction |
| --- | --- | --- | --- |
| SYS-01 | GitHub OTA check, install, and same-version reinstall | DECIDE | Required before public release, implementation should be new. |
| SYS-02 | OTA resource preparation and suspension of unnecessary services | DECIDE | Strong candidate after OTA transport is selected. |
| SYS-03 | SD-card recovery update | DECIDE | Valuable recovery path. |
| SYS-04 | What's New popup | DECIDE | Low priority until versioned releases begin. |
| SYS-05 | Reopen setup without erasing unrelated preferences | OS2 | Implemented from Settings. |
| SYS-06 | Full factory reset | DECIDE | Required eventually, with MIT-era semantics and no activation state. |
| SYS-07 | Runtime heap, DMA, largest-block, PSRAM, and service diagnostics | OS2 | Implemented through `SystemHealth` and Serial. |
| SYS-08 | Display/LVGL render watchdog | DECIDE | Add only after measured failure modes justify it. |
| SYS-09 | SD asset validation and file indexing | DECIDE | Needed for audio; index scope should remain bounded. |

## Confirmed Omissions

The following OS 1 systems must not be implemented in coroNET OS 2 unless this document is explicitly revised:

1. The on-device manual LED animation creator and its preset slots (`LED-14`).
2. External lighting-provider integrations for Philips Hue, WLED, Home Assistant, Nanoleaf, and Shelly (`LED-15`).
3. The disabled MIDI synthesizer/player (`SOUND-10`).
