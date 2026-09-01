# coroNET OS 2 Companion Protocol

The Android companion app controls coroNET through structured state and commands, not screen streaming. BLE is the setup and recovery channel. Authenticated local WiFi is the preferred channel for normal use because it is faster and better suited to complete application screens.

The current protocol version is `1`. Multi-byte binary fields use little-endian byte order.

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

## Binary State Snapshot V1

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

The V1 snapshot is 175 bytes. Temperature value `-32768` means unavailable.

Flag bits are: setup done `0`, WiFi connected `1`, web API ready `2`, BLE connected `3`, display ready `4`, touch ready `5`, printer configured `6`, printer connected `7`, audio ready `8`, and temporary BLE fallback active `9`.

Printer state values follow firmware enum order: unknown `0`, idle `1`, printing `2`, paused `3`, error `4`, and complete `5`.

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

The same field names are accepted by `POST /api/settings`. BLE accepts appearance, screen saver, quiet mode, LED brightness/policy, sound volume, local vent calibration, and Panda mode controls. Larger configuration changes should be split into small patches so each command remains below the 384-byte command limit.

## Initial Pairing

Each device generates a random 128-bit local API token and stores it in NVS. Complete first pairing as follows:

1. Connect over BLE and subscribe to the state and event characteristics.
2. Send `{"cmd":"getPairingToken"}`.
3. Reassemble message type `4` and store its `id` and `token` atomically in the phone's secure storage.
4. Send `{"cmd":"confirmPairing"}` only after storage succeeds.
5. Wait for the `pairing_confirmed` acknowledgement.

BLE remains available until pairing is confirmed, even when WiFi transport was selected. After confirmation, the token is no longer returned. A future settings screen will provide a physical action for revoking the token and opening a new pairing window.

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

Every `/api/*` request requires one of these headers:

```text
Authorization: Bearer <32-character-token>
```

```text
X-coroNET-Token: <32-character-token>
```

Unauthenticated calls return HTTP `401`. Settings JSON bodies are limited to 4096 bytes. The firmware intentionally does not publish a wildcard CORS origin; the Android app uses its native HTTP client. Secrets are never returned by settings endpoints, only `wifiPasswordSet` and `printerApiKeySet` flags.

Settings are applied to active services immediately. NVS persistence is debounced for 1.5 seconds and forced after at most 5 seconds to prevent slider controls from wearing flash.

## Android Reference Client

The native reference application lives in [`android/`](../android). It queues GATT descriptor and characteristic operations, reconstructs framed notifications, stores per-device API tokens in encrypted preferences backed by Android Keystore, remembers multiple devices, prefers the authenticated WiFi API after pairing, and posts local notifications for printer Error and Finish transitions.
