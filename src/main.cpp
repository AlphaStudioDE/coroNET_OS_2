#include <Arduino.h>

#include "audio/AudioService.h"
#include "ble/BleService.h"
#include "config/AppConfig.h"
#include "core/MemoryService.h"
#include "core/QuietService.h"
#include "core/SystemState.h"
#include "core/SystemHealth.h"
#include "display/DisplayService.h"
#include "led/LedService.h"
#include "panda/PandaBreathService.h"
#include "printer/PrinterService.h"
#include "settings/SettingsService.h"
#include "web/WebControlService.h"
#include "wifi/WifiService.h"
#include "vent/VentService.h"
#include "update/OtaService.h"

namespace {

coronet::DisplayService displayService;
coronet::BleService bleService;
coronet::SystemHealth systemHealth;
coronet::WebControlService webControlService;
char serialCommand[96] = "";
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
        coronet::audioService().playTestTone();
    } else if (strcmp(serialCommand, "audio stop") == 0) {
        coronet::audioService().stop();
    } else if (strcmp(serialCommand, "audio status") == 0) {
        coronet::audioService().logStatus();
    } else if (strcmp(serialCommand, "sd status") == 0) {
        Serial.printf("[console] SD %s\n", coronet::audioService().mountStorage() ? "ready" : "unavailable");
    } else if (strncmp(serialCommand, "audio play ", 11) == 0) {
        coronet::audioService().playFile(serialCommand + 11);
    } else if (strncmp(serialCommand, "audio scenario ", 15) == 0) {
        const int scenario = atoi(serialCommand + 15);
        if (scenario >= 0 && scenario < coronet::enumCount(coronet::SoundScenario{})) {
            coronet::audioService().playScenario(static_cast<coronet::SoundScenario>(scenario));
        }
    } else if (strcmp(serialCommand, "audio release") == 0) {
        coronet::audioService().release();
    } else if (strcmp(serialCommand, "audio profile balanced") == 0) {
        coronet::audioService().useDmaProfile(coronet::AudioDmaProfile::Balanced);
    } else if (strcmp(serialCommand, "audio profile coronet1") == 0) {
        coronet::audioService().useDmaProfile(coronet::AudioDmaProfile::Coronet1);
    } else if (strcmp(serialCommand, "audio rate 22050") == 0) {
        coronet::audioService().setSampleRate(22050);
    } else if (strcmp(serialCommand, "audio rate 44100") == 0) {
        coronet::audioService().setSampleRate(44100);
    } else if (strcmp(serialCommand, "audio rate 48000") == 0) {
        coronet::audioService().setSampleRate(48000);
    } else if (strcmp(serialCommand, "ui home") == 0) {
        displayService.requestPage(coronet::ui::Page::Home);
        Serial.println("[console] Home screen requested");
    } else if (strcmp(serialCommand, "ui settings") == 0) {
        displayService.requestPage(coronet::ui::Page::Settings);
        Serial.println("[console] Settings screen requested");
    } else if (strcmp(serialCommand, "ui led") == 0) {
        displayService.requestPage(coronet::ui::Page::Led);
    } else if (strcmp(serialCommand, "ui vent") == 0) {
        displayService.requestPage(coronet::ui::Page::Vent);
    } else if (strcmp(serialCommand, "ui sound") == 0) {
        displayService.requestPage(coronet::ui::Page::Sound);
    } else if (strcmp(serialCommand, "saver test") == 0) {
        coronet::state().lastTouchMs = millis() - 6UL * 60000UL;
    } else if (strcmp(serialCommand, "saver wake") == 0) {
        coronet::state().lastTouchMs = millis();
    } else if (strncmp(serialCommand, "theme ", 6) == 0) {
        unsigned skin = 0, color = 0, hue = 190;
        if (sscanf(serialCommand + 6, "%u %u %u", &skin, &color, &hue) >= 2 && skin < 4U && color < 3U && hue < 360U) {
            coronet::AppSettings& settings = coronet::settingsService().mutableSettings();
            settings.uiSkin = static_cast<coronet::UiSkin>(skin);
            settings.uiColorMode = static_cast<coronet::UiColorMode>(color);
            settings.accentHueDegrees = static_cast<uint16_t>(hue);
            coronet::settingsService().save();
            Serial.printf("[console] theme skin=%u color=%u hue=%u\n", skin, color, hue);
        }
    } else if (strncmp(serialCommand, "clock style ", 12) == 0) {
        const int style = atoi(serialCommand + 12);
        if (style >= 0 && style < static_cast<int>(coronet::ClockStyle::Count)) {
            coronet::settingsService().mutableSettings().clockStyle = static_cast<coronet::ClockStyle>(style);
            coronet::settingsService().save();
        }
    } else if (strcmp(serialCommand, "ota check") == 0) {
        Serial.printf("[console] OTA check %s\n", coronet::otaService().requestCheck() ? "queued" : "busy");
    } else if (strcmp(serialCommand, "ota install") == 0) {
        Serial.printf("[console] OTA install %s\n", coronet::otaService().requestInstall(false) ? "queued" : "busy");
    } else if (strcmp(serialCommand, "ota reinstall") == 0) {
        Serial.printf("[console] OTA reinstall %s\n", coronet::otaService().requestInstall(true) ? "queued" : "busy");
    } else if (strcmp(serialCommand, "ota sd") == 0) {
        Serial.printf("[console] SD recovery %s\n", coronet::otaService().requestSdRecovery() ? "queued" : "busy");
    } else if (strcmp(serialCommand, "ota status") == 0) {
        coronet::otaService().logStatus();
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
    } else if (strcmp(serialCommand, "panda status") == 0) {
        coronet::pandaBreathService().logStatus();
    } else if (strncmp(serialCommand, "panda host ", 11) == 0) {
        coronet::AppSettings& settings = coronet::settingsService().mutableSettings();
        strlcpy(settings.pandaHost, serialCommand + 11, sizeof(settings.pandaHost));
        settings.pandaEnabled = true;
        coronet::settingsService().save();
        coronet::pandaBreathService().applyNow();
        Serial.printf("[console] Panda host=%s enabled=1\n", settings.pandaHost);
    } else if (strncmp(serialCommand, "panda mode ", 11) == 0) {
        const int mode = atoi(serialCommand + 11);
        if (mode >= 0 && mode < static_cast<int>(coronet::PandaBreathMode::Count)) {
            coronet::settingsService().mutableSettings().pandaMode =
                static_cast<coronet::PandaBreathMode>(mode);
            coronet::settingsService().save();
            coronet::pandaBreathService().applyNow();
            Serial.printf("[console] Panda mode=%d\n", mode);
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
    coronet::quietService().begin();
    systemHealth.checkpoint("settings");
    systemHealth.begin();
    displayService.begin();
    systemHealth.checkpoint("display-touch");
    coronet::ledService().begin();
    systemHealth.checkpoint("led");
    coronet::audioService().begin();
    systemHealth.checkpoint("audio");
    coronet::wifiService().begin();
    systemHealth.checkpoint("wifi");
    coronet::printerService().begin();
    systemHealth.checkpoint("printer");
    coronet::ventService().begin();
    systemHealth.checkpoint("vent");
    coronet::pandaBreathService().begin();
    systemHealth.checkpoint("panda");
    webControlService.begin();
    systemHealth.checkpoint("web");
    bleService.begin();
    systemHealth.checkpoint("ble");
    coronet::otaService().begin();
}

void loop() {
    processSerialConsole();
    systemHealth.loop();
    coronet::settingsService().loop();
    coronet::quietService().loop();
    displayService.loop();
    coronet::ledService().loop();
    coronet::audioService().loop();
    coronet::wifiService().loop();
    coronet::printerService().loop();
    coronet::ventService().loop();
    coronet::pandaBreathService().loop();
    webControlService.loop();
    bleService.loop();
    coronet::otaService().loop();
    delay(10);
}
