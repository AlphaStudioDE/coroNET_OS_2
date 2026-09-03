# Development Updates

## 2026-09-03

### Status Sound Library

- Added an intuitive per-event sound selector for Print Start, Print Finish, Error, Pause, and Idle.
- Replaced single-step filename cycling with a lightweight paged picker that displays only five WAV entries at a time.
- Added explicit default-sound selection, alphabetical file ordering, missing-file feedback, playback testing, stopping, per-event volume, and repeat controls.
- Added bounded recursive indexing for `/sounds` subfolders while retaining direct root-level WAV support and reserving `/boot.wav` exclusively for the boot experience.
- Moved manual SD rescanning onto the audio worker task so card access does not block LVGL or make the touchscreen appear frozen.
- Added card-removal detection and a public status-sound setup guide.

### Rebuilt LED Animation Library

- Recreated the complete coroNET animation library for the OS 2 engine instead of copying legacy rendering code directly.
- Added 336 documented animations across Print, Pause, Error, Idle, Finish, and Other, including telemetry-aware effects driven by progress, filament, temperature, timing, connectivity, and ventilation state.
- Standardized all effects on logical Right, Center, Left, Inside, full-OUTER, and mirrored visual-path helpers.
- Added a public LED animation catalog describing every selectable effect and the live printer data it represents.

### Physical LED Color Calibration

- Added an on-device calibration workspace with fixed LCD references for red, orange, yellow, green, cyan, blue, violet, and magenta.
- Added independent hue, saturation, and brightness correction for every reference color, with smooth interpolation for all intermediate animation hues.
- Persisted calibration in device settings while supporting per-color reset, full reset, cancel, and explicit save actions.
- Decoupled the LCD preview from physical correction, allowing the screen to remain the visual target while the SK6812 strip is compensated for its spectrum and diffuser.
- Restored the proven coroNET 1 gamma-2 brightness curve, RGB hue-preserving luminance scaling, saturation-aware RGBW extraction, and animation headroom.
- Removed low-level brightness flooring, reduced pastel contamination during fades, corrected preview direction, and stabilized saturated colors during frame smoothing.
- Replaced Meteor's drifting hue increment with controlled spectrum anchors and a true pure-red pass.

### Discovery, Memory, And Distribution

- Made printer discovery fast and deterministic by trying saved endpoints and mDNS before a bounded network scan.
- Improved DMA headroom through PSRAM-backed LVGL allocation, reduced audio pressure, startup reservations, and expanded heap diagnostics.
- Added validated, versioned Espressif Flash Download Tool packages with a merged factory image, individual binaries, manifests, instructions, and checksums.
- Prepared firmware `0.2.0` for GitHub OTA delivery, same-version reinstall, SD recovery, and clean factory flashing.

## 2026-09-02

### Verified OTA Delivery

- Replaced insecure OTA transport with certificate-bundle-verified HTTPS and a trusted-clock prerequisite.
- Added semantic version comparison, downgrade rejection, explicit same-version reinstall, release asset size validation, ESP32 image-header validation, and mandatory MD5 verification.
- Added an OTA maintenance phase that flushes settings, stops nonessential web/BLE/Panda work, releases audio DMA, and preserves a clear full-screen update state.
- Added ESP32 bootloader rollback validation: a new image is marked valid only after 30 seconds of stable display, touch, and LED startup.
- Deferred Arduino's automatic early OTA acceptance so the full hardware validation window now runs before an image is marked valid.
- Reapplied certificate-bundle trust on every redirected TLS connection and verified a complete public GitHub OTA install on hardware.
- Reduced the OTA worker stack and added a short BLE resource window around secure GitHub requests, preserving the contiguous internal-RAM block required by TLS and eliminating intermittent `HTTP -1` checks. BLE resumes automatically after a version check or failed update attempt.
- Added complete firmware-update controls and confirmation dialogs to the Wi-Fi-connected Android companion.
- Added tag-derived firmware versions and automatic GitHub Release assets, checksums, and release publication.

### Boot Experience

- Added a full first-run coroNET Boot Experience synchronized to `boot.wav`, followed by a smooth handoff into setup.
- Added a quiet 3.5-second daily boot with a complete branded splash and no repetitive startup audio.
- Added coordinated screen and RGBW LED transitions into the live printer animation without a hard final-frame cut.
- Refined the quick LED signature so Right, Center, and Left each present a complete color spectrum with continuous tonal boundaries.
- Kept connectivity services active during the quick boot while protecting full-boot LED and audio timing from background work.

### Companion Settings Reliability

- Fixed controls that could appear to work only once when a BLE settings snapshot was delayed or missed.
- Added immediate local settings feedback so button labels, switches, and subsequent actions always use the latest selected value.
- Added a delayed BLE settings reconciliation after edits without allowing an older response to undo a newer choice.
- Serialized WiFi settings writes and guarded periodic refreshes against stale out-of-order responses.
- Fixed the Android companion rejecting every protocol V2 frame as if it were an obsolete V1 frame.
- Added automatic BLE reconnection and a verified WiFi-to-BLE fallback when the preferred local HTTP endpoint is unavailable.
- Kept BLE connected while probing WiFi and prevented failed WiFi polls from replacing a healthy BLE session with an Offline state.

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
