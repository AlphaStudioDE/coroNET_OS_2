#pragma once

#include <stdint.h>

namespace coronet {

enum class WifiScanStatus : uint8_t {
    Idle = 0,
    Scanning,
    Complete,
    Failed,
};

struct WifiNetworkInfo {
    char ssid[33] = "";
    int32_t rssi = -127;
    bool secured = true;
};

class WifiService {
public:
    void begin();
    void loop();
    void requestScan();
    WifiScanStatus scanStatus() const { return scanStatus_; }
    uint8_t scanCount() const { return scanCount_; }
    uint32_t scanRevision() const { return scanRevision_; }
    const WifiNetworkInfo* network(uint8_t index) const;

private:
    static constexpr uint8_t MaxScanResults = 12;

    char activeSsid_[33] = "";
    char activePassword_[65] = "";
    WifiNetworkInfo* scanResults_ = nullptr;
    bool started_ = false;
    WifiScanStatus scanStatus_ = WifiScanStatus::Idle;
    uint8_t scanCount_ = 0;
    uint32_t scanRevision_ = 0;

    void applySettings();
    void pollScan();
    void collectScanResults(int16_t count);
};

WifiService& wifiService();

}
