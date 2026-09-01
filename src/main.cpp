#include <Arduino.h>

#include "audio/AudioService.h"
#include "ble/BleService.h"
#include "config/AppConfig.h"
#include "core/MemoryService.h"
#include "core/SystemState.h"
#include "core/SystemHealth.h"
#include "display/DisplayService.h"
#include "led/LedService.h"
#include "printer/PrinterService.h"
#include "settings/SettingsService.h"
#include "web/WebControlService.h"
#include "wifi/WifiService.h"
#include "vent/VentService.h"

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
    } else if (strcmp(serialCommand, "audio test") == 0) {
        audioService.playTestTone();
    } else if (strcmp(serialCommand, "audio stop") == 0) {
        audioService.stop();
    } else if (strcmp(serialCommand, "audio status") == 0) {
        audioService.logStatus();
    } else if (strcmp(serialCommand, "audio release") == 0) {
        audioService.release();
    } else if (strcmp(serialCommand, "audio profile balanced") == 0) {
        audioService.useDmaProfile(coronet::AudioDmaProfile::Balanced);
    } else if (strcmp(serialCommand, "audio profile coronet1") == 0) {
        audioService.useDmaProfile(coronet::AudioDmaProfile::Coronet1);
    } else if (strcmp(serialCommand, "audio rate 22050") == 0) {
        audioService.setSampleRate(22050);
    } else if (strcmp(serialCommand, "audio rate 44100") == 0) {
        audioService.setSampleRate(44100);
    } else if (strcmp(serialCommand, "audio rate 48000") == 0) {
        audioService.setSampleRate(48000);
    } else if (strcmp(serialCommand, "ui home") == 0) {
        displayService.requestPage(coronet::ui::Page::Home);
        Serial.println("[console] Home screen requested");
    } else if (strcmp(serialCommand, "ui settings") == 0) {
        displayService.requestPage(coronet::ui::Page::Settings);
        Serial.println("[console] Settings screen requested");
    } else if (strcmp(serialCommand, "led status") == 0) {
        coronet::ledService().logStatus();
    } else if (strcmp(serialCommand, "led preview stop") == 0) {
        coronet::ledService().cancelPreview();
        Serial.println("[console] LED preview stopped");
    } else if (strncmp(serialCommand, "led preview ", 12) == 0) {
        unsigned category = 0;
        unsigned animation = 0;
        if (sscanf(serialCommand + 12, "%u %u", &category, &animation) == 2 && category < 6U) {
            coronet::ledService().requestPreview(
                static_cast<coronet::LedCategory>(category), static_cast<uint8_t>(animation));
            Serial.printf("[console] LED preview category=%u animation=%u\n", category, animation);
        } else {
            Serial.println("[console] usage: led preview <category 0-5> <animation>");
        }
    } else if (strcmp(serialCommand, "vent status") == 0) {
        coronet::ventService().logStatus();
    } else if (strncmp(serialCommand, "vent mode ", 10) == 0) {
        const int mode = atoi(serialCommand + 10);
        if (mode >= 0 && mode <= 2) {
            coronet::settingsService().mutableSettings().ventMode = static_cast<coronet::VentMode>(mode);
            coronet::settingsService().save();
            coronet::ventService().applyNow();
            Serial.printf("[console] vent mode=%d\n", mode);
        }
    } else if (strncmp(serialCommand, "vent manual ", 12) == 0) {
        unsigned fan = 0;
        unsigned flap = 0;
        if (sscanf(serialCommand + 12, "%u %u", &fan, &flap) == 2 && fan <= 100U && flap <= 100U) {
            coronet::AppSettings& settings = coronet::settingsService().mutableSettings();
            settings.ventMode = coronet::VentMode::Manual;
            settings.manualFanPercent = static_cast<uint8_t>(fan);
            settings.manualFlapPercent = static_cast<uint8_t>(flap);
            coronet::settingsService().save();
            coronet::ventService().applyNow();
            Serial.printf("[console] vent manual fan=%u flap=%u\n", fan, flap);
        } else {
            Serial.println("[console] usage: vent manual <fan 0-100> <flap 0-100>");
        }
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
    coronet::ledService().begin();
    systemHealth.checkpoint("led");
    audioService.begin();
    systemHealth.checkpoint("audio");
    coronet::wifiService().begin();
    systemHealth.checkpoint("wifi");
    coronet::printerService().begin();
    systemHealth.checkpoint("printer");
    coronet::ventService().begin();
    systemHealth.checkpoint("vent");
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
    coronet::ledService().loop();
    audioService.loop();
    coronet::wifiService().loop();
    coronet::printerService().loop();
    coronet::ventService().loop();
    webControlService.loop();
    bleService.loop();
    delay(10);
}
