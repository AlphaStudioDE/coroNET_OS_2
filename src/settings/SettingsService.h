#pragma once

#include <Arduino.h>

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
    uint16_t schemaVersion = 4;
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
