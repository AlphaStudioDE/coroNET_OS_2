#!/usr/bin/env python3
"""Validate firmware LED catalogs against render cases and documentation."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = ("Idle", "Print", "Pause", "Error", "Finish", "Other")


def parse_names(source: str, category: str) -> list[str]:
    match = re.search(
        rf"constexpr const char\* {category}Names\[\] = \{{(.*?)\n\}};",
        source,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"Missing {category}Names")
    return re.findall(r'"([^"]+)"', match.group(1))


def parse_enum_members(header: str, category: str) -> list[str]:
    match = re.search(
        rf"enum class {category}Animation\s*:\s*uint8_t\s*\{{(.*?)\n\}};",
        header,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"Missing {category}Animation enum")
    members = []
    for raw in match.group(1).split(","):
        member = raw.strip().split("=")[0].strip()
        if member and member != "Count":
            members.append(member)
    return members


def parse_documented_names(document: str, category: str) -> list[str]:
    match = re.search(
        rf"^## {category}\s*$\n(.*?)(?=^## |\Z)",
        document,
        re.MULTILINE | re.DOTALL,
    )
    if not match:
        raise AssertionError(f"Missing documentation section: {category}")
    return [
        row.group(1).strip()
        for row in re.finditer(r"^\|\s*\d+\s*\|\s*([^|]+?)\s*\|", match.group(1), re.MULTILINE)
    ]


def main() -> None:
    catalog_source = (ROOT / "src/led/LedAnimations.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/led/LedAnimations.h").read_text(encoding="utf-8")
    renderer = (ROOT / "src/led/LedService.cpp").read_text(encoding="utf-8")
    document = (ROOT / "docs/LED_ANIMATIONS.md").read_text(encoding="utf-8")

    total = 0
    for category in CATEGORIES:
        names = parse_names(catalog_source, category)
        members = parse_enum_members(header, category)
        documented = parse_documented_names(document, category)
        if len(names) != len(members):
            raise AssertionError(f"{category}: {len(names)} names but {len(members)} enum members")
        if names != documented:
            raise AssertionError(f"{category}: firmware and documentation order differ")
        missing = [member for member in members if f"case {category}Animation::{member}:" not in renderer]
        if missing:
            raise AssertionError(f"{category}: missing render cases: {', '.join(missing)}")
        if len(names) != len(set(names)):
            raise AssertionError(f"{category}: duplicate display names")
        total += len(names)
        print(f"{category}: {len(names)} animations")

    print(f"LED catalog validated: {total} animations")


if __name__ == "__main__":
    main()
