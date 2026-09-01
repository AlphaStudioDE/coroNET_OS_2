# coroNET OS 2

[![Firmware CI](https://github.com/AlphaStudioDE/coroNET_OS_2/actions/workflows/firmware.yml/badge.svg)](https://github.com/AlphaStudioDE/coroNET_OS_2/actions/workflows/firmware.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange)
![Status](https://img.shields.io/badge/status-active%20development-blue)

**coroNET OS 2** is an open-source touchscreen companion controller for 3D printers. It turns printer telemetry into a physical interface with status-aware RGBW lighting, sound, ventilation control, diagnostics, and phone connectivity.

OS 2 is a ground-up successor to [coroNET OS 1](https://github.com/AlphaStudioDE/coroNET_OS_1). It keeps the same physical hardware and GPIO layout while replacing the original monolithic firmware with a modular PlatformIO architecture designed for long-term stability, PSRAM-first memory use, and synchronized control from the touchscreen and Android companion app.

<p align="center">
  <img src="https://raw.githubusercontent.com/AlphaStudioDE/coroNET_OS_1/main/mods/alternative-magnetic-enclosure/photo_front_mounted.jpg" alt="coroNET hardware reference mounted beside a 3D printer" width="760">
</p>

> The photograph shows the shared coroNET hardware running OS 1. OS 2 uses the same enclosure and electronics; dedicated OS 2 product photography will replace it as the new interface reaches release quality.

## Start Here

[Getting started](GETTING_STARTED.md) | [Bill of materials](BOM.md) | [Assembly and wiring](ASSEMBLY.md) | [Flashing](FLASHING.md) | [Roadmap](ROADMAP.md) | [Architecture](docs/ARCHITECTURE.md) | [Contributing](CONTRIBUTING.md)

## What The Finished Device Is Designed To Do

- display printer state, progress, temperatures, active tool, material, job time, and connectivity;
- drive 60 SK6812 RGBW LEDs as logical right, center, left, and inside sections;
- render printer-aware animations from progress, filament color, temperatures, and events;
- play startup, status, finish, warning, and interaction sounds from microSD;
- control a PWM fan and servo-driven ventilation flap;
- provide setup, diagnostics, configuration, and OTA updates on a 3.5-inch touchscreen;
- synchronize settings and live state with an Android app over BLE or local WiFi;
- support multiple coroNET devices in one companion app;
- keep large buffers in PSRAM and reserve internal/DMA memory for hardware-critical work.

## Current Development Status

The repository is public early so that the architecture, documentation, hardware assumptions, and development history are visible from the beginning.

| Area | Status |
| --- | --- |
| PlatformIO project and MIT licensing | Working |
| JC3248W535 display and touch | Working on hardware |
| PSRAM canvas and small DMA transfer windows | Working on hardware |
| Versioned NVS settings | Working, with live apply and write debounce |
| Unique device identity and BLE peripheral | Working, framed protocol V1 |
| Local WiFi HTTP API and mDNS | Token-authenticated implementation |
| Moonraker HTTP polling | Background worker implementation |
| Audio playback | Service boundary only |
| Final touchscreen UI and setup wizard | In development |
| Android companion app for OS 2 | Planned |
| Layered RGBW LED engine | Planned |
| Fan, servo, OTA, SD assets, and release installer | Planned |

Development builds are not yet end-user releases. The public flashing package will appear under [GitHub Releases](https://github.com/AlphaStudioDE/coroNET_OS_2/releases) after the release path is validated.

## Hardware

- JC3248W535 ESP32-S3 touchscreen controller, 16 MB flash and OPI PSRAM;
- SK6812 RGBNW strip, 60 LEDs;
- 5 V / 10 A power supply;
- 4 ohm / 3 W speaker using the board's integrated audio output;
- 5 V PWM fan and micro servo;
- 1000 uF / 10 V capacitor at the LED power input;
- printed enclosure and mounting parts from [hardware/print/coroNET.3mf](hardware/print/coroNET.3mf).

See [BOM.md](BOM.md) before purchasing and [ASSEMBLY.md](ASSEMBLY.md) before applying power.

## Build The Firmware

Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode), clone this repository, and run:

```powershell
pio run
pio run -t upload
pio device monitor
```

The target board definition is included in the repository. Detailed instructions and recovery notes are in [FLASHING.md](FLASHING.md).

## Repository Layout

```text
boards/       custom PlatformIO board definition
docs/         architecture, protocol, hardware, and technical notes
hardware/     printable mechanical files and hardware documentation
include/      project and LVGL configuration
src/          modular firmware services and local BSP
.github/      automated builds and project templates
```

Generated firmware binaries are intentionally excluded from Git history. Tested binaries and factory flashing packages will be attached to versioned GitHub Releases.

## Open Source

coroNET OS 2 is licensed under the [MIT License](LICENSE). Source code, documentation, and project-owned files in this repository may be used, modified, and redistributed under those terms unless a file explicitly states separate third-party terms.

Contributions, hardware validation, documentation corrections, and reproducible bug reports are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

## Safety

This project controls lighting, motors, fans, network-connected printer data, and high-current 5 V distribution. Verify polarity, common ground, insulation, wire current ratings, servo limits, fan behavior, and LED power before unattended use. Never power a heater, fan, servo, or LED strip directly from an ESP32 GPIO.

## Credits

Created and maintained by **Damian Borkowski**.

Special thanks to **@wlodeka** on Discord for hardware testing, feedback, and the optional magnetic enclosure developed for coroNET OS 1.

coroNET is an independent community project and is not affiliated with or endorsed by Snapmaker. Snapmaker and other product names may be trademarks of their respective owners.
