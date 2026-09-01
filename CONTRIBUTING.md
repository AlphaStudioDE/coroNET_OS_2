# Contributing To coroNET OS 2

Thank you for helping improve coroNET OS 2. The project welcomes code, documentation, hardware testing, mechanical improvements, and focused bug reports.

## Before You Start

- Read [README.md](README.md), [ROADMAP.md](ROADMAP.md), and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- Search existing issues before opening a duplicate.
- Discuss large architectural or hardware changes in an issue before investing substantial work.
- Keep changes focused; unrelated cleanup belongs in a separate pull request.

## Development Setup

```powershell
git clone https://github.com/AlphaStudioDE/coroNET_OS_2.git
cd coroNET_OS_2
pio run
```

Code must compile for the `coronet_os2` environment. Hardware-facing changes should include the board revision, wiring, and serial evidence used for testing.

## Project Rules

- Preserve the physical GPIO contract unless a new hardware revision is explicitly introduced.
- Allocate large, non-DMA data in PSRAM when practical.
- Reserve DMA/internal RAM for peripheral requirements, small critical objects, and task stacks.
- Access LVGL only while holding the display lock.
- Do not block audio, LED rendering, or the UI with long network operations.
- Use structured parsing for structured protocols.
- Never commit WiFi passwords, printer API keys, personal tokens, or generated local settings.
- Update documentation when behavior, wiring, settings, or public APIs change.

## Commit And Pull Request Style

Use short, descriptive commits, for example:

```text
feat: add WiFi companion discovery
fix: protect BLE command queue across tasks
docs: clarify LED power wiring
```

A pull request should explain:

- what changed and why;
- how it was tested;
- hardware used;
- memory or timing impact when relevant;
- screenshots or serial logs for visible/runtime changes.

## Mechanical Contributions

Include editable source when possible, exported printable files, print orientation/settings, required hardware, photographs, and clear licensing. Community files are not automatically covered by the repository MIT license unless their author explicitly contributes them under compatible terms.

## License

By contributing, you agree that your contribution may be distributed under the repository's [MIT License](LICENSE), unless an explicitly reviewed third-party file states separate compatible terms.
