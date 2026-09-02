# Development Updates

## 2026-09-02

### Companion Settings Reliability

- Fixed controls that could appear to work only once when a BLE settings snapshot was delayed or missed.
- Added immediate local settings feedback so button labels, switches, and subsequent actions always use the latest selected value.
- Added a delayed BLE settings reconciliation after edits without allowing an older response to undo a newer choice.
- Serialized WiFi settings writes and guarded periodic refreshes against stale out-of-order responses.

### Printer Telemetry Contract

- Added explicit telemetry validity plus independent telemetry, connection, and printer-event revisions.
- Validated Moonraker object-query structure before accepting data; incomplete JSON can no longer silently become an Idle printer state.
- Made reconnect establish a clean baseline instead of synthesizing Start, Finish, Error, or Idle events.
- Moved audio, Error screen wake, Panda post-print tempering, BLE delivery, WiFi state, and Android notifications onto one shared printer-transition sequence.
- Extended companion protocol V2 and the WiFi API with telemetry freshness and transition metadata, preventing duplicate phone notifications after reconnect or transport switching.
- Revalidated firmware and Android builds, then verified hardware startup with zero dropped LED frames and stable DMA/PSRAM recovery.

## 2026-09-01

### Product UI

- Added complete Home, LED, Vent, Sound, and Settings surfaces using one-screen-at-a-time LVGL ownership.
- Added Coronet, Graphite, Aurora, and Minimal skins with dark, light, and automatic color modes.
- Added configurable accent hue, display brightness, temporary quiet mode, and seven low-cost clock styles.
- Added touch inactivity screen saver behavior and Error-only printer wake.
- Expanded Home telemetry with material, color, duration, ETA, fan, and flap state.

### Hardware Services

- Added indexed SD WAV playback with PSRAM staging and startup/status asset validation.
- Added the layered RGBW LED engine, logical four-section mapping, ambient inside mode, dimming, mirroring, and preview control.
- Added local PWM fan and servo flap control with 500-2500 us calibration, reverse, limits, and fail-safe outputs.
- Added the optional Panda Breath workflow service for automatic, preheat, tempering, forced-on, and filament-drying modes.

### Connectivity And Recovery

- Expanded the authenticated WiFi API to synchronize UI, LED, sound, vent, Panda, and quiet settings.
- Expanded BLE with framed settings groups and bounded operational settings patches.
- Added GitHub OTA check/install/reinstall, progress state, maintenance preparation, app-valid marking, factory reset, and `/firmware.bin` SD recovery.
- Fixed rapid clock transitions by clearing style-specific LVGL object references before every rebuild.

### Android Companion

- Added a native Android application with multi-device storage, BLE discovery, queued GATT operations, framed payload reassembly, and automatic first pairing.
- Added encrypted per-device token storage, preferred local WiFi control, bidirectional settings refresh, and Error/Finish notifications.
- Added mirrored Home, LED, Vent, Sound, and Settings screens and verified the debug APK on the minimum API 26 emulator.

### Reliability Pass

- Serialized Android MTU negotiation and GATT service discovery, with a bounded fallback when a phone does not return the MTU callback.
- Added notification reception compatible with Android 8 through Android 12 while retaining the Android 13+ callback path.
- Bounded BLE command queues and fragment assembly by source, type, message ID, count, payload size, and timeout; complete payloads are now assembled with one allocation.
- Isolated printer transition history per saved device and removed stale scan timers when switching or reconnecting devices.
- Disabled Android cloud backup for Keystore-bound pairing data and prevented stale GitHub release metadata from being reused by OTA.
- Revalidated firmware startup, stable heap/DMA recovery, OTA metadata checks, repeated BLE reconnects, framed settings transfer, and the API 26 app lifecycle.
