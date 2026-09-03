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
- [x] harden BLE framing, command queue, and exact JSON parsing
- [x] secure and pair the local WiFi control API
- [x] add WiFi-to-BLE recovery fallback
- [x] finalize flash partitions before the first public firmware image

## User Experience

- [x] first-run setup wizard
- [x] validated WiFi setup with nearby network selection and connection feedback
- [x] automatic Snapmaker and Moonraker discovery during first setup
- [x] modern home dashboard with live printer, progress, temperature, and connectivity state
- [x] lightweight Home/Settings navigation and initial live Settings controls
- [x] LED, ventilation, and sound tabs
- [x] Coronet, Graphite, Aurora, and Minimal UI skins
- [x] dark and light mode for every skin
- [x] revisioned settings with immediate service application and debounced NVS persistence
- [x] physical pairing reset/confirmation screen
- [x] seven clock styles, screen saver, display-off mode, and printer-error wake
- [ ] accessibility, localization, and touch calibration review

## Companion App

- [x] Android project for OS 2
- [x] discover and remember multiple coroNET devices
- [x] BLE setup, secure token pairing, and fallback connection
- [x] preferred local WiFi control
- [x] bidirectional state and settings synchronization
- [x] printer error and finish notifications
- [x] automatic reconnect, revisioned conflict resolution, and per-device offline cached state

## Hardware Services

- [x] SD card service, bounded WAV index, and asset validation
- [x] modern I2S output driver with PSRAM staging and measured DMA profiles
- [x] non-blocking I2S WAV audio engine
- [x] layered RGBW LED engine
- [x] logical right, center, left, and inside mapping
- [x] ambient, dimming, mirroring, and previews
- [x] full first-run and short daily boot experiences with synchronized screen, LED, audio, and live-state handoff
- [x] printer-aware LED layers and rebuilt OS 1 animation concepts
- [x] PWM fan control
- [x] servo flap control and calibration
- [x] ventilation logic driven exclusively by printer telemetry, with failsafe states
- [ ] Panda Breath discovery and direct control validated on physical hardware
- [x] improved Panda automatic, preheat, tempering, forced-on, and drying state machine

## Connectivity And Updates

- [x] non-blocking printer polling worker with consecutive-failure tolerance
- [x] non-blocking mDNS and local-subnet printer discovery
- [x] resilient printer connection lifecycle and reconnect telemetry
- [x] shared revisioned printer telemetry and transition contract
- [x] event-driven Moonraker WebSocket updates with HTTP integrity fallback
- [x] OTA update service with rollback validity marker
- [x] SD recovery update path
- [x] validated factory Flash Download Tool package
- [x] versioned release metadata, checksums, and changelog

## Future Integration Spotlight: AirGuard 300

**AirGuard 300** is a separate open-source DIY chamber-heating project being developed in parallel by Damian Borkowski. It is intended for broad Klipper compatibility and is being engineered around an uncompromising safety-first architecture, with the ambition of becoming the safest DIY chamber-heating platform in its class.

coroNET OS 2 is planned to become its native visual and control companion. The integration direction includes:

- [ ] automatic AirGuard 300 discovery on the local network
- [ ] authenticated status, telemetry, and command exchange
- [ ] clear presentation of heater state, chamber targets, safety interlocks, and faults
- [ ] coordinated ventilation and chamber-heating behavior without bypassing AirGuard's independent safety layers
- [ ] matching controls on the coroNET touchscreen and Android companion app
- [ ] public protocol documentation so both projects remain open and independently buildable

The protocol, hardware details, and release schedule will be published only after the relevant safety mechanisms have been validated. Until then, AirGuard 300 remains a deliberate glimpse of the wider open-source ecosystem planned around coroNET.

## Public Release

- [ ] long-duration memory, WiFi, BLE, display, audio, and LED soak tests
- [ ] disconnect and power-failure tests
- [x] complete build photography and wiring diagrams
- [x] documented BOM and tested reference hardware
- [x] release binaries and reproducible build notes
- [ ] Snapmaker contest submission material
