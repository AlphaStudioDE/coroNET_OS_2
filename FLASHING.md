# Flashing coroNET OS 2

coroNET OS 2 uses PlatformIO for development builds. Versioned firmware and checksums are published through [GitHub Releases](https://github.com/AlphaStudioDE/coroNET_OS_2/releases).

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

## Espressif Flash Download Tool Package

Tester and factory packages contain a merged image that can be written at `0x0` with the official [Espressif Flash Download Tool](https://www.espressif.com/en/support/download/other-tools). This is the recommended method for installing coroNET OS 2 on a new board or recovering a board with an unknown partition table.

The package also includes the equivalent individual images:

| Address | Image |
| --- | --- |
| `0x0` | `coronet_bootloader.bin` |
| `0x8000` | `coronet_partitions.bin` |
| `0xE000` | `coronet_boot_app0.bin` |
| `0x10000` | `coronet_os2.bin` |

Use the included `FLASH_TOOL_INSTRUCTIONS.txt`. Do not select the merged image and individual images together. A complete erase is intentional for a factory flash and removes Wi-Fi credentials, companion pairing, and device settings.

To create a package from a tested local build:

```powershell
$env:CORONET_FIRMWARE_VERSION = "0.4.5"
pio run
python scripts/validate_release_artifacts.py .pio/build/coronet_os2 $env:CORONET_FIRMWARE_VERSION
python scripts/prepare_flash_tool_package.py .pio/build/coronet_os2 $env:CORONET_FIRMWARE_VERSION
```

Generated packages are placed under `dist/` and are intentionally excluded from Git.

## Over-The-Air Updates

Once a full development or factory image is installed, later application updates can be installed from **Settings > Firmware update** on coroNET or from the Android companion while it is connected over local Wi-Fi.

The updater:

- checks the latest published GitHub Release over certificate-verified HTTPS;
- compares semantic firmware versions and rejects downgrades;
- requires `coronet_os2.bin` and its `coronet_os2.bin.md5` checksum;
- verifies the release asset size, ESP32 image header, and complete MD5 before accepting the image;
- stops nonessential services and releases audio DMA before writing the inactive OTA slot;
- restarts into the new slot and marks it valid only after a 30-second hardware startup validation window;
- lets the ESP32 bootloader roll back when the updated application cannot complete validation.

`INSTALL` accepts a newer release. `REINSTALL` intentionally permits the same published version. OTA cannot change the partition table, bootloader, or flash layout.

For offline recovery, place the release application image on the SD card as `/firmware.bin` and use **SD RECOVERY**. After a successful installation it is renamed to `/firmware.applied` so the device cannot reinstall it repeatedly.

## Creating A Release

The GitHub Actions workflow builds every commit. Pushing a semantic version tag creates or updates the matching GitHub Release and uploads the exact assets expected by the updater:

```powershell
git tag v0.1.0
git push origin v0.1.0
```

The tag determines the version embedded in the firmware. A tagged release contains:

- `coronet_os2.bin` and `coronet_os2.bin.md5` for OTA and SD recovery;
- `coronet_bootloader.bin`;
- `coronet_partitions.bin`;
- `coroNET_OS_2_<version>_Flash_Tool.zip` with a merged factory image and individual binaries;
- `SHA256SUMS.txt` for independent download verification.

Create a release tag only from a tested commit. Publishing a tag makes that firmware visible to installed devices.

## Release Distribution Policy

- Source code and documentation live in the Git repository.
- Every commit and pull request is compiled by GitHub Actions.
- CI artifacts are for development verification.
- Tested user-installable binaries belong in a versioned GitHub Release with generated checksums.
- Factory packages include a merged image, individual images, exact addresses, and SHA-256/MD5 checksums for Espressif Flash Download Tool.
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
