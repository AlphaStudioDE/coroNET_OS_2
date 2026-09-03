# Getting Started With coroNET OS 2

This guide is the shortest path to a running coroNET OS 2 installation. New devices can use a tested factory package from [GitHub Releases](https://github.com/AlphaStudioDE/coroNET_OS_2/releases), while contributors can build the same source with PlatformIO.

## 1. Choose An Installation Path

The same physical coroNET hardware used by OS 1 is supported. Display, touch, setup, Moonraker telemetry, RGBW lighting, SD audio, local ventilation, Panda Breath workflows, OTA, BLE, local WiFi control, and the Android companion are integrated in OS 2.

For a new installation or recovery, download the latest `coroNET_OS_2_<version>_Flash_Tool.zip` release asset and follow its included instructions. Use the PlatformIO steps below for development builds. Read [FLASHING.md](FLASHING.md) before changing an existing installation.

## 2. Prepare The Hardware

Read [BOM.md](BOM.md) before purchasing. The core development setup requires:

- JC3248W535 ESP32-S3 touchscreen controller;
- USB data cable;
- 5 V power appropriate for the connected loads;
- SK6812 RGBNW LEDs, speaker, fan, and servo when building the complete device.

For a complete physical build, print [hardware/print/coroNET.3mf](hardware/print/coroNET.3mf) and follow [ASSEMBLY.md](ASSEMBLY.md).

## 3. Install The Toolchain

Recommended setup:

1. Install [Git](https://git-scm.com/downloads).
2. Install [Visual Studio Code](https://code.visualstudio.com/).
3. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode).
4. Clone the repository:

```powershell
git clone https://github.com/AlphaStudioDE/coroNET_OS_2.git
cd coroNET_OS_2
```

PlatformIO downloads the pinned ESP32 platform and required libraries during the first build.

## 4. Build And Flash

Connect the JC3248W535 with a USB data cable, then run:

```powershell
pio run
pio run -t upload
pio device monitor
```

If more than one serial device is connected, specify the port:

```powershell
pio run -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Replace `COM3` with the port shown on your computer. See [FLASHING.md](FLASHING.md) for recovery mode and build artifacts.

## 5. Complete Setup And Verify Operation

After a successful flash:

- a factory-reset device should play the full Boot Experience and open Setup Wizard;
- a configured device should use the short daily boot and open Home;
- touch, display, LEDs, SD audio, and ventilation controls should respond from their matching screens;
- BLE should advertise a unique name such as `coroNET_0F3C`;
- WiFi setup should confirm the network before printer discovery;
- Home should show live printer state after Moonraker connects;
- the serial monitor should print periodic memory and service health information without repeated resets.

Pair the Android companion from the physical **Settings > Companion connection > Pair phone** flow. Android's Bluetooth settings screen alone does not complete coroNET's authenticated GATT pairing.

## 6. Report A Problem

Before opening an issue, collect:

- the exact board and wiring used;
- the Git commit hash from `git rev-parse --short HEAD`;
- complete PlatformIO build output;
- serial output from reset through the failure;
- whether the same behavior occurs with LEDs, fan, servo, and speaker disconnected.

Use the repository issue tracker and never include WiFi passwords, printer API keys, or other secrets in logs.
