Import("env")

from pathlib import Path
import gzip


project_dir = Path(env.subst("$PROJECT_DIR"))
source = project_dir / "web" / "index.html"
target = project_dir / "include" / "generated" / "WebUiAsset.h"


def render_header(payload: bytes) -> str:
    rows = []
    for offset in range(0, len(payload), 16):
        chunk = payload[offset:offset + 16]
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return """#pragma once

#include <Arduino.h>

namespace coronet::webui {

static const uint8_t IndexHtmlGzip[] PROGMEM = {
%s
};

static constexpr size_t IndexHtmlGzipSize = sizeof(IndexHtmlGzip);

}
""" % "\n".join(rows)


def generate(*_):
    raw = source.read_bytes()
    payload = gzip.compress(raw, compresslevel=9, mtime=0)
    content = render_header(payload)
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_text(encoding="utf-8") != content:
        target.write_text(content, encoding="utf-8", newline="\n")
    print(f"[web-ui] {len(raw)} bytes -> {len(payload)} bytes gzip")


generate()
