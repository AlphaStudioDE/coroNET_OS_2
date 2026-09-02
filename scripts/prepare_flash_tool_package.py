import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import zipfile


FLASH_LAYOUT = (
    (0x0000, "coronet_bootloader.bin"),
    (0x8000, "coronet_partitions.bin"),
    (0xE000, "coronet_boot_app0.bin"),
    (0x10000, "coronet_os2.bin"),
)


def fail(message: str) -> None:
    print(f"flash package preparation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def digest(path: Path, algorithm: str) -> str:
    hasher = hashlib.new(algorithm)
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def find_boot_app0(explicit: Path | None) -> Path:
    if explicit:
        return explicit
    candidate = (
        Path.home()
        / ".platformio"
        / "packages"
        / "framework-arduinoespressif32"
        / "tools"
        / "partitions"
        / "boot_app0.bin"
    )
    return candidate


def create_merged_image(output: Path, files: dict[str, Path]) -> None:
    merged = bytearray()
    for address, name in FLASH_LAYOUT:
        data = files[name].read_bytes()
        if len(merged) > address:
            fail(f"{name} overlaps the previous image at 0x{address:x}")
        merged.extend(b"\xff" * (address - len(merged)))
        merged.extend(data)
    output.write_bytes(merged)


def write_instructions(path: Path, version: str, merged_name: str) -> None:
    path.write_text(
        f"""coroNET OS 2 {version} - Espressif Flash Download Tool

RECOMMENDED: ONE-FILE FACTORY FLASH

1. Download the official Espressif Flash Download Tool:
   https://www.espressif.com/en/support/download/other-tools
2. Start the tool and select chip ESP32-S3 and work mode DEVELOP.
3. Select only this file:
   {merged_name}    address 0x0
4. Enable the checkbox next to the selected file.
5. Use these settings:
   SPI SPEED: 80 MHz
   SPI MODE: DIO
   FLASH SIZE: 16 MB
   DoNotChgBin: enabled
6. Select the correct COM port. Use 921600 baud; use 460800 or 115200 if
   flashing is unreliable.
7. Erase the complete flash before installing this factory image.
8. Click START. Wait for FINISH before disconnecting USB.

IMPORTANT

- The full erase removes Wi-Fi credentials, pairing data, and all coroNET
  settings. The first-start wizard will run again.
- Do not select the merged image together with the individual images.
- Do not flash files at addresses other than those listed here.
- Use a USB data cable and stable power.

ADVANCED: INDIVIDUAL FILES

coronet_bootloader.bin   0x0
coronet_partitions.bin   0x8000
coronet_boot_app0.bin    0xE000
coronet_os2.bin          0x10000

The merged image contains the same four files at the same addresses.
""",
        encoding="utf-8",
        newline="\n",
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a tester-ready coroNET package for Espressif Flash Download Tool"
    )
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("version")
    parser.add_argument("--output-root", type=Path, default=Path("dist"))
    parser.add_argument("--boot-app0", type=Path)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    boot_app0 = find_boot_app0(args.boot_app0).resolve()
    sources = {
        "coronet_bootloader.bin": build_dir / "bootloader.bin",
        "coronet_partitions.bin": build_dir / "partitions.bin",
        "coronet_boot_app0.bin": boot_app0,
        "coronet_os2.bin": build_dir / "firmware.bin",
    }
    for name, source in sources.items():
        if not source.is_file():
            fail(f"missing source for {name}: {source}")

    package_name = f"coroNET_OS_2_{args.version}_Flash_Tool"
    package_dir = args.output_root.resolve() / package_name
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)

    copied: dict[str, Path] = {}
    for name, source in sources.items():
        destination = package_dir / name
        shutil.copy2(source, destination)
        copied[name] = destination

    merged_name = f"coronet_factory_{args.version}.bin"
    merged_path = package_dir / merged_name
    create_merged_image(merged_path, copied)
    copied[merged_name] = merged_path

    instructions = package_dir / "FLASH_TOOL_INSTRUCTIONS.txt"
    write_instructions(instructions, args.version, merged_name)

    manifest = {
        "product": "coroNET OS 2",
        "version": args.version,
        "chip": "ESP32-S3",
        "flashSize": "16MB",
        "flashMode": "DIO",
        "flashFrequency": "80MHz",
        "mergedImage": {"file": merged_name, "address": "0x0"},
        "individualImages": [
            {"file": name, "address": f"0x{address:X}"}
            for address, name in FLASH_LAYOUT
        ],
    }
    (package_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )

    hash_targets = sorted(copied.values(), key=lambda item: item.name)
    (package_dir / "SHA256SUMS.txt").write_text(
        "".join(f"{digest(item, 'sha256')}  {item.name}\n" for item in hash_targets),
        encoding="ascii",
        newline="\n",
    )
    (package_dir / "MD5SUMS.txt").write_text(
        "".join(f"{digest(item, 'md5')}  {item.name}\n" for item in hash_targets),
        encoding="ascii",
        newline="\n",
    )

    zip_path = args.output_root.resolve() / f"{package_name}.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for item in sorted(package_dir.iterdir(), key=lambda entry: entry.name):
            archive.write(item, arcname=f"{package_name}/{item.name}")

    print(f"flash package: {package_dir}")
    print(f"flash package ZIP: {zip_path}")


if __name__ == "__main__":
    main()
