#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace coronet {

class WebControlService {
public:
    void begin();
    void loop();

private:
    WebServer server_{80};
    bool routesReady_ = false;
    bool serverRunning_ = false;
    bool mdnsRunning_ = false;
    uint32_t lastStateMs_ = 0;

    void registerRoutes();
    void updateRuntimeState();
    void start();
    void stop();
    bool shouldRun() const;

    void sendCors();
    void sendJson(int code, const String& payload);
    void sendNoContent();
    void handleRoot();
    void handleState();
    void handleSettings();
    void handleUpdateSettings();
    void handlePrinterTest();
    void handleNotFound();
};

}
