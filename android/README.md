# coroNET Android Companion

This is the native Android reference client for coroNET OS 2. It mirrors structured device state rather than streaming the ESP32 display.

## Current capabilities

- BLE discovery and framed protocol V2 reassembly;
- automatic first-pairing token transfer and encrypted per-device storage;
- multiple saved coroNET devices with fast selection;
- preferred authenticated local WiFi polling and settings control;
- automatic BLE reconnect and WiFi recovery through the stable mDNS hostname;
- encrypted per-device offline state and settings cache;
- firmware-revision conflict handling for simultaneous touchscreen and phone edits;
- Home, LED, Vent, Sound, and Settings screens;
- adaptive landscape console UI matching the touchscreen's information hierarchy while using the phone's wider canvas;
- real firmware-driven LED animation previews and physical color calibration over BLE or WiFi;
- browsable microSD sound folders, per-status sound assignment, playback, stop, and library rescan over BLE or WiFi;
- synchronized device naming and companion transport selection;
- revisioned printer Error and Finish notifications without reconnect duplicates;
- Android 8.0 (API 26) and newer.

## Build

Open this directory in Android Studio, or run from a terminal with JDK 17 and the Android SDK configured:

```powershell
.\gradlew.bat assembleDebug
```

The debug APK is generated at `app/build/outputs/apk/debug/app-debug.apk`.

Physical-phone BLE testing remains part of the release validation checklist. The UI and application lifecycle are also exercised on the included minimum-SDK emulator during development.
