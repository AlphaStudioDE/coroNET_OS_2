#include "WifiService.h"

#include <WiFi.h>

#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

void WifiService::begin() {
    WiFi.mode(WIFI_STA);
    applySettings();
    started_ = true;
    state().wifiConnected = WiFi.status() == WL_CONNECTED;
}

void WifiService::loop() {
    if (started_) {
        const AppSettings& cfg = settingsService().settings();
        if (strncmp(activeSsid_, cfg.wifiSsid, sizeof(activeSsid_)) != 0 ||
            strncmp(activePassword_, cfg.wifiPassword, sizeof(activePassword_)) != 0) {
            applySettings();
        }
    }
    state().wifiConnected = WiFi.status() == WL_CONNECTED;
}

void WifiService::applySettings() {
    const AppSettings& cfg = settingsService().settings();
    strlcpy(activeSsid_, cfg.wifiSsid, sizeof(activeSsid_));
    strlcpy(activePassword_, cfg.wifiPassword, sizeof(activePassword_));

    if (!activeSsid_[0]) {
        WiFi.disconnect(false, false);
        state().wifiConnected = false;
        return;
    }

    Serial.printf("[wifi] connecting ssid=%s\n", activeSsid_);
    WiFi.disconnect(false, false);
    WiFi.begin(activeSsid_, activePassword_);
}

}
