#pragma once

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace coronet {

enum class WifiScanStatus : uint8_t {
    Idle = 0,
    Scanning,
    Complete,
    Failed,
};

enum class WifiConnectStatus : uint8_t {
    Idle = 0,
    Connecting,
    Connected,
    NoNetwork,
    AuthenticationFailed,
    Timeout,
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
    void requestConnectionTest(const char* ssid, const char* password);
    void acceptConnectionTest();
    void cancelConnectionTest();
    WifiScanStatus scanStatus() const { return scanStatus_; }
    uint8_t scanCount() const { return scanCount_; }
    uint32_t scanRevision() const { return scanRevision_; }
    const WifiNetworkInfo* network(uint8_t index) const;
    WifiConnectStatus connectionStatus() const { return connectionStatus_; }
    uint32_t connectionRevision() const { return connectionRevision_; }
    const char* connectionSsid() const { return testSsid_; }
    void connectionIp(char* output, size_t outputSize) const;
    bool acquireMdns(uint32_t readyTimeoutMs, TickType_t lockTimeoutTicks);
    void releaseMdns();
    bool publishMdnsService(const char* service, const char* protocol, uint16_t port);
    uint32_t mdnsGeneration() const { return mdnsGeneration_; }

private:
    static constexpr uint8_t MaxScanResults = 12;

    char activeSsid_[33] = "";
    char activePassword_[65] = "";
    char testSsid_[33] = "";
    char testPassword_[65] = "";
    WifiNetworkInfo* scanResults_ = nullptr;
    bool started_ = false;
    WifiScanStatus scanStatus_ = WifiScanStatus::Idle;
    uint8_t scanCount_ = 0;
    uint32_t scanRevision_ = 0;
    WifiConnectStatus connectionStatus_ = WifiConnectStatus::Idle;
    uint32_t connectionRevision_ = 0;
    uint32_t connectionStartedMs_ = 0;
    bool connectionTestActive_ = false;
    bool connectionStartPending_ = false;
    bool scanStartPending_ = false;
    bool restoreConnectionAfterScan_ = false;
    uint32_t scanPrepareStartedMs_ = 0;
    uint32_t connectionPrepareStartedMs_ = 0;
    SemaphoreHandle_t mdnsMutex_ = nullptr;
    volatile bool mdnsRunning_ = false;
    bool wifiWasConnected_ = false;
    uint32_t wifiConnectedSinceMs_ = 0;
    volatile uint32_t mdnsStartedMs_ = 0;
    uint32_t mdnsGeneration_ = 0;
    uint32_t mdnsIp_ = 0;

    void applySettings();
    void startPreparedScan();
    void pollScan();
    void pollConnectionTest();
    void collectScanResults(int16_t count);
    void maintainMdns();
};

WifiService& wifiService();

}
