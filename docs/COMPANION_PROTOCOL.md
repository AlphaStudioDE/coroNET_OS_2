# coroNET OS 2 Companion Protocol

The Android companion app controls coroNET through structured state and commands, not screen streaming. BLE is the setup and recovery channel. Authenticated local WiFi is the preferred channel for normal use because it is faster and better suited to complete application screens.

The current protocol version is `2`. Multi-byte binary fields use little-endian byte order.

## Device Discovery

- Scan for service UUID `7b7e0001-9f2a-4f3c-8d2a-c0a0e7c0ffee`.
- Request an ATT MTU of `247` before subscribing. The protocol also fragments messages for smaller negotiated MTUs.
- Do not identify a saved device by its Bluetooth name. The default name is `coroNET_XXXX`, but the user may change it.
- Use the stable 12-character ESP32-derived `deviceId` contained in state and pairing messages.
- The local hostname is `coronet-xxxx.local`, where `xxxx` is the generated device suffix.
- Android's system Bluetooth settings are not a GATT client. Use the coroNET app, nRF Connect, or another BLE development client.

The app must maintain separate state, settings, token, hostname, and notification history for every saved `deviceId`.

## GATT Characteristics

| Purpose | UUID | Properties |
| --- | --- | --- |
| Live state | `7b7e0002-9f2a-4f3c-8d2a-c0a0e7c0ffee` | Read, Notify |
| Commands | `7b7e0003-9f2a-4f3c-8d2a-c0a0e7c0ffee` | Write, Write Without Response |
| Events/settings/pairing | `7b7e0004-9f2a-4f3c-8d2a-c0a0e7c0ffee` | Read, Notify |

## Notification Framing

Every notification starts with this packed 8-byte header:

| Offset | Type | Field |
| --- | --- | --- |
| 0 | `uint8` | protocol version |
| 1 | `uint8` | message type |
| 2 | `uint16` | message ID |
| 4 | `uint16` | complete payload length |
| 6 | `uint8` | zero-based chunk index |
| 7 | `uint8` | total chunk count |

Message types are:

- `1`: binary state snapshot;
- `2`: JSON event;
- `3`: JSON settings snapshot;
- `4`: JSON pairing response.

The firmware limits an ATT notification to `min(244, negotiatedMtu - 3)` bytes. The app must group chunks by characteristic, message type, and message ID, reject duplicate or out-of-range chunks, verify the reconstructed length, and discard incomplete messages after a timeout. A payload must not be parsed before every chunk is present.

## Binary State Snapshot V2

Message type `1` contains this packed payload in order:

| Type | Field |
| --- | --- |
| `uint8` | protocol version |
| `uint8` | message type (`1`) |
| `uint16` | structure size |
| `uint32` | state revision |
| `uint32` | uptime in milliseconds |
| `uint16` | state flags |
| `uint8` | printer state enum |
| `uint8` | print progress, `0..100` |
| `uint8` | active tool, `0..3` |
| `uint8` | reserved |
| `int16` | active tool temperature in tenths of a degree |
| `int16` | bed temperature in tenths of a degree |
| `int16` | chamber temperature in tenths of a degree |
| `char[13]` | null-terminated device ID |
| `char[25]` | null-terminated device name |
| `char[48]` | null-terminated printer status |
| `char[65]` | null-terminated print filename |
| `uint32` | printer telemetry revision |
| `uint32` | printer state-event sequence |
| `uint8` | previous printer state for the latest event |
| `uint8` | destination printer state for the latest event |

The V2 snapshot is 185 bytes. Its first 175 bytes retain the V1 field order. Temperature value `-32768` means unavailable.

Flag bits are: setup done `0`, WiFi connected `1`, web API ready `2`, BLE connected `3`, display ready `4`, touch ready `5`, printer configured `6`, printer connected `7`, audio ready `8`, temporary BLE fallback active `9`, and valid printer telemetry `10`.

Printer state values follow firmware enum order: unknown `0`, idle `1`, printing `2`, paused `3`, error `4`, and complete `5`.

The event sequence changes only for a genuine state transition observed during uninterrupted valid telemetry. Initial connection and reconnection establish a baseline without manufacturing an event. Clients should baseline the first sequence they receive and notify only when a later sequence changes; this prevents duplicate Error or Finish notifications after transport switching or reconnecting.

## Commands

Commands are UTF-8 JSON objects. The firmware parses the exact `cmd` field with ArduinoJson; command names found inside an SSID, device name, or another value are ignored. `snapshot`, `ping`, and `getSettings` are also accepted as exact legacy plaintext commands during bring-up.

```json
{"cmd":"setDeviceName","name":"Workshop coroNET"}
```

```json
{"cmd":"resetDeviceName"}
```

```json
{"cmd":"setWifi","ssid":"Workshop WiFi","password":"secret"}
```

```json
{"cmd":"setPrinter","host":"192.168.1.50","port":7125,"apiKey":""}
```

```json
{"cmd":"testPrinterConnection"}
```

```json
{"cmd":"setSetupDone","done":true}
```

```json
{"cmd":"setCompanionTransport","mode":"auto"}
```

Valid transport values are `auto`, `ble`, and `wifi`.

Operational controls can send a bounded settings patch over BLE. Only supplied fields are changed:

```json
{"cmd":"setSettings","ledEnabled":true,"ledBrightness":[70,80,70,45]}
```

Clock presentation and the POSIX time-zone rule use the same patch command:

```json
{"cmd":"setSettings","clock24Hour":false,"timeZone":"CET-1CEST,M3.5.0,M10.5.0/3"}
```

The same field names are accepted by `POST /api/settings`. BLE accepts appearance, screen saver, quiet mode, LED brightness/policy, sound volume, local vent calibration, and Panda mode controls. Larger configuration changes should be split into small patches so each command remains below the 384-byte command limit.

## Secure Companion Pairing

Pairing must be started physically from **Settings > Companion connection > Pair phone** on coroNET. Starting the wizard immediately revokes the previous phone relationship, rotates the random 128-bit API token, and opens a two-minute pairing session. The old token cannot be restored by cancelling the wizard.

1. coroNET shows a random six-digit comparison code and advertises over BLE.
2. The app connects, subscribes, and sends `{"cmd":"getPairingChallenge"}`. While a temporary pairing candidate is connected, it repeats this idempotent request at a low rate until a valid challenge arrives or the two-minute attempt ends; this covers a wizard opened just after GATT setup and a notification lost during reconnection.
3. Firmware returns message type `4` with `t=pairing_challenge`, the device identity, session ID, comparison code, and remaining lifetime. It does not include the API token.
4. The user verifies that both displays show the same code and confirms independently on the phone and coroNET.
5. The phone sends `{"cmd":"confirmPairingCode","session":123,"code":456789}`. The device confirmation is a local touchscreen action and cannot be sent remotely.
6. Only after both confirmations, firmware sends `t=pairing_result` containing the session ID, device ID, local IP address when available, and the new API token.
7. The app stores the result atomically in encrypted preferences and sends `{"cmd":"completePairing","session":123}`.
8. Firmware persists `apiPaired=true` and acknowledges `pairing_confirmed`.

The Android client treats scan results as temporary candidates. Tapping a discovered device may establish a BLE connection and wait for a physically opened wizard, but it must not add the device to the saved-device list or call it paired. Persistence happens only after a valid `pairing_result` has been securely stored and acknowledged.

Either side may send or invoke cancellation. Expired, cancelled, mismatched, and incomplete sessions leave the old relationship revoked and never expose the new token. This is an application-level physical confirmation flow; it does not claim to be operating-system Bluetooth Secure Connections numeric comparison.

## BLE Session Authentication

Each new BLE connection starts unauthenticated. A saved app sends the 32-character token before requesting settings or changing controls:

```json
{"cmd":"authenticate","token":"0123456789abcdef0123456789abcdef"}
```

Firmware compares the token without an early-exit timing difference and replies with `authenticated` or `authentication_failed`. Unauthenticated clients may discover the device, request a basic state snapshot, and participate in a physically opened pairing wizard, but they cannot read settings or change device configuration. The BLE connection that completes pairing becomes authenticated immediately.

## Transport Recovery

- `auto`: BLE remains available and the WiFi API starts whenever WiFi is connected.
- `ble`: companion control uses BLE; the web API remains stopped.
- `wifi`: BLE stops after pairing and a successful WiFi connection.
- If `wifi` mode cannot connect for 45 seconds, temporary BLE advertising starts as a recovery channel.
- A fallback BLE connection remains alive until the phone disconnects, even if WiFi recovers during that session.

## Authenticated WiFi API

The API is available in `auto` and `wifi` modes while the device has a WiFi connection:

- `GET /api/state`;
- `GET /api/settings`;
- `POST /api/settings`;
- `POST /api/printer/test`;
- `POST /api/ota/check`;
- `POST /api/ota/install`;
- `POST /api/ota/reinstall`;
- `POST /api/ota/sd`.

OTA commands are deliberately Wi-Fi-only. BLE is not used to stream firmware. The `ota` object returned by `GET /api/state` contains numeric `state`, `progress`, `available`, `version`, and human-readable `status` fields. State values follow the firmware `OtaState` order: idle `0`, checking `1`, available `2`, up to date `3`, preparing `4`, downloading `5`, installing `6`, success `7`, and failed `8`.

Clients should disable repeated update actions while states `1` or `4..6` are active. `install` accepts only a newer published release; `reinstall` permits the same release after explicit user confirmation. When maintenance begins, the web API intentionally stops and the app may temporarily report the device offline while the physical display continues to show installation progress.

Every `/api/*` request requires one of these headers:

```text
Authorization: Bearer <32-character-token>
```

```text
X-coroNET-Token: <32-character-token>
```

Unauthenticated calls return HTTP `401`. Settings JSON bodies are limited to 4096 bytes. The firmware intentionally does not publish a wildcard CORS origin; the Android app uses its native HTTP client. Secrets are never returned by settings endpoints, only `wifiPasswordSet` and `printerApiKeySet` flags.

Settings are applied to active services immediately. NVS persistence is debounced for 1.5 seconds and forced after at most 5 seconds to prevent slider controls from wearing flash.

`GET /api/settings` returns `settingsRevision`, and every BLE settings group includes the same value as `sr`. The revision changes whenever firmware accepts a settings mutation. Clients must ignore older revisions, keep locally pending fields during an optimistic update, and replace them with the device-confirmed value after acknowledgement. Concurrent edits to different fields therefore merge; concurrent edits to the same field settle on the latest value accepted by coroNET.

The Android client keeps the most recent valid snapshot and settings for each device in encrypted preferences. When both transports are unavailable, it presents that data explicitly as cached instead of replacing the interface with zero or placeholder values. WiFi recovery first retries the last successful address and then the stable `coronet-xxxx.local` hostname, while BLE service-discovery, subscription, and write failures enter the bounded reconnect backoff.

## Android Reference Client

The native reference application lives in [`android/`](../android). It queues GATT descriptor and characteristic operations, reconstructs framed notifications, stores per-device API tokens and cached state in encrypted preferences backed by Android Keystore, remembers multiple devices, prefers the authenticated WiFi API after pairing, reconnects automatically, resolves concurrent settings through firmware revisions, and posts local notifications for printer Error and Finish transitions.
