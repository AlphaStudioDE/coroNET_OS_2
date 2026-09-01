#include "WifiService.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {
WifiService gWifiService;
}

WifiService& wifiService() {
    return gWifiService;
}

void WifiService::begin() {
    WiFi.mode(WIFI_STA);
    applySettings();
    started_ = true;
    state().wifiConnected = WiFi.status() == WL_CONNECTED;
}

void WifiService::loop() {
    pollScan();
    if (started_) {
        const AppSettings& cfg = settingsService().settings();
        if (strncmp(activeSsid_, cfg.wifiSsid, sizeof(activeSsid_)) != 0 ||
            strncmp(activePassword_, cfg.wifiPassword, sizeof(activePassword_)) != 0) {
            applySettings();
        }
    }
    state().wifiConnected = WiFi.status() == WL_CONNECTED;
}

void WifiService::requestScan() {
    if (scanStatus_ == WifiScanStatus::Scanning) return;
    if (!scanResults_) {
        scanResults_ = static_cast<WifiNetworkInfo*>(heap_caps_calloc(
            MaxScanResults, sizeof(WifiNetworkInfo), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!scanResults_) {
            scanStatus_ = WifiScanStatus::Failed;
            scanRevision_++;
            Serial.println("[wifi] scan result allocation failed");
            return;
        }
    }

    scanCount_ = 0;
    WiFi.scanDelete();
    const int16_t result = WiFi.scanNetworks(true, true);
    if (result == WIFI_SCAN_FAILED) {
        scanStatus_ = WifiScanStatus::Failed;
        scanRevision_++;
        Serial.println("[wifi] scan start failed");
        return;
    }
    scanStatus_ = WifiScanStatus::Scanning;
    scanRevision_++;
    Serial.println("[wifi] scan started");
    if (result >= 0) collectScanResults(result);
}

const WifiNetworkInfo* WifiService::network(uint8_t index) const {
    if (!scanResults_ || index >= scanCount_) return nullptr;
    return &scanResults_[index];
}

void WifiService::pollScan() {
    if (scanStatus_ != WifiScanStatus::Scanning) return;
    const int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;
    if (result == WIFI_SCAN_FAILED) {
        scanStatus_ = WifiScanStatus::Failed;
        scanRevision_++;
        WiFi.scanDelete();
        Serial.println("[wifi] scan failed");
        return;
    }
    collectScanResults(result);
}

void WifiService::collectScanResults(int16_t count) {
    scanCount_ = 0;
    if (!scanResults_) {
        scanStatus_ = WifiScanStatus::Failed;
        scanRevision_++;
        WiFi.scanDelete();
        return;
    }

    for (int16_t index = 0; index < count && scanCount_ < MaxScanResults; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) continue;

        bool duplicate = false;
        for (uint8_t existing = 0; existing < scanCount_; ++existing) {
            if (strncmp(scanResults_[existing].ssid, ssid.c_str(),
                        sizeof(scanResults_[existing].ssid)) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        WifiNetworkInfo& network = scanResults_[scanCount_++];
        ssid.toCharArray(network.ssid, sizeof(network.ssid));
        network.rssi = WiFi.RSSI(index);
        network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }

    scanStatus_ = WifiScanStatus::Complete;
    scanRevision_++;
    WiFi.scanDelete();
    Serial.printf("[wifi] scan complete networks=%u\n", static_cast<unsigned>(scanCount_));
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
