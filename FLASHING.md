# Flashing coroNET OS 2

coroNET OS 2 currently uses PlatformIO for building and flashing development firmware. Tested factory images and a simplified end-user flashing package will be attached to versioned [GitHub Releases](https://github.com/AlphaStudioDE/coroNET_OS_2/releases) when the release process is ready.

## Recommended Development Flash

Install [PlatformIO](https://platformio.org/install/ide?install=vscode), clone the repository, and connect the JC3248W535 with a USB data cable.

Build:

```powershell
pio run
```

Upload:

```powershell
pio run -t upload
```

When multiple ports are present:

```powershell
pio device list
pio run -t upload --upload-port COM3
```

Replace `COM3` with the USB serial port assigned to the ESP32-S3.

Open the serial monitor:

```powershell
pio device monitor --port COM3 --baud 115200
```

## Enter The ESP32-S3 Bootloader

If upload cannot connect:

1. Hold `BOOT`.
2. Press and release `RESET`.
3. Release `BOOT`.
4. Start the upload again.

Also try a different USB data cable, close other serial monitors, and lower `upload_speed` if the connection remains unreliable.

## Build Outputs

After `pio run`, PlatformIO creates the following development artifacts under `.pio/build/coronet_os2/`:

- `bootloader.bin`;
- `partitions.bin`;
- `firmware.bin`;
- `firmware.elf` and `firmware.map` for diagnostics.

The `.pio` directory is intentionally excluded from Git. Build products are reproducible outputs, not source files.

The current 16 MB flash layout reserves two 6 MB OTA application slots, 3 MB for the local filesystem, and dedicated NVS, OTA metadata, and coredump partitions. Changing from an older development partition table requires a complete USB flash that includes `partitions.bin`; an application-only OTA cannot safely redefine its own slot layout.

## Release Distribution Policy

- Source code and documentation live in the Git repository.
- Every commit and pull request is compiled by GitHub Actions.
- CI artifacts are for development verification.
- Tested user-installable binaries belong in a signed/versioned GitHub Release.
- A future factory package will include all images and exact addresses required by Espressif Flash Download Tool.
- Do not download random firmware binaries from issue attachments or unofficial mirrors.

Official Espressif tools:

- [Flash Download Tools](https://www.espressif.com/en/tools-type/flash-download-tools)
- [ESP32-S3 Flash Download Tool guide](https://docs.espressif.com/projects/esp-test-tools/en/latest/esp32s3/production_stage/tools/flash_download_tool.html)

## Flashing Safety

- Use stable USB and 5 V power.
- Disconnect questionable external loads while diagnosing reset loops.
- Keep the serial log from reset through the failure.
- Never enable secure boot, flash encryption, or encrypted download unless following a dedicated coroNET release procedure.
- A full erase removes NVS settings such as WiFi and device configuration.
