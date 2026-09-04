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
    char webSessionToken_[33] = "";
    uint32_t mdnsGenerationPublished_ = 0;
    uint32_t lastStateMs_ = 0;

    void registerRoutes();
    void updateRuntimeState();
    void publishMdnsIfNeeded();
    void start();
    void stop();
    bool shouldRun() const;

    bool authorizeRequest();
    bool authorizeWebOrigin();
    void rotateWebSession();
    void sendCommonHeaders();
    void sendJson(int code, const String& payload);
    void sendNoContent();
    void handleRoot();
    void handleApiDescription();
    void handleWebSession();
    void handleState();
    void handleSettings();
    void handleUpdateSettings();
    void handleLedCatalog();
    void handleLedFrame();
    void handleLedPreview();
    void handleLedCalibration();
    void handleTimeZones();
    void handleAudioLibrary();
    void handleAudioPlay();
    void handleAudioStop();
    void handleAudioRescan();
    void handlePrinterTest();
    void handlePandaDiscover();
    void handleOtaCheck();
    void handleOtaInstall(bool reinstall);
    void handleOtaSdRecovery();
    void handleNotFound();
};

}
