# coroNET OS 2 Roadmap

This roadmap describes direction, not a promise of release dates. Features move to complete only after they build successfully and are validated on the target hardware.

## Foundation

- [x] MIT-licensed PlatformIO repository
- [x] custom JC3248W535 board definition
- [x] local display and touch BSP
- [x] PSRAM-first allocation policy
- [x] LVGL canvas in PSRAM with bounded DMA transfer buffers
- [x] versioned NVS settings skeleton
- [x] unique hardware-derived device identity
- [x] initial NimBLE peripheral and local WiFi API
- [x] initial Moonraker HTTP polling
- [x] automated GitHub firmware build
- [ ] harden BLE framing, command queue, and exact JSON parsing
- [ ] secure and pair the local WiFi control API
- [ ] add WiFi-to-BLE recovery fallback
- [ ] finalize flash partitions before the first public firmware image

## User Experience

- [ ] first-run setup wizard
- [ ] modern home dashboard
- [ ] Coronet, Graphite, Aurora, and Minimal UI skins
- [ ] dark and light mode for every skin
- [ ] settings shared consistently between display, BLE, and WiFi
- [ ] screen saver and low-power behavior
- [ ] accessibility, localization, and touch calibration review

## Companion App

- [ ] Android project for OS 2
- [ ] discover and remember multiple coroNET devices
- [ ] BLE setup and fallback connection
- [ ] preferred local WiFi control
- [ ] bidirectional state and settings synchronization
- [ ] printer error and finish notifications
- [ ] reconnect, conflict resolution, and offline cached state

## Hardware Services

- [ ] SD card service and asset validation
- [ ] non-blocking I2S WAV audio engine
- [ ] layered RGBW LED engine
- [ ] logical right, center, left, and inside mapping
- [ ] ambient, dimming, mirroring, previews, and boot show
- [ ] printer-aware LED layers and selected OS 1 animation concepts
- [ ] PWM fan control
- [ ] servo flap control and calibration
- [ ] ventilation logic and failsafe states

## Connectivity And Updates

- [ ] resilient printer connection lifecycle
- [ ] event-driven printer updates where supported
- [ ] OTA update service with rollback
- [ ] SD recovery update path
- [ ] factory flashing package
- [ ] versioned release metadata and changelog

## Public Release

- [ ] long-duration memory, WiFi, BLE, display, audio, and LED soak tests
- [ ] disconnect and power-failure tests
- [ ] complete build photography and wiring diagrams
- [ ] validated BOM and tested alternatives
- [ ] release binaries and reproducible build notes
- [ ] Snapmaker contest submission material
