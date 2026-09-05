# coroNET OS 1 Feature Scope Review

This document inventories the product-level screens, cards, controls, and background systems found in the final coroNET OS 1 firmware. It is a migration decision list, not a promise to port every feature.

Status meanings:

- `OS2`: implemented in the current OS 2 codebase.
- `DEFERRED`: intentionally postponed until measurements or field testing justify it.
- `REDESIGN`: keep the purpose, but do not port the OS 1 implementation directly.
- `OMIT`: intentionally excluded from coroNET OS 2.

## Shell, Boot, And Setup

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| CORE-01 | Boot screen with progress and firmware version | REDESIGN | Implemented as deterministic full first-run and short daily Boot Experience variants. |
| CORE-02 | Synchronized LED boot show and `boot.wav` playback | REDESIGN | Implemented as coordinated screen, LED, and audio sequences with measured handoff timing. |
| CORE-03 | First-run Wi-Fi and printer setup wizard | OS2 | Implemented with validation and discovery. |
| CORE-04 | Activation screen, device ID, license file, and manual activation key | OMIT | OS 2 is MIT licensed and does not use product activation. |
| CORE-05 | Persistent NVS settings and migrations | OS2 | Implemented as versioned settings. |
| CORE-06 | Top status rail for Wi-Fi, printer, ventilation, sound, and overall health | OS2 | Implemented as a compact Wi-Fi, BLE companion, and printer connection header. Ventilation and audio details remain on their relevant screens instead of crowding the global header. |
| CORE-07 | Bottom navigation: Home, LED, Heat/Vent, Sound, Settings | OS2 | Lightweight one-screen-at-a-time router is implemented. |
| CORE-08 | Touch activity tracking and global screen wake | OS2 | Implemented with display-off and clock saver modes plus printer-error wake. |
| CORE-09 | Touch anywhere to stop active status audio | REDESIGN | Implemented with the same audio state shared by the touchscreen, Android app, and browser panel. |

## Home

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| HOME-01 | Printer state, filename, progress, print time, and ETA | OS2 | Implemented from validated Moonraker telemetry. |
| HOME-02 | Active tool and nozzle temperature | OS2 | Implemented. |
| HOME-03 | Material name and filament color swatch | OS2 | Implemented for the active tool from available printer telemetry. |
| HOME-04 | Chamber temperature | OS2 | Implemented. |
| HOME-05 | Live fan motion and flap position indicators | OS2 | Implemented from the central ventilation state. |
| HOME-06 | Printer alerts and event text | REDESIGN | Implemented as compact state and attention feedback instead of the old alert box. |
| HOME-07 | Live miniature LED preview | REDESIGN | Moved to the dedicated LED interfaces as a compact firmware-generated 60-pixel frame, avoiding persistent preview objects on Home. |

## Printer And Connectivity

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| NET-01 | Nearby Wi-Fi scan, password entry, and connection validation | OS2 | Implemented. |
| NET-02 | Automatic Snapmaker and Moonraker discovery | OS2 | Implemented. |
| NET-03 | Manual printer host and port configuration | OS2 | Implemented through setup. |
| NET-04 | Moonraker HTTP telemetry polling | OS2 | Implemented as fallback and periodic realtime integrity audit. |
| NET-05 | Moonraker WebSocket subscription and event updates | OS2 | Implemented in the background printer worker with object discovery, stale-session detection, and reconnect handling. |
| NET-06 | Fast/medium/slow telemetry polling groups | REDESIGN | Replaced by realtime events plus measured fallback and audit intervals. |
| NET-07 | Printer and Wi-Fi watchdog/reconnect behavior | OS2 | Implemented with stale-session detection, bounded retry, last-valid-state protection, and HTTP fallback; extended soak qualification remains on the release path. |
| NET-08 | BLE companion bridge | OS2 | Implemented with framed protocol V2 and revisioned printer events. |
| NET-09 | Local Wi-Fi companion API and mDNS | OS2 | Implemented with token authentication. |
| NET-10 | Multiple coroNET devices in the Android app | OS2 | Implemented with saved-device selection, per-device credentials, cache, and reconnect state. |

## LED Screen And Engine

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| LED-01 | 60 SK6812 RGBW LEDs split into right, center, left, and inside | OS2 | Implemented with the original physical mapping and optional mirroring. |
| LED-02 | Separate animation selection for Idle, Print, Pause, Error, Finish, and Other | REDESIGN | Rebuilt as 336 selectable animations across six categories on the layered engine. |
| LED-03 | Ten-second physical animation preview | OS2 | Implemented as a non-blocking preview using representative state data. |
| LED-04 | On-screen 60-dot physical LED preview | REDESIGN | Implemented as a compact firmware-generated frame shared with the touchscreen, Android app, and browser panel at 2 FPS while visible. |
| LED-05 | Per-section brightness for left, center, right, and inside | OS2 | Implemented with independent section controls. |
| LED-06 | Inside style: fixed white or ambient mapping | OS2 | Implemented as a global output policy. |
| LED-07 | Per-section inactivity DIMM with independent percentages | OS2 | Implemented with shared activity tracking and independent target levels. |
| LED-08 | Mirror physical LED layout | OS2 | Implemented for alternate assembly direction, including previews and ambient mapping. |
| LED-09 | Color Remix hue offset per printer-state category | OS2 | Implemented while preserving colors with semantic meaning. |
| LED-10 | Decorative Other mode independent of printer state | OS2 | Implemented as an explicit decorative operating mode. |
| LED-11 | Printer-aware animation data: progress, filament color, tool, bed, and chamber temperature | OS2 | Implemented throughout the layered animation catalog. |
| LED-12 | Finish-animation handoff and special completion sequences such as Snake | REDESIGN | Rebuilt as explicit event and handoff state machines. |
| LED-13 | Silent mode able to suppress LEDs while preserving errors | OS2 | Implemented through Quiet Mode with optional error override. |
| LED-14 | Manual on-screen animation creator, targets, effects, and preset slots | OMIT | Too heavy and difficult to use on the small display. |
| LED-15 | External lighting providers: Philips Hue, WLED, Home Assistant, Nanoleaf, Shelly | OMIT | Not needed by testers; excluded to save firmware, RAM, networking, and UI cost. |

## Sound

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| SOUND-01 | I2S mono audio service with PSRAM staging | OS2 | Implemented and measured with a dedicated producer task and bounded DMA ring. |
| SOUND-02 | SD-backed WAV playback | OS2 | Implemented with a dedicated audio task and PSRAM staging. |
| SOUND-03 | Scenarios: Start, Finish, Error, Pause, and Idle | OS2 | Implemented as five independently configured scenario groups. |
| SOUND-04 | Per-scenario file selection | OS2 | Implemented with bounded SD indexing and folder browsing. |
| SOUND-05 | Per-scenario volume | OS2 | Implemented with independent scenario volume. |
| SOUND-06 | Per-scenario repeat/off behavior | OS2 | Implemented with explicit stop behavior for repeating playback. |
| SOUND-07 | SD folder browser with paged file list and preview | OS2 | Implemented as a lightweight five-row picker backed by a bounded, sorted PSRAM index. |
| SOUND-08 | Audio preview, touch stop, and replay cooldown | OS2 | Non-blocking preview and global touch-to-stop are implemented. |
| SOUND-09 | Fade-in, fade-out, click/pop suppression, and output idle shutdown | OS2 | Implemented with gain ramps, queued digital silence, complete partial-write handling, and controlled I2S release. |
| SOUND-10 | MIDI synthesizer and multi-track playback | OMIT | Already disabled in OS 1 and unnecessary for OS 2. |
| SOUND-11 | Boot audio continuing beyond the visual boot handoff | OS2 | Implemented so the full boot track can finish independently of the visual handoff. |

## Ventilation And Heat

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| VENT-01 | Automatic, cavity-target, and manual ventilation modes | OS2 | Implemented as three local ventilation modes. |
| VENT-02 | Printer telemetry as the sole temperature input for ventilation | OS2 | There is no source selector. Ventilation consumes printer data and selects the relevant available temperature field internally. |
| VENT-03 | Target-temperature control | OS2 | Implemented for automatic and cavity-target modes. |
| VENT-04 | Manual fan PWM and flap position | OS2 | Implemented for direct hardware testing and override. |
| VENT-05 | Servo closed/open endpoint calibration | OS2 | Implemented for the existing hardware. |
| VENT-06 | Servo direction reverse | OS2 | Implemented for assembly variants. |
| VENT-07 | Fan and servo failsafe output behavior | OS2 | Implemented for startup, invalid telemetry, and maintenance states. |
| VENT-08 | Panda Breath discovery and direct stock-firmware control | OS2 | Implemented with mDNS discovery and manual host configuration; physical Panda qualification remains open. |
| VENT-09 | Panda modes: Auto, preheat hold, tempering, forced on, filament drying | REDESIGN | Rebuilt as an explicit testable state machine. |
| VENT-10 | Panda material profiles and drying presets | REDESIGN | Rebuilt with shared profiles and synchronized controls; physical Panda qualification remains open. |
| VENT-11 | DIY chamber-heater output mode | OS2 | GPIO46 manual active-HIGH 3.3 V logic output for an external relay, MOSFET, optocoupler, or dedicated driver. Forced LOW during startup and firmware maintenance. |

## Display, Time, And Quiet Behavior

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| UI-01 | Dark, Light, Black, and Creamy color themes | REDESIGN | Replaced by dark, light, and automatic modes shared by every OS 2 skin. |
| UI-02 | Classic, Modern, Flat, and Retro UI layouts | REDESIGN | Replaced by the Coronet, Graphite, Aurora, and Minimal skins. |
| UI-03 | Accent hue and brightness customization | OS2 | Implemented through shared theme tokens. |
| UI-04 | Main display brightness | OS2 | Implemented in Settings. |
| UI-05 | Screen saver disabled, screen off, or clock | OS2 | Implemented with all three choices. |
| UI-06 | Clock brightness | OS2 | Implemented independently of normal display brightness. |
| UI-07 | Digital, Retro, Analog, Linear/Horizon, Bauhaus, Dot Matrix, and Arc clocks | OS2 | Rebuilt as seven styles using the shared theme model. |
| UI-08 | 12/24-hour format | OS2 | Implemented across the settings model and clock screen. |
| UI-09 | Time zone selection and NTP synchronization | OS2 | Implemented with location-specific DST rules and synchronized touchscreen, Android, and browser settings. |
| UI-10 | Quiet mode duration and target: sound, LEDs, both, or off | OS2 | Implemented as one coherent shared policy. |
| UI-11 | Allow printer errors through quiet mode | OS2 | Implemented as an explicit safety override. |
| UI-12 | Error-only display wake from printer events | OS2 | Implemented for both clock and display-off saver modes. |

## Firmware And Product Maintenance

| ID | OS 1 capability | Status | OS 2 implementation decision |
| --- | --- | --- | --- |
| SYS-01 | GitHub OTA check, install, and same-version reinstall | OS2 | Implemented with verified release metadata, checksums, maintenance mode, and rollback validation. |
| SYS-02 | OTA resource preparation and suspension of unnecessary services | OS2 | Implemented by releasing BLE, Moonraker, and Panda network resources during the TLS window. |
| SYS-03 | SD-card recovery update | OS2 | Implemented as the offline recovery path. |
| SYS-04 | What's New popup | OMIT | Release notes remain on GitHub; the on-device popup is excluded to reduce UI, firmware, and maintenance cost. |
| SYS-05 | Reopen setup without erasing unrelated preferences | OS2 | Implemented from Settings. |
| SYS-06 | Full factory reset | OS2 | Implemented with MIT-era semantics and no activation state. |
| SYS-07 | Runtime heap, DMA, largest-block, PSRAM, and service diagnostics | OS2 | Implemented through `SystemHealth` and Serial. |
| SYS-08 | Display/LVGL render watchdog | DEFERRED | Add only after measured failure modes justify it. |
| SYS-09 | SD asset validation and file indexing | OS2 | Implemented with a bounded PSRAM index and explicit asset validation. |

## Confirmed Omissions

The following OS 1 systems must not be implemented in coroNET OS 2 unless this document is explicitly revised:

1. The on-device manual LED animation creator and its preset slots (`LED-14`).
2. External lighting-provider integrations for Philips Hue, WLED, Home Assistant, Nanoleaf, and Shelly (`LED-15`).
3. The disabled MIDI synthesizer/player (`SOUND-10`).
4. Product activation, license files, and manual activation keys (`CORE-04`).
5. The on-device What's New popup (`SYS-04`); versioned release notes remain available on GitHub.
6. Any runtime printer simulator, demo state, fake progress source, or mock telemetry override. OS 2 consumes only telemetry from the configured real printer.
