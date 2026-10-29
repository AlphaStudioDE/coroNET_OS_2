# Local regression checks

These checks do not connect to an ESP32, printer, heater, phone or public service.
Run from the repository root after PlatformIO has installed project dependencies:

```text
node tests/runtime-regression.mjs
node tests/display-transfer-regression.mjs
node tests/web-regression.mjs
```

The C++ runners require a C++17 compiler (`g++`, or the `CXX` environment variable).
They extract production functions at run time and compile them with deterministic
RTOS, filesystem, I2S and LCD substitutes. ArduinoJson comes from the pinned
PlatformIO dependency. Generated source/executables stay in ignored `.pio/host-tests`.
The web runner executes the actual inline functions with controlled asynchronous
responses; it also syntax-checks the entire inline script.

From `android/`, run `./gradlew.bat testDebugUnitTest lintDebug assembleDebug`.
BLE framing tests call the production `BleFrameAssembler`, including 255 chunks,
4096-byte payloads, duplicate/out-of-order frames, expiry and session clearing.
Settings tests call the production parser, not a duplicate implementation.

These are regression tests, not hardware qualification. In particular, mocked
timeouts do not measure real task scheduling, SPI/DMA timing, audio quality,
Bluetooth callback timing or heater behavior. See the audit's `FIXES.md` for the
physical test gate before any release.
