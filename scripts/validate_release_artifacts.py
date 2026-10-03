import argparse
from pathlib import Path
import struct
import sys


ESP_IMAGE_MAGIC = 0xE9
ESP_FLASH_MODE_DIO = 0x02
MIN_FIRMWARE_SIZE = 2_000_000
MAX_FIRMWARE_SIZE = 0x600000
FLASH_SIZE = 0x1000000
PARTITION_ENTRY_MAGIC = 0x50AA
EXPECTED_PARTITIONS = {
    "nvs": (0x01, 0x02, 0x009000, 0x005000),
    "otadata": (0x01, 0x00, 0x00E000, 0x002000),
    "app0": (0x00, 0x10, 0x010000, 0x600000),
    "app1": (0x00, 0x11, 0x610000, 0x600000),
    "spiffs": (0x01, 0x82, 0xC10000, 0x300000),
    "coredump": (0x01, 0x03, 0xF10000, 0x010000),
}


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


def validate_partitions(path: Path) -> None:
    if not path.is_file() or path.stat().st_size != 0xC00:
        fail("partitions.bin is missing or has an unexpected size")

    data = path.read_bytes()
    actual = {}
    ranges = []
    for offset in range(0, len(data), 32):
        magic, part_type, subtype, address, size, raw_label, flags = struct.unpack_from(
            "<HBBII16sI", data, offset
        )
        if magic != PARTITION_ENTRY_MAGIC:
            break
        label = raw_label.split(b"\0", 1)[0].decode("ascii", errors="strict")
        if not label or label in actual:
            fail(f"invalid or duplicate partition label {label!r}")
        if size == 0 or address + size > FLASH_SIZE:
            fail(f"partition {label} exceeds the 16 MB flash boundary")
        actual[label] = (part_type, subtype, address, size)
        ranges.append((address, address + size, label))
        if flags != 0:
            fail(f"partition {label} has unexpected flags 0x{flags:x}")

    if actual != EXPECTED_PARTITIONS:
        fail(f"partition layout differs from the required coroNET layout: {actual}")
    ranges.sort()
    for previous, current in zip(ranges, ranges[1:]):
        if previous[1] > current[0]:
            fail(f"partitions {previous[2]} and {current[2]} overlap")


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

    validate_partitions(build_dir / "partitions.bin")
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
