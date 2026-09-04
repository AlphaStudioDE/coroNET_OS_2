# coroNET OS 2 Roadmap

This roadmap describes direction, not a promise of release dates. Features move to complete only after they build successfully and are validated on the target hardware. Release `0.4.4` continues structured physical refinement with the complete firmware, Android companion, and browser panel aligned; the remaining path is organized around hardware testing, polish, and formal 1.0 qualification.

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
- [x] true 2 FPS LED output preview over WiFi and BLE

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

## Release Path

### 0.3.3 - Software Audit Baseline

- [x] complete cross-project audit of firmware, Android companion, and browser panel
- [x] validate the complete 336-animation LED catalog against names, enums, render cases, and documentation
- [x] make remote settings updates transactional across BLE and WiFi
- [x] harden Android network handling, permission feedback, and pairing-token storage
- [x] add Android unit tests and run firmware, Android, and LED validation in CI
- [x] publish matching OTA firmware, Flash Tool package, checksums, and Android APK

### 0.4.x - Physical Refinement And Corrections

- [x] publish the first coordinated `0.4.2` physical-validation build for firmware, Android, and browser control
- [x] add firmware-driven 2 FPS LED previews to Android and the browser without additional DMA allocation
- [x] add local two-hour temperature history with per-series controls and active-tool emphasis
- [x] redesign the Android companion for portrait phones and make discovery scale to multiple coroNET devices
- [x] harden rapid sound selection and move growing history persistence away from interactive UI work
- [x] run the physical LED engine at a real 50 FPS with time-interpolated waves and subpixel motion
- [x] add bounded missed-frame handling so delayed LED work cannot produce catch-up flashes
- [x] fade manual audio stops and track changes to digital silence before releasing I2S
- [x] keep the display backlight off until the first complete LVGL boot frame is ready
- [ ] exercise every touchscreen, Android, and browser workflow on physical hardware
- [ ] visually inspect every LED animation for intent, direction, transitions, color, and brightness
- [ ] test simultaneous control from the touchscreen, Android companion, and browser panel
- [ ] validate BLE/WiFi recovery, Moonraker reconnect, OTA, SD audio, ventilation, and pairing resets
- [ ] correct every functional issue found during physical testing
- [ ] validate Panda Breath discovery and direct control on physical hardware

### 0.5.x - Final Experience Polish

- [ ] complete visual and interaction polish on the coroNET touchscreen
- [ ] complete visual and interaction polish in the Android companion
- [ ] complete visual and interaction polish in the local browser panel
- [ ] finish accessibility, localization, touch-target, and touch-calibration review
- [ ] align wording, hierarchy, feedback, empty states, and error presentation across all three interfaces

### 1.0.0 - Production Qualification Milestone

- [ ] complete long-duration memory, WiFi, BLE, display, audio, LED, and control soak tests
- [ ] complete repeated disconnect, restart, update, brownout, and power-failure recovery tests
- [ ] resolve all release-blocking defects and repeat the affected stability tests
- [x] complete build photography and wiring diagrams
- [x] document the BOM and reference hardware
- [x] provide release binaries and reproducible build notes
- [ ] publish coroNET OS 2 `1.0.0` only after the stability gate passes
- [ ] prepare Snapmaker contest submission material
