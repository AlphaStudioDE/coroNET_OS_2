#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

#include "../core/ProductTypes.h"
#include "../core/SystemState.h"

namespace coronet {

class PandaBreathService {
public:
    void begin();
    void loop();
    void applyNow();
    void disconnect();
    void logStatus() const;

private:
    struct DryProfile {
        uint8_t temperatureC;
        uint8_t hours;
    };

    void configureFromSettings();
    void connectIfNeeded();
    void handleEvent(WStype_t type, uint8_t* payload, size_t length);
    void handleMessage(const uint8_t* payload, size_t length);
    void updateWorkflow(uint32_t now);
    void setPhase(PandaWorkflowPhase phase, const char* text);
    void requestOff();
    void requestHeat(uint8_t targetC, PandaWorkflowPhase phase, const char* text);
    void requestDry(uint8_t targetC, uint8_t hours);
    void sendDesired(bool force = false);
    void sendOff();
    void sendHeat(uint8_t targetC);
    void sendDry(uint8_t targetC, uint8_t hours);
    DryProfile dryProfile() const;
    uint8_t temperingTarget(uint32_t now) const;
    static void normalizeHost(const char* input, char output[65]);

    WebSocketsClient socket_;
    bool initialized_ = false;
    bool connected_ = false;
    bool socketConfigured_ = false;
    bool desiredOn_ = false;
    bool desiredDrying_ = false;
    uint8_t desiredTargetC_ = 0;
    uint8_t desiredHours_ = 0;
    bool commandDirty_ = false;
    char configuredHost_[65] = "";
    uint32_t observedSettingsRevision_ = 0;
    uint32_t lastConnectAttemptMs_ = 0;
    uint32_t lastWorkflowMs_ = 0;
    uint32_t lastCommandMs_ = 0;
    uint32_t phaseStartedMs_ = 0;
    uint32_t holdStartedMs_ = 0;
    uint32_t observedPrinterEventSequence_ = 0;
};

PandaBreathService& pandaBreathService();

}
