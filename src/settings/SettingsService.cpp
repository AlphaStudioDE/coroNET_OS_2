#include "SettingsService.h"

#include <Preferences.h>

namespace coronet {

namespace {
constexpr const char* Namespace = "coronet2";
constexpr uint16_t CurrentSchema = 3;

uint16_t sanePort(uint16_t port) {
    return port == 0 ? 7125 : port;
}
}

static SettingsService gSettingsService;

SettingsService& settingsService() {
    return gSettingsService;
}

void SettingsService::begin() {
    load();
}

void SettingsService::loop() {
}

void SettingsService::load() {
    Preferences prefs;
    if (!prefs.begin(Namespace, true)) {
        resetToDefaults();
        return;
    }

    settings_.schemaVersion = prefs.getUShort("schema", CurrentSchema);
    settings_.setupDone = prefs.getBool("setupDone", false);
    settings_.bleEnabled = prefs.getBool("ble", true);
    settings_.displayBrightness = prefs.getUChar("brightness", 80);
    settings_.uiSkin = static_cast<UiSkin>(prefs.getUChar("uiSkin", static_cast<uint8_t>(UiSkin::Coronet)));
    settings_.uiColorMode = static_cast<UiColorMode>(prefs.getUChar("uiColor", static_cast<uint8_t>(UiColorMode::Dark)));
    settings_.companionTransport = static_cast<CompanionTransport>(prefs.getUChar("transport", static_cast<uint8_t>(CompanionTransport::Auto)));
    String deviceName = prefs.getString("deviceName", "");
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("wifiPass", "");
    String printerHost = prefs.getString("printerHost", "");
    settings_.printerPort = prefs.getUShort("printerPort", 7125);
    String printerApiKey = prefs.getString("printerKey", "");
    prefs.end();

    const bool needsMigrationSave = settings_.schemaVersion < CurrentSchema;
    if (settings_.schemaVersion > CurrentSchema) {
        resetToDefaults();
        save();
        return;
    }

    deviceName.toCharArray(settings_.deviceName, sizeof(settings_.deviceName));
    ssid.toCharArray(settings_.wifiSsid, sizeof(settings_.wifiSsid));
    pass.toCharArray(settings_.wifiPassword, sizeof(settings_.wifiPassword));
    printerHost.toCharArray(settings_.printerHost, sizeof(settings_.printerHost));
    printerApiKey.toCharArray(settings_.printerApiKey, sizeof(settings_.printerApiKey));
    if (settings_.displayBrightness > 100) settings_.displayBrightness = 80;
    settings_.printerPort = sanePort(settings_.printerPort);
    if (static_cast<uint8_t>(settings_.uiSkin) > static_cast<uint8_t>(UiSkin::Minimal)) {
        settings_.uiSkin = UiSkin::Coronet;
    }
    if (static_cast<uint8_t>(settings_.uiColorMode) > static_cast<uint8_t>(UiColorMode::Auto)) {
        settings_.uiColorMode = UiColorMode::Dark;
    }
    if (static_cast<uint8_t>(settings_.companionTransport) > static_cast<uint8_t>(CompanionTransport::Wifi)) {
        settings_.companionTransport = CompanionTransport::Auto;
    }
    if (needsMigrationSave) {
        settings_.schemaVersion = CurrentSchema;
        save();
    }
    loaded_ = true;
}

void SettingsService::save() {
    Preferences prefs;
    if (!prefs.begin(Namespace, false)) return;
    prefs.putUShort("schema", CurrentSchema);
    prefs.putBool("setupDone", settings_.setupDone);
    prefs.putBool("ble", settings_.bleEnabled);
    prefs.putUChar("brightness", settings_.displayBrightness);
    prefs.putUChar("uiSkin", static_cast<uint8_t>(settings_.uiSkin));
    prefs.putUChar("uiColor", static_cast<uint8_t>(settings_.uiColorMode));
    prefs.putUChar("transport", static_cast<uint8_t>(settings_.companionTransport));
    prefs.putString("deviceName", settings_.deviceName);
    prefs.putString("ssid", settings_.wifiSsid);
    prefs.putString("wifiPass", settings_.wifiPassword);
    prefs.putString("printerHost", settings_.printerHost);
    prefs.putUShort("printerPort", sanePort(settings_.printerPort));
    prefs.putString("printerKey", settings_.printerApiKey);
    prefs.end();
}

void SettingsService::resetToDefaults() {
    settings_ = AppSettings{};
    settings_.schemaVersion = CurrentSchema;
    loaded_ = true;
}

}
