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
coronet::WifiService wifiService;
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
    coronet::settingsService().begin();
    systemHealth.begin();
    displayService.begin();
    audioService.begin();
    wifiService.begin();
    coronet::printerService().begin();
    webControlService.begin();
    bleService.begin();
}

void loop() {
    systemHealth.loop();
    displayService.loop();
    audioService.loop();
    wifiService.loop();
    coronet::printerService().loop();
    webControlService.loop();
    bleService.loop();
    delay(10);
}
