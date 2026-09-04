#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "audio/AudioService.h"
#include "ble/BleService.h"
#include "boot/BootExperience.h"
#include "companion/PairingService.h"
#include "config/AppConfig.h"
#include "config/HardwareConfig.h"
#include "core/MemoryService.h"
#include "core/QuietService.h"
#include "core/SystemState.h"
#include "core/SystemHealth.h"
#include "bsp/esp_bsp.h"
#include "display/DisplayService.h"
#include "led/LedService.h"
#include "panda/PandaBreathService.h"
#include "printer/PrinterService.h"
#include "settings/SettingsService.h"
#include "web/WebControlService.h"
#include "wifi/WifiService.h"
#include "vent/VentService.h"
#include "update/OtaService.h"

SET_LOOP_TASK_STACK_SIZE(6144);

namespace {

coronet::DisplayService displayService;
coronet::SystemHealth systemHealth;
coronet::WebControlService webControlService;
char serialCommand[96] = "";
size_t serialCommandLength = 0;

enum class BootStage : uint32_t {
    Entry = 1,
    Memory,
    Settings,
    Display,
    Led,
    Audio,
    Wifi,
    Printer,
    Vent,
    Panda,
    Web,
    Ble,
    Ota,
    Running,
};

constexpr uint32_t BootStageMagic = 0x434E3252UL;
RTC_NOINIT_ATTR uint32_t rtcBootStageMagic;
RTC_NOINIT_ATTR uint32_t rtcBootStageValue;
RTC_NOINIT_ATTR uint32_t rtcBootStageInverse;

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "external-reset";
        case ESP_RST_SW: return "software-reset";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt-watchdog";
        case ESP_RST_TASK_WDT: return "task-watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_UNKNOWN:
        default: return "unknown";
    }
}

const char* bootStageName(uint32_t stage) {
    switch (static_cast<BootStage>(stage)) {
        case BootStage::Entry: return "entry";
        case BootStage::Memory: return "memory";
        case BootStage::Settings: return "settings";
        case BootStage::Display: return "display-touch";
        case BootStage::Led: return "led";
        case BootStage::Audio: return "audio";
        case BootStage::Wifi: return "wifi";
        case BootStage::Printer: return "printer";
        case BootStage::Vent: return "vent";
        case BootStage::Panda: return "panda";
        case BootStage::Web: return "web";
        case BootStage::Ble: return "ble";
        case BootStage::Ota: return "ota";
        case BootStage::Running: return "running";
        default: return "invalid";
    }
}

void setBootStage(BootStage stage) {
    const uint32_t value = static_cast<uint32_t>(stage);
    rtcBootStageValue = value;
    rtcBootStageInverse = ~value;
    rtcBootStageMagic = BootStageMagic;
}

void logBootDiagnostics() {
    const esp_reset_reason_t reason = esp_reset_reason();
    const bool priorStageValid = rtcBootStageMagic == BootStageMagic &&
                                 rtcBootStageInverse == ~rtcBootStageValue;
    Serial.printf("[boot] reset=%u (%s) previous-stage=%s\n",
                  static_cast<unsigned>(reason), resetReasonName(reason),
                  priorStageValid ? bootStageName(rtcBootStageValue) : "unavailable");
    setBootStage(BootStage::Entry);
}

void logTaskDiagnostics() {
    const UBaseType_t capacity = uxTaskGetNumberOfTasks() + 8U;
    TaskStatus_t* tasks = static_cast<TaskStatus_t*>(heap_caps_calloc(
        capacity, sizeof(TaskStatus_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tasks) {
        Serial.println("[tasks] PSRAM allocation failed");
        return;
    }

    const UBaseType_t count = uxTaskGetSystemState(tasks, capacity, nullptr);
    Serial.printf("[tasks] count=%u (stack headroom is minimum observed)\n",
                  static_cast<unsigned>(count));
    for (UBaseType_t i = 0; i < count; ++i) {
        Serial.printf("[task] %-16s priority=%u stackHeadroom=%uB\n",
                      tasks[i].pcTaskName,
                      static_cast<unsigned>(tasks[i].uxCurrentPriority),
                      static_cast<unsigned>(tasks[i].usStackHighWaterMark));
    }
    heap_caps_free(tasks);
}

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
    } else if (strcmp(serialCommand, "audio rescan") == 0) {
        Serial.printf("[console] audio SD rescan %s\n",
                      coronet::audioService().requestStorageRefresh() ? "queued" : "unavailable");
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
    } else if (strcmp(serialCommand, "memory detail") == 0) {
        coronet::logHeapDiagnostics("console");
        coronet::ledService().logStatus();
        coronet::audioService().logStatus();
        coronet::printerService().logStatus();
    } else if (strcmp(serialCommand, "memory tasks") == 0) {
        logTaskDiagnostics();
    } else if (strcmp(serialCommand, "led status") == 0) {
        coronet::ledService().logStatus();
    } else if (strcmp(serialCommand, "led calibration") == 0) {
        Serial.printf("[console] LED color calibration %s\n",
                      coronet::ledService().startColorCalibration() ? "started" : "unavailable");
    } else if (strcmp(serialCommand, "led calibration stop") == 0) {
        coronet::ledService().stopColorCalibration();
        Serial.println("[console] LED color calibration stopped");
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
    // Keep uninitialized LCD memory hidden until LVGL has rendered the boot frame.
    digitalWrite(EXAMPLE_PIN_NUM_QSPI_BL, LOW);
    pinMode(EXAMPLE_PIN_NUM_QSPI_BL, OUTPUT);

    pinMode(coronet::hw::DiyChamberHeaterPin, OUTPUT);
    digitalWrite(coronet::hw::DiyChamberHeaterPin, LOW);
    Serial.begin(coronet::config::SerialBaud);
    delay(150);

    logBootDiagnostics();

    coronet::state().bootMs = millis();

    Serial.println();
    Serial.println(coronet::config::FirmwareName);
    Serial.print("Version: ");
    Serial.println(coronet::config::FirmwareVersion);

    coronet::memoryService().begin();
    setBootStage(BootStage::Memory);
    systemHealth.checkpoint("memory");
    coronet::settingsService().begin();
    coronet::quietService().begin();
    coronet::bootExperience().begin(coronet::settingsService().settings().setupDone);
    setBootStage(BootStage::Settings);
    systemHealth.checkpoint("settings");
    systemHealth.begin();
    displayService.begin();
    setBootStage(BootStage::Display);
    systemHealth.checkpoint("display-touch");
    coronet::ledService().begin();
    setBootStage(BootStage::Led);
    systemHealth.checkpoint("led");
    coronet::audioService().begin();
    setBootStage(BootStage::Audio);
    systemHealth.checkpoint("audio");
    coronet::memoryService().reserveStartupDma(64U * 1024U);
    coronet::wifiService().begin();
    setBootStage(BootStage::Wifi);
    systemHealth.checkpoint("wifi");
    coronet::printerService().begin();
    setBootStage(BootStage::Printer);
    systemHealth.checkpoint("printer");
    coronet::ventService().begin();
    setBootStage(BootStage::Vent);
    systemHealth.checkpoint("vent");
    coronet::pandaBreathService().begin();
    setBootStage(BootStage::Panda);
    systemHealth.checkpoint("panda");
    webControlService.begin();
    setBootStage(BootStage::Web);
    systemHealth.checkpoint("web");
    coronet::bleService().begin();
    setBootStage(BootStage::Ble);
    systemHealth.checkpoint("ble");
    coronet::otaService().begin();
    setBootStage(BootStage::Ota);
    coronet::bootExperience().systemReady();
    setBootStage(BootStage::Running);
}

void loop() {
    coronet::bootExperience().loop();
    coronet::settingsService().loop();
    displayService.loop();
    coronet::ledService().loop();
    coronet::audioService().loop();
    if (coronet::bootExperience().protectsFirstImpression()) {
        delay(2);
        return;
    }
    processSerialConsole();
    systemHealth.loop();
    coronet::quietService().loop();
    coronet::wifiService().loop();
    coronet::printerService().loop();
    coronet::ventService().loop();
    coronet::pandaBreathService().loop();
    webControlService.loop();
    coronet::memoryService().runtimeLoop(coronet::state().webReady);
    coronet::pairingService().loop();
    coronet::bleService().loop();
    coronet::otaService().loop();
    delay(10);
}
