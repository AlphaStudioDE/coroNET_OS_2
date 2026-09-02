#pragma once

#include <Arduino.h>

#include "../core/ProductTypes.h"

namespace coronet {

enum class UiSkin : uint8_t {
    Coronet = 0,
    Graphite = 1,
    Aurora = 2,
    Minimal = 3,
};

enum class UiColorMode : uint8_t {
    Dark = 0,
    Light = 1,
    Auto = 2,
};

enum class CompanionTransport : uint8_t {
    Auto = 0,
    Ble = 1,
    Wifi = 2,
};

struct AppSettings {
    uint16_t schemaVersion = 5;
    bool setupDone = false;
    bool bleEnabled = true;
    uint8_t displayBrightness = 80;
    UiSkin uiSkin = UiSkin::Coronet;
    UiColorMode uiColorMode = UiColorMode::Dark;
    CompanionTransport companionTransport = CompanionTransport::Auto;
    char deviceName[25] = "";
    char wifiSsid[33] = "";
    char wifiPassword[65] = "";
    char printerHost[65] = "";
    uint16_t printerPort = 7125;
    char printerApiKey[97] = "";
    char apiToken[33] = "";
    bool apiPaired = false;

    bool ledEnabled = true;
    bool ledOtherMode = false;
    uint8_t ledBrightness[enumCount(LedSection{})] = {70, 70, 70, 70};
    bool ledDimmEnabled[enumCount(LedSection{})] = {false, false, false, false};
    uint8_t ledDimmPercent[enumCount(LedSection{})] = {20, 20, 20, 20};
    InsideColorStyle insideColorStyle = InsideColorStyle::White;
    bool mirrorLedLayout = false;
    uint8_t ledAnimation[enumCount(LedCategory{})] = {0, 0, 0, 0, 0, 0};
    int16_t ledColorRemixDegrees[enumCount(LedCategory{})] = {0, 0, 0, 0, 0, 0};

    uint8_t soundVolume[enumCount(SoundScenario{})] = {75, 75, 85, 70, 60};
    bool soundRepeat[enumCount(SoundScenario{})] = {false, false, true, false, false};
    char soundPath[enumCount(SoundScenario{})][65] = {};

    VentMode ventMode = VentMode::Automatic;
    uint8_t ventTargetTempC = 40;
    uint8_t manualFanPercent = 0;
    uint8_t manualFlapPercent = 0;
    uint8_t fanMinPercent = 30;
    uint8_t fanMaxPercent = 100;
    uint8_t failsafeFanPercent = 100;
    uint8_t failsafeFlapPercent = 100;
    uint16_t servoClosedUs = 1000;
    uint16_t servoOpenUs = 2000;
    bool servoReverse = false;

    char pandaHost[65] = "";
    bool pandaEnabled = false;
    PandaBreathMode pandaMode = PandaBreathMode::Off;
    uint8_t pandaTargetTempC = 40;
    uint8_t pandaPrintTargetTempC = 40;
    PandaDryPreset pandaDryPreset = PandaDryPreset::Pla;
    uint8_t pandaDryHours = 12;
    uint8_t pandaPreheatHoldMinutes = 15;
    uint8_t pandaTemperingDurationMinutes = 30;
    uint8_t pandaTemperingEndTempC = 0;
    bool pandaTemperingAfterPrint = false;

    uint16_t accentHueDegrees = 190;
    ScreenSaverMode screenSaverMode = ScreenSaverMode::Clock;
    uint8_t screenSaverDelayMinutes = 5;
    uint8_t clockBrightness = 35;
    ClockStyle clockStyle = ClockStyle::Digital;
    bool clock24Hour = true;
    char timeZone[41] = "CET-1CEST,M3.5.0,M10.5.0/3";
    QuietTarget quietTarget = QuietTarget::Off;
    uint16_t quietDurationMinutes = 60;
    bool quietErrorsBypass = true;
};

class SettingsService {
public:
    void begin();
    void loop();
    const AppSettings& settings() const { return settings_; }
    AppSettings& mutableSettings() { return settings_; }
    uint32_t revision() const { return revision_; }
    void save();
    void flush();
    void resetToDefaults();
    void resetApiPairing();

private:
    void load();
    void saveNow();
    void ensureApiToken();
    AppSettings settings_;
    bool loaded_ = false;
    bool savePending_ = false;
    uint32_t revision_ = 1;
    uint32_t dirtySinceMs_ = 0;
    uint32_t lastChangeMs_ = 0;
};

SettingsService& settingsService();

}
