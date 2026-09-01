# coroNET OS 2 Companion Protocol

The Android companion app controls coroNET through structured data, not screen streaming. BLE is the first-setup and fallback channel. WiFi is the preferred local control channel after network setup because it is faster, roomier, and better suited for richer app screens.

## Device Discovery

- Scan for devices advertising service UUID `7b7e0001-9f2a-4f3c-8d2a-c0a0e7c0ffee`.
- Use a BLE scanner or the coroNET Android companion app for testing. The normal Android Bluetooth settings screen is not a reliable GATT client and may not show or connect to this device like it would with classic Bluetooth accessories.
- Do not rely on the Bluetooth display name alone. The name is user-visible and can change.
- The default advertised name is `coroNET_XXXX`, where `XXXX` is derived from the ESP32 hardware ID.
- The state snapshot contains a stable `id` field with the full ESP32-derived device identifier.
- The app should store paired/known devices by `id`, with the latest user-visible `name` as metadata.
- After WiFi is configured, the firmware exposes a local HTTP API at `http://coronet-xxxx.local/`, where `xxxx` matches the generated device suffix.

## Multiple Devices

The app must support more than one coroNET device:

- keep a saved device list keyed by `id`;
- show the current advertised/user name for each device;
- allow switching the active device from the app UI;
- keep per-device cached state and settings;
- route notifications, such as printer error or finish, to the matching saved device.
- prefer WiFi control when the device is reachable on the local network, and fall back to BLE when WiFi is unavailable.

## Device Name

The device name can be changed through settings. Firmware accepts:

```json
{"cmd":"setDeviceName","name":"Workshop coroNET"}
```

To return to the generated default name:

```json
{"cmd":"resetDeviceName"}
```

Names are intentionally limited to short ASCII letters, numbers, spaces, underscores, and hyphens so they remain safe inside BLE advertising packets and JSON snapshots.

## Transport Mode

coroNET can expose companion control through BLE, WiFi, or both:

- `auto`: BLE remains available for setup/fallback and WiFi starts when connected.
- `ble`: only BLE companion control is used.
- `wifi`: WiFi companion control is used; BLE advertising is skipped on the next boot.

Set the transport through BLE:

```json
{"cmd":"setCompanionTransport","mode":"auto"}
```

The same setting is available through the WiFi API as `transport`.

## Bring-Up Commands

The first firmware protocol is intentionally small and text-based so it can be tested from nRF Connect before the Android app UI is complete.

Read the current device state:

```json
snapshot
```

Read stored setup settings:

```json
{"cmd":"getSettings"}
```

Save WiFi credentials:

```json
{"cmd":"setWifi","ssid":"Workshop WiFi","password":"secret"}
```

Save Moonraker printer connection:

```json
{"cmd":"setPrinter","host":"192.168.1.50","port":7125}
```

Test the configured printer:

```json
{"cmd":"testPrinterConnection"}
```

Mark setup as complete:

```json
{"cmd":"setSetupDone","done":true}
```

## WiFi HTTP API

Once the device is connected to WiFi and transport mode is `auto` or `wifi`, the app can use:

- `GET /api/state` for live system and printer state.
- `GET /api/settings` for stored setup settings. Secrets are not returned; only `wifiPasswordSet` and `printerApiKeySet` flags are exposed.
- `POST /api/settings` to update setup values.
- `POST /api/printer/test` to test the configured Moonraker connection.

Example settings update:

```json
{
  "deviceName": "Workshop coroNET",
  "transport": "wifi",
  "wifiSsid": "Workshop WiFi",
  "wifiPassword": "secret",
  "printerHost": "192.168.1.50",
  "printerPort": 7125,
  "printerApiKey": ""
}
```

The API includes permissive CORS headers to keep Android WebView, browser-based diagnostics, and local development simple.

## Live State

The state characteristic is kept compact so notifications fit comfortably over BLE. Temperatures are sent as tenths of a degree Celsius. For example, `2453` means `245.3 C`; `-32768` means unavailable.
