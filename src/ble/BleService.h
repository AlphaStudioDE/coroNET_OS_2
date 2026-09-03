#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class NimBLECharacteristic;

namespace coronet {

class BleService {
public:
    void begin();
    void loop();
    void publishEvent(const char* type, const char* message);
    void queueCommand(const char* command, size_t length);
    void onConnected(uint16_t mtu);
    void onDisconnected();
    void onMtuChanged(uint16_t mtu);
    bool active();

private:
    static constexpr size_t CommandMaxLength = 384;
    static constexpr UBaseType_t CommandQueueDepth = 4;

    struct QueuedCommand {
        uint16_t length;
        char data[CommandMaxLength];
    };

    bool started_ = false;
    bool connected_ = false;
    bool stateDirty_ = true;
    bool connectionEventPending_ = false;
    bool disconnectEventPending_ = false;
    bool commandOverflowPending_ = false;
    bool fallbackActive_ = false;
    bool sessionAuthenticated_ = false;
    uint16_t connectionMtu_ = 23;
    uint16_t messageId_ = 0;
    uint32_t lastNotifyMs_ = 0;
    uint32_t revision_ = 0;
    uint32_t observedPrinterTelemetryRevision_ = 0;
    uint32_t observedPrinterConnectionRevision_ = 0;
    uint32_t observedPrinterEventSequence_ = 0;
    uint32_t observedPairingRevision_ = 0;
    uint32_t observedPairingSessionId_ = 0;
    uint32_t appliedSettingsRevision_ = 0;
    uint32_t wifiOfflineSinceMs_ = 0;
    uint32_t lastPairingPublishMs_ = 0;
    QueueHandle_t commandQueue_ = nullptr;
    portMUX_TYPE connectionMux_ = portMUX_INITIALIZER_UNLOCKED;
    char deviceId_[13] = "";
    char advertisedName_[25] = "";

    void startStack();
    void stopStack();
    void applySettings();
    void updateAdvertisingName();
    void handleCommand(const char* command, size_t length);
    void publishState(bool force);
    void publishSettings();
    void publishSoundLibrary(uint8_t folder, uint8_t page);
    void publishPairingChallenge();
    void publishPairingResult();
    void refreshAdvertisedName();
    bool isConnected();
    bool sendFramed(NimBLECharacteristic* characteristic,
                    uint8_t messageType,
                    const uint8_t* payload,
                    size_t length);
};

BleService& bleService();

}
