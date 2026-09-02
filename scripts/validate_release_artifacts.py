import argparse
from pathlib import Path
import sys


ESP_IMAGE_MAGIC = 0xE9
ESP_FLASH_MODE_DIO = 0x02
MIN_FIRMWARE_SIZE = 2_000_000
MAX_FIRMWARE_SIZE = 0x600000


def fail(message: str) -> None:
    print(f"release artifact validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def validate_image(path: Path, *, minimum: int, maximum: int) -> bytes:
    if not path.is_file():
        fail(f"missing {path}")
    data = path.read_bytes()
    if not minimum <= len(data) <= maximum:
        fail(f"{path.name} has unexpected size {len(data)} bytes")
    if len(data) < 4 or data[0] != ESP_IMAGE_MAGIC:
        fail(f"{path.name} is not an ESP application image")
    if data[2] != ESP_FLASH_MODE_DIO:
        fail(f"{path.name} uses flash mode 0x{data[2]:02x}, expected DIO")
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate coroNET release build outputs")
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("version")
    args = parser.parse_args()

    build_dir = args.build_dir
    firmware = validate_image(
        build_dir / "firmware.bin",
        minimum=MIN_FIRMWARE_SIZE,
        maximum=MAX_FIRMWARE_SIZE,
    )
    validate_image(build_dir / "bootloader.bin", minimum=8_000, maximum=64_000)

    partitions = build_dir / "partitions.bin"
    if not partitions.is_file() or partitions.stat().st_size != 0xC00:
        fail("partitions.bin is missing or has an unexpected size")
    if not (build_dir / "src" / "main.cpp.o").is_file():
        fail("application source objects are missing; refusing a helper/dummy build")
    if args.version.encode("ascii") not in firmware:
        fail(f"firmware does not contain expected version {args.version}")

    print(
        "release artifacts OK: "
        f"firmware={len(firmware)}B version={args.version} flash-mode=DIO"
    )


if __name__ == "__main__":
    main()
