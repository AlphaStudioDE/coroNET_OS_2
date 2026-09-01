#include <Arduino.h>

#include "audio/AudioService.h"
#include "ble/BleService.h"
#include "config/AppConfig.h"
#include "core/MemoryService.h"
#include "core/SystemState.h"
#include "core/SystemHealth.h"
#include "display/DisplayService.h"
#include "printer/PrinterService.h"
#include "settings/SettingsService.h"
#include "web/WebControlService.h"
#include "wifi/WifiService.h"

namespace {

coronet::DisplayService displayService;
coronet::AudioService audioService;
coronet::BleService bleService;
coronet::SystemHealth systemHealth;
coronet::WebControlService webControlService;
char serialCommand[48] = "";
size_t serialCommandLength = 0;

void executeSerialCommand() {
    serialCommand[serialCommandLength] = '\0';
    if (strcmp(serialCommand, "wizard reset") == 0) {
        coronet::AppSettings& settings = coronet::settingsService().mutableSettings();
        settings.setupDone = false;
        coronet::state().setupDone = false;
        coronet::settingsService().save();
        coronet::settingsService().flush();
        Serial.println("[console] setup wizard reopened; restarting");
        Serial.flush();
        delay(50);
        ESP.restart();
    } else if (strcmp(serialCommand, "printer scan") == 0) {
        Serial.printf("[console] printer discovery %s\n",
                      coronet::printerService().requestDiscovery() ? "started" : "not started");
    } else if (strcmp(serialCommand, "wifi test") == 0) {
        const coronet::AppSettings& settings = coronet::settingsService().settings();
        coronet::wifiService().requestConnectionTest(settings.wifiSsid, settings.wifiPassword);
        Serial.println("[console] saved Wi-Fi connection test started");
    } else if (strcmp(serialCommand, "wifi accept") == 0) {
        coronet::wifiService().acceptConnectionTest();
        Serial.println("[console] Wi-Fi connection test accepted");
    } else if (serialCommandLength > 0) {
        Serial.printf("[console] unknown command: %s\n", serialCommand);
    }
    serialCommandLength = 0;
}

void processSerialConsole() {
    while (Serial.available() > 0) {
        const char input = static_cast<char>(Serial.read());
        if (input == '\r' || input == '\n') {
            if (serialCommandLength > 0) executeSerialCommand();
            continue;
        }
        if (input >= 32 && input <= 126 && serialCommandLength < sizeof(serialCommand) - 1) {
            serialCommand[serialCommandLength++] = input;
        }
    }
}

}

void setup() {
    Serial.begin(coronet::config::SerialBaud);
    delay(150);

    coronet::state().bootMs = millis();

    Serial.println();
    Serial.println(coronet::config::FirmwareName);
    Serial.print("Version: ");
    Serial.println(coronet::config::FirmwareVersion);

    coronet::memoryService().begin();
    systemHealth.checkpoint("memory");
    coronet::settingsService().begin();
    systemHealth.checkpoint("settings");
    systemHealth.begin();
    displayService.begin();
    systemHealth.checkpoint("display-touch");
    audioService.begin();
    systemHealth.checkpoint("audio");
    coronet::wifiService().begin();
    systemHealth.checkpoint("wifi");
    coronet::printerService().begin();
    systemHealth.checkpoint("printer");
    webControlService.begin();
    systemHealth.checkpoint("web");
    bleService.begin();
    systemHealth.checkpoint("ble");
}

void loop() {
    processSerialConsole();
    systemHealth.loop();
    coronet::settingsService().loop();
    displayService.loop();
    audioService.loop();
    coronet::wifiService().loop();
    coronet::printerService().loop();
    webControlService.loop();
    bleService.loop();
    delay(10);
}
