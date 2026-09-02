# coroNET Android Companion

This is the native Android reference client for coroNET OS 2. It mirrors structured device state rather than streaming the ESP32 display.

## Current capabilities

- BLE discovery and framed protocol V2 reassembly;
- automatic first-pairing token transfer and encrypted per-device storage;
- multiple saved coroNET devices with fast selection;
- preferred authenticated local WiFi polling and settings control;
- Home, LED, Vent, Sound, and Settings screens;
- revisioned printer Error and Finish notifications without reconnect duplicates;
- Android 8.0 (API 26) and newer.

## Build

Open this directory in Android Studio, or run from a terminal with JDK 17 and the Android SDK configured:

```powershell
.\gradlew.bat assembleDebug
```

The debug APK is generated at `app/build/outputs/apk/debug/app-debug.apk`.

Physical-phone BLE testing remains part of the release validation checklist. The UI and application lifecycle are also exercised on the included minimum-SDK emulator during development.
