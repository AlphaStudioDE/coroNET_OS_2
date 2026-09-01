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
