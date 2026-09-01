# Getting Started With coroNET OS 2

This guide is the shortest path from the repository to a running development build. coroNET OS 2 is currently under active development; a tested end-user factory image will be published through GitHub Releases when the release path is ready.

## 1. Understand The Current Stage

The same physical coroNET hardware used by OS 1 is supported. Display, touch, PSRAM allocation, BLE, WiFi service boundaries, local HTTP control, and initial printer polling are present. The final UI, LED engine, complete audio, ventilation, OTA, and Android app are still being built.

Do not use a development build for unattended control.

## 2. Prepare The Hardware

Read [BOM.md](BOM.md) before purchasing. The core development setup requires:

- JC3248W535 ESP32-S3 touchscreen controller;
- USB data cable;
- 5 V power appropriate for the connected loads;
- optional LEDs, speaker, fan, and servo as each subsystem is implemented.

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

## 5. Verify The Bring-Up Screen

After a successful flash:

- the display should show the coroNET OS 2 bring-up screen;
- touch input should update the touch counter;
- PSRAM should report ready;
- BLE should advertise a unique name such as `coroNET_0F3C`;
- the serial monitor should print periodic memory and service health information.

The normal Android Bluetooth settings screen is not a reliable BLE GATT test tool. Use a BLE scanner such as nRF Connect until the OS 2 companion app is available.

## 6. Report A Problem

Before opening an issue, collect:

- the exact board and wiring used;
- the Git commit hash from `git rev-parse --short HEAD`;
- complete PlatformIO build output;
- serial output from reset through the failure;
- whether the same behavior occurs with LEDs, fan, servo, and speaker disconnected.

Use the repository issue tracker and never include WiFi passwords, printer API keys, or other secrets in logs.
