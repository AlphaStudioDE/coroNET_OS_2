#include "BleService.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "../config/AppConfig.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemState.h"
#include "../printer/PrinterService.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

BleService* gActiveService = nullptr;
NimBLECharacteristic* gStateChr = nullptr;
NimBLECharacteristic* gCommandChr = nullptr;
NimBLECharacteristic* gEventChr = nullptr;

bool extractJsonStringValue(const char* command, const char* key, char* out, size_t outSize) {
    if (!command || !key || !out || outSize == 0) return false;
    out[0] = '\0';

    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(command, needle);
    if (!p) return false;

    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;

    size_t n = 0;
    while (*p && *p != '"' && n + 1 < outSize) {
        if (*p == '\\' && p[1]) p++;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return true;
}

bool extractJsonUInt16Value(const char* command, const char* key, uint16_t& out) {
    if (!command || !key) return false;

    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(command, needle);
    if (!p) return false;

    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '"') p++;
    char* endPtr = nullptr;
    const unsigned long value = strtoul(p, &endPtr, 10);
    if (endPtr == p || value == 0 || value > 65535UL) return false;
    out = static_cast<uint16_t>(value);
    return true;
}

bool extractJsonBoolValue(const char* command, const char* key, bool& out) {
    if (!command || !key) return false;

    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(command, needle);
    if (!p) return false;

    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '"') p++;
    if (strncmp(p, "true", 4) == 0 || *p == '1') {
        out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0 || *p == '0') {
        out = false;
        return true;
    }
    return false;
}

void normalizePrinterHost(const char* input, char* out, size_t outSize, uint16_t* port) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!input) return;

    String host(input);
    host.trim();
    if (host.startsWith("http://")) host.remove(0, 7);
    if (host.startsWith("https://")) host.remove(0, 8);
    const int slash = host.indexOf('/');
    if (slash >= 0) host.remove(slash);

    const int colon = host.lastIndexOf(':');
    if (colon > 0 && port) {
        const String portText = host.substring(colon + 1);
        const int parsedPort = portText.toInt();
        if (parsedPort > 0 && parsedPort <= 65535) {
            *port = static_cast<uint16_t>(parsedPort);
            host.remove(colon);
        }
    }

    size_t n = 0;
    for (size_t i = 0; i < host.length() && n + 1 < outSize; ++i) {
        const char c = host[i];
        const bool ok = (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') ||
                        c == '.' || c == '-' || c == '_';
        if (ok) out[n++] = c;
    }
    out[n] = '\0';
}

void jsonStringCopy(const char* input, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!input) return;

    size_t n = 0;
    while (*input && n + 1 < outSize) {
        const char c = *input++;
        if (c == '"' || c == '\\') {
            if (n + 2 >= outSize) break;
            out[n++] = '\\';
            out[n++] = c;
        } else if (static_cast<uint8_t>(c) >= 32) {
            out[n++] = c;
        }
    }
    out[n] = '\0';
}

int16_t tempToTenths(float value) {
    if (isnan(value)) return INT16_MIN;
    if (value > 3276.0f) return INT16_MAX;
    if (value < -3276.0f) return INT16_MIN + 1;
    return static_cast<int16_t>(value * 10.0f);
}

const char* printerStateName(PrinterState state) {
    switch (state) {
        case PrinterState::Idle: return "idle";
        case PrinterState::Printing: return "printing";
        case PrinterState::Paused: return "paused";
        case PrinterState::Error: return "error";
        case PrinterState::Complete: return "complete";
        case PrinterState::Unknown:
        default: return "unknown";
    }
}

class ServerCallbacks final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        (void)server;
        (void)connInfo;
        if (gActiveService) {
            gActiveService->publishEvent("ble", "connected");
        }
        state().bleConnected = true;
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        (void)connInfo;
        (void)reason;
        state().bleConnected = false;
        if (server) server->startAdvertising();
    }
};

class CommandCallbacks final : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        (void)connInfo;
        if (!gActiveService || !characteristic) return;

        std::string value = characteristic->getValue();
        if (value.empty()) return;

        gActiveService->queueCommand(value.data(), value.size());
    }
};

}

void BleService::begin() {
    if (started_) return;
    const AppSettings& cfg = settingsService().settings();
    if (!cfg.bleEnabled || cfg.companionTransport == CompanionTransport::Wifi) {
        Serial.println("[ble] disabled by companion transport settings");
        return;
    }

    strlcpy(deviceId_, deviceIdentity().id(), sizeof(deviceId_));
    refreshAdvertisedName();

    NimBLEDevice::init(advertisedName_);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    NimBLEDevice::setMTU(247);

    NimBLEServer* server = NimBLEDevice::createServer();
    if (!server) return;
    server->setCallbacks(new ServerCallbacks());
    server->advertiseOnDisconnect(true);

    NimBLEService* service = server->createService(config::BleServiceUuid);
    if (!service) return;

    gStateChr = service->createCharacteristic(
        config::BleStateUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        256);
    gCommandChr = service->createCharacteristic(
        config::BleCommandUuid,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        192);
    gEventChr = service->createCharacteristic(
        config::BleEventUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        512);

    if (!gStateChr || !gCommandChr || !gEventChr) return;
    gActiveService = this;
    gCommandChr->setCallbacks(new CommandCallbacks());

    server->start();
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    const bool uuidOk = advertising->addServiceUUID(config::BleServiceUuid);
    const bool nameOk = advertising->setName(advertisedName_);
    const bool startOk = advertising->start();
    Serial.printf("[ble] advertising id=%s name=%s uuid=%u nameOk=%u start=%u\n",
                  deviceId_,
                  advertisedName_,
                  uuidOk ? 1 : 0,
                  nameOk ? 1 : 0,
                  startOk ? 1 : 0);

    started_ = true;
    state().bleReady = true;
    publishEvent("boot", "coroNET OS 2 ready");
    publishState(true);
}

void BleService::loop() {
    if (!started_) return;
    connected_ = state().bleConnected;
    if (commandPending_) {
        char command[sizeof(pendingCommand_)] = "";
        noInterrupts();
        strlcpy(command, pendingCommand_, sizeof(command));
        pendingCommand_[0] = '\0';
        commandPending_ = false;
        interrupts();
        handleCommand(command);
    }
    publishState(false);
}

void BleService::refreshAdvertisedName() {
    deviceIdentity().effectiveName(settingsService().settings().deviceName, advertisedName_, sizeof(advertisedName_));
}

void BleService::publishEvent(const char* type, const char* message) {
    if (!gEventChr) return;
    char safeType[48];
    char safeMessage[320];
    jsonStringCopy(type ? type : "event", safeType, sizeof(safeType));
    jsonStringCopy(message ? message : "", safeMessage, sizeof(safeMessage));

    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"t\":\"e\",\"r\":%lu,\"type\":\"%s\",\"msg\":\"%s\"}",
             ++revision_,
             safeType,
             safeMessage);
    gEventChr->setValue(payload);
    if (connected_) gEventChr->notify();
}

void BleService::queueCommand(const char* command, size_t length) {
    if (!command || length == 0) return;
    const size_t n = min(length, sizeof(pendingCommand_) - 1U);
    memcpy(pendingCommand_, command, n);
    pendingCommand_[n] = '\0';
    commandPending_ = true;
}

void BleService::handleCommand(const char* command) {
    if (!command || !*command) return;

    if (strcmp(command, "snapshot") == 0 || strstr(command, "\"snapshot\"")) {
        publishState(true);
        publishEvent("ack", "snapshot");
        return;
    }

    if (strcmp(command, "ping") == 0 || strstr(command, "\"ping\"")) {
        publishEvent("ack", "pong");
        return;
    }

    if (strcmp(command, "getSettings") == 0 || strstr(command, "getSettings")) {
        publishSettings();
        publishEvent("ack", "settings");
        return;
    }

    if (strstr(command, "setWifi")) {
        char ssid[sizeof(settingsService().mutableSettings().wifiSsid)] = "";
        char password[sizeof(settingsService().mutableSettings().wifiPassword)] = "";
        if (!extractJsonStringValue(command, "ssid", ssid, sizeof(ssid))) {
            publishEvent("ack", "wifi_ssid_missing");
            return;
        }
        extractJsonStringValue(command, "password", password, sizeof(password));

        AppSettings& cfg = settingsService().mutableSettings();
        strlcpy(cfg.wifiSsid, ssid, sizeof(cfg.wifiSsid));
        strlcpy(cfg.wifiPassword, password, sizeof(cfg.wifiPassword));
        settingsService().save();
        publishSettings();
        publishEvent("ack", "wifi_saved");
        return;
    }

    if (strstr(command, "setPrinter")) {
        char rawHost[sizeof(settingsService().mutableSettings().printerHost) + 16] = "";
        char cleanHost[sizeof(settingsService().mutableSettings().printerHost)] = "";
        char apiKey[sizeof(settingsService().mutableSettings().printerApiKey)] = "";
        uint16_t port = settingsService().settings().printerPort ? settingsService().settings().printerPort : 7125;
        if (!extractJsonStringValue(command, "host", rawHost, sizeof(rawHost))) {
            publishEvent("ack", "printer_host_missing");
            return;
        }
        extractJsonUInt16Value(command, "port", port);
        extractJsonStringValue(command, "apiKey", apiKey, sizeof(apiKey));
        normalizePrinterHost(rawHost, cleanHost, sizeof(cleanHost), &port);
        if (!cleanHost[0]) {
            publishEvent("ack", "printer_host_invalid");
            return;
        }

        AppSettings& cfg = settingsService().mutableSettings();
        strlcpy(cfg.printerHost, cleanHost, sizeof(cfg.printerHost));
        cfg.printerPort = port;
        strlcpy(cfg.printerApiKey, apiKey, sizeof(cfg.printerApiKey));
        settingsService().save();
        publishSettings();
        publishEvent("ack", "printer_saved");
        return;
    }

    if (strstr(command, "testPrinterConnection")) {
        PrinterTestResult result = printerService().testConnection();
        publishState(true);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s:%d", result.message, result.httpCode);
        publishEvent(result.ok ? "printer_test_ok" : "printer_test_failed", msg);
        return;
    }

    if (strstr(command, "setSetupDone")) {
        bool done = true;
        extractJsonBoolValue(command, "done", done);
        AppSettings& cfg = settingsService().mutableSettings();
        cfg.setupDone = done;
        settingsService().save();
        state().setupDone = done;
        publishSettings();
        publishState(true);
        publishEvent("ack", done ? "setup_done" : "setup_open");
        return;
    }

    if (strstr(command, "resetDeviceName")) {
        AppSettings& cfg = settingsService().mutableSettings();
        cfg.deviceName[0] = '\0';
        settingsService().save();
        refreshAdvertisedName();
        NimBLEDevice::setDeviceName(advertisedName_);
        if (NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising()) {
            advertising->enableScanResponse(true);
            advertising->setName(advertisedName_);
            advertising->refreshAdvertisingData();
        }
        publishState(true);
        publishEvent("ack", "device_name_reset");
        return;
    }

    if (strstr(command, "setDeviceName") || strstr(command, "setName")) {
        char requestedName[sizeof(settingsService().mutableSettings().deviceName)] = "";
        char cleanName[sizeof(settingsService().mutableSettings().deviceName)] = "";
        if (extractJsonStringValue(command, "name", requestedName, sizeof(requestedName))) {
            deviceIdentity().sanitizeName(requestedName, cleanName, sizeof(cleanName));
            AppSettings& cfg = settingsService().mutableSettings();
            strlcpy(cfg.deviceName, cleanName, sizeof(cfg.deviceName));
            settingsService().save();
            refreshAdvertisedName();
            NimBLEDevice::setDeviceName(advertisedName_);
            if (NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising()) {
                advertising->enableScanResponse(true);
                advertising->setName(advertisedName_);
                advertising->refreshAdvertisingData();
            }
            publishState(true);
            publishEvent("ack", cleanName[0] ? "device_name_saved" : "device_name_reset");
            return;
        }
        publishEvent("ack", "device_name_missing");
        return;
    }

    if (strstr(command, "setCompanionTransport")) {
        char mode[16] = "";
        if (!extractJsonStringValue(command, "mode", mode, sizeof(mode))) {
            publishEvent("ack", "transport_mode_missing");
            return;
        }
        for (char* p = mode; *p; ++p) {
            if (*p >= 'A' && *p <= 'Z') *p = static_cast<char>(*p - 'A' + 'a');
        }
        AppSettings& cfg = settingsService().mutableSettings();
        if (strcmp(mode, "auto") == 0) cfg.companionTransport = CompanionTransport::Auto;
        else if (strcmp(mode, "ble") == 0 || strcmp(mode, "bt") == 0) cfg.companionTransport = CompanionTransport::Ble;
        else if (strcmp(mode, "wifi") == 0) cfg.companionTransport = CompanionTransport::Wifi;
        else {
            publishEvent("ack", "transport_mode_invalid");
            return;
        }
        settingsService().save();
        publishSettings();
        publishEvent("ack", "transport_saved_restart_ble_if_needed");
        return;
    }

    publishEvent("ack", "unknown_command");
}

void BleService::publishSettings() {
    if (!gEventChr) return;

    const AppSettings& cfg = settingsService().settings();
    char safeName[64];
    char safeSsid[80];
    char safePrinterHost[80];
    jsonStringCopy(advertisedName_, safeName, sizeof(safeName));
    jsonStringCopy(cfg.wifiSsid, safeSsid, sizeof(safeSsid));
    jsonStringCopy(cfg.printerHost, safePrinterHost, sizeof(safePrinterHost));

    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"t\":\"settings\",\"r\":%lu,\"id\":\"%s\",\"name\":\"%s\","
             "\"setupDone\":%u,\"bleEnabled\":%u,\"transport\":%u,\"brightness\":%u,"
             "\"uiSkin\":%u,\"uiColor\":%u,\"wifiSsid\":\"%s\","
             "\"printerHost\":\"%s\",\"printerPort\":%u,\"printerApiKeySet\":%u}",
             ++revision_,
             deviceId_,
             safeName,
             cfg.setupDone ? 1 : 0,
             cfg.bleEnabled ? 1 : 0,
             static_cast<unsigned>(cfg.companionTransport),
             static_cast<unsigned>(cfg.displayBrightness),
             static_cast<unsigned>(cfg.uiSkin),
             static_cast<unsigned>(cfg.uiColorMode),
             safeSsid,
             safePrinterHost,
             static_cast<unsigned>(cfg.printerPort),
             cfg.printerApiKey[0] ? 1 : 0);

    gEventChr->setValue(payload);
    if (connected_) gEventChr->notify();
}

void BleService::publishState(bool force) {
    if (!gStateChr) return;
    const unsigned long now = millis();
    if (!force && !stateDirty_ && now - lastNotifyMs_ < config::BleStateNotifyIntervalMs) return;

    const SystemState& s = state();
    char safeName[32];
    char safePrinterStatus[48];
    jsonStringCopy(advertisedName_, safeName, sizeof(safeName));
    jsonStringCopy(s.printerStatusText, safePrinterStatus, sizeof(safePrinterStatus));

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"t\":\"s\",\"r\":%lu,\"id\":\"%s\",\"n\":\"%s\",\"fw\":\"%s\","
             "\"up\":%lu,\"setup\":%u,\"wifi\":%u,\"web\":%u,\"ble\":%u,\"disp\":%u,\"touch\":%u,"
             "\"pcfg\":%u,\"pon\":%u,\"ps\":\"%s\",\"stat\":\"%s\",\"p\":%u,"
             "\"tool\":%u,\"tt\":%d,\"bt\":%d,\"ct\":%d}",
             ++revision_,
             deviceId_,
             safeName,
             config::FirmwareVersion,
             static_cast<unsigned long>(s.uptimeMs),
             s.setupDone ? 1 : 0,
             s.wifiConnected ? 1 : 0,
             s.webReady ? 1 : 0,
             s.bleConnected ? 1 : 0,
             s.displayReady ? 1 : 0,
             s.touchReady ? 1 : 0,
             s.printerConfigured ? 1 : 0,
             s.printerConnected ? 1 : 0,
             printerStateName(s.printerState),
             safePrinterStatus,
             static_cast<unsigned>(s.printProgress),
             static_cast<unsigned>(s.activeTool),
             tempToTenths(s.activeToolTempC),
             tempToTenths(s.bedTempC),
             tempToTenths(s.chamberTempC));

    gStateChr->setValue(payload);
    if (connected_) gStateChr->notify();
    lastNotifyMs_ = now;
    stateDirty_ = false;
}

}
