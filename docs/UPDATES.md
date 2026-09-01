# Development Updates

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
