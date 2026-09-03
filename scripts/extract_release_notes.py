#!/usr/bin/env python3

import argparse
from pathlib import Path


def release_section(document: str, version: str) -> str:
    heading = f"## {version}"
    lines = document.splitlines()
    try:
        start = lines.index(heading) + 1
    except ValueError as error:
        raise SystemExit(f"release notes section not found: {heading}") from error

    end = len(lines)
    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break

    notes = "\n".join(lines[start:end]).strip()
    if not notes:
        raise SystemExit(f"release notes section is empty: {heading}")
    return notes + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract one release section from UPDATES.md")
    parser.add_argument("source", type=Path)
    parser.add_argument("version")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    notes = release_section(args.source.read_text(encoding="utf-8"), args.version)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(notes, encoding="utf-8", newline="\n")
    print(f"release notes: {args.output} ({args.version})")


if __name__ == "__main__":
    main()
