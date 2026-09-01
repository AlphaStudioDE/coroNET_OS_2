#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../core/ProductTypes.h"

namespace coronet {

class OtaService {
public:
    void begin();
    void loop();
    bool requestCheck();
    bool requestInstall(bool allowSameVersion = false);
    bool requestSdRecovery();
    void factoryReset();
    void logStatus() const;

private:
    enum class Request : uint8_t { None, Check, Install, Reinstall, SdRecovery };
    static void taskEntry(void* context);
    void taskLoop(Request request);
    bool startRequest(Request request);
    bool checkLatestRelease();
    bool installFromUrl(const char* url);
    bool installFromSd();
    bool validateImageHeader(Stream& stream, size_t expectedSize);
    void setState(OtaState stateValue, const char* message, uint8_t progress = 0);

    TaskHandle_t task_ = nullptr;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    Request pendingRequest_ = Request::None;
    char downloadUrl_[320] = "";
    uint32_t stableSinceMs_ = 0;
    bool appMarkedValid_ = false;
};

OtaService& otaService();

}
