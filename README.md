# coroNET OS 2

coroNET OS 2 is the open-source successor to coroNET 1, rewritten from the ground up for the same hardware platform with a cleaner architecture, modern UI direction, and phone companion support designed from day one.

This project is licensed under the MIT License.

## Goals

- Keep the same physical coroNET hardware and GPIO layout as coroNET 1.
- Move from Arduino IDE monolith development to a modular PlatformIO project.
- Build a stable foundation first: display, touch, audio, WiFi, BLE, and local companion control.
- Use NimBLE for the Android companion connection instead of the heavier classic ESP32 BLE library.
- Support multiple coroNET devices in the companion app through a stable hardware-derived device ID, user-visible names, BLE setup, and WiFi control.
- Design the new LED engine as a layered system, inspired by the best coroNET 1 effects but built cleanly for OS 2.
- Keep memory ownership explicit: PSRAM for large allocations, DMA only where hardware requires it.
- Support multiple UI skins, each with light and dark variants, starting with the original simple Coronet look.

## Current Status

Early architecture skeleton with a real JC3248W535 display/touch bring-up screen, system state, hardware configuration, PSRAM-first memory policy, BLE/WiFi/audio/display service boundaries, Moonraker polling, and a local WiFi HTTP API for the future Android companion app.

## Development

Build with PlatformIO:

```powershell
pio run
```

Upload:

```powershell
pio run -t upload
```

Serial monitor:

```powershell
pio device monitor
```
