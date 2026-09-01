#include "BleService.h"

#include <algorithm>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "BleProtocol.h"
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

bool readBoundedString(JsonVariantConst value, char* out, size_t outSize, bool required) {
    if (!out || outSize == 0) return false;
    if (!value.is<const char*>()) return !required;

    const char* text = value.as<const char*>();
    if (!text || strlen(text) >= outSize) return false;
    strlcpy(out, text, outSize);
    return true;
}

class ServerCallbacks final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        (void)server;
        if (gActiveService) gActiveService->onConnected(connInfo.getMTU());
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        (void)connInfo;
        (void)reason;
        if (gActiveService) gActiveService->onDisconnected();
        if (server && gActiveService && gActiveService->active()) server->startAdvertising();
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) override {
        (void)connInfo;
        if (gActiveService) gActiveService->onMtuChanged(mtu);
    }
};

class CommandCallbacks final : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        (void)connInfo;
        if (!gActiveService || !characteristic) return;

        const std::string value = characteristic->getValue();
        if (!value.empty()) gActiveService->queueCommand(value.data(), value.size());
    }
};

}

void BleService::begin() {
    if (!commandQueue_) {
        commandQueue_ = xQueueCreate(CommandQueueDepth, sizeof(QueuedCommand));
        if (!commandQueue_) {
            Serial.println("[ble] command queue allocation failed");
            state().bleReady = false;
            return;
        }
    }

    gActiveService = this;
    strlcpy(deviceId_, deviceIdentity().id(), sizeof(deviceId_));
    appliedSettingsRevision_ = settingsService().revision();
    applySettings();
}

void BleService::loop() {
    applySettings();
    if (!started_) return;

    bool connectedEvent = false;
    bool disconnectedEvent = false;
    bool overflowEvent = false;
    portENTER_CRITICAL(&connectionMux_);
    connectedEvent = connectionEventPending_;
    disconnectedEvent = disconnectEventPending_;
    overflowEvent = commandOverflowPending_;
    connectionEventPending_ = false;
    disconnectEventPending_ = false;
    commandOverflowPending_ = false;
    portEXIT_CRITICAL(&connectionMux_);

    if (connectedEvent) {
        state().bleConnected = true;
        publishEvent("ble", fallbackActive_ ? "connected_fallback" : "connected");
        publishState(true);
    }
    if (disconnectedEvent) {
        state().bleConnected = false;
        stateDirty_ = true;
        pairingTokenIssued_ = false;
    }
    if (overflowEvent) publishEvent("error", "command_queue_full");

    QueuedCommand queued{};
    uint8_t handled = 0;
    while (handled < CommandQueueDepth && xQueueReceive(commandQueue_, &queued, 0) == pdTRUE) {
        handleCommand(queued.data, queued.length);
        handled++;
    }

    publishState(false);
}

void BleService::startStack() {
    if (started_) return;

    portENTER_CRITICAL(&connectionMux_);
    connectionEventPending_ = false;
    disconnectEventPending_ = false;
    portEXIT_CRITICAL(&connectionMux_);

    refreshAdvertisedName();
    NimBLEDevice::init(advertisedName_);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    NimBLEDevice::setMTU(247);

    NimBLEServer* server = NimBLEDevice::createServer();
    if (!server) {
        NimBLEDevice::deinit(true);
        return;
    }
    server->setCallbacks(new ServerCallbacks());
    server->advertiseOnDisconnect(true);

    NimBLEService* service = server->createService(config::BleServiceUuid);
    if (!service) {
        NimBLEDevice::deinit(true);
        return;
    }

    gStateChr = service->createCharacteristic(
        config::BleStateUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        256);
    gCommandChr = service->createCharacteristic(
        config::BleCommandUuid,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        CommandMaxLength);
    gEventChr = service->createCharacteristic(
        config::BleEventUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        512);

    if (!gStateChr || !gCommandChr || !gEventChr) {
        NimBLEDevice::deinit(true);
        gStateChr = nullptr;
        gCommandChr = nullptr;
        gEventChr = nullptr;
        return;
    }

    gCommandChr->setCallbacks(new CommandCallbacks());
    server->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    const bool uuidOk = advertising->addServiceUUID(config::BleServiceUuid);
    const bool nameOk = advertising->setName(advertisedName_);
    const bool startOk = advertising->start();

    portENTER_CRITICAL(&connectionMux_);
    started_ = startOk;
    portEXIT_CRITICAL(&connectionMux_);
    state().bleReady = startOk;
    stateDirty_ = true;
    Serial.printf("[ble] advertising id=%s name=%s uuid=%u nameOk=%u start=%u fallback=%u\n",
                  deviceId_, advertisedName_, uuidOk ? 1 : 0, nameOk ? 1 : 0,
                  startOk ? 1 : 0, fallbackActive_ ? 1 : 0);

    if (!startOk) {
        NimBLEDevice::deinit(true);
        gStateChr = nullptr;
        gCommandChr = nullptr;
        gEventChr = nullptr;
    }
}

void BleService::stopStack() {
    if (!started_ && !state().bleReady) return;

    portENTER_CRITICAL(&connectionMux_);
    started_ = false;
    connected_ = false;
    portEXIT_CRITICAL(&connectionMux_);
    state().bleReady = false;
    state().bleConnected = false;
    NimBLEDevice::deinit(true);
    portENTER_CRITICAL(&connectionMux_);
    connectionEventPending_ = false;
    disconnectEventPending_ = false;
    portEXIT_CRITICAL(&connectionMux_);
    gStateChr = nullptr;
    gCommandChr = nullptr;
    gEventChr = nullptr;
    Serial.println("[ble] stopped");
}

void BleService::applySettings() {
    const AppSettings& cfg = settingsService().settings();
    const uint32_t now = millis();
    const bool wifiConnected = state().wifiConnected;

    if (cfg.companionTransport == CompanionTransport::Wifi && !wifiConnected) {
        if (wifiOfflineSinceMs_ == 0) wifiOfflineSinceMs_ = now ? now : 1;
        fallbackActive_ = now - wifiOfflineSinceMs_ >= config::BleWifiFallbackDelayMs;
    } else {
        wifiOfflineSinceMs_ = 0;
        fallbackActive_ = false;
    }

    bool shouldStart = !state().maintenanceMode && (!cfg.apiPaired ||
                       (cfg.bleEnabled && cfg.companionTransport != CompanionTransport::Wifi) ||
                       fallbackActive_);
    if (!state().maintenanceMode && !shouldStart && isConnected()) shouldStart = true;

    if (shouldStart && !started_) startStack();
    else if (!shouldStart && started_) stopStack();

    const uint32_t revision = settingsService().revision();
    if (started_ && revision != appliedSettingsRevision_) {
        updateAdvertisingName();
        stateDirty_ = true;
    }
    appliedSettingsRevision_ = revision;
}

void BleService::refreshAdvertisedName() {
    deviceIdentity().effectiveName(settingsService().settings().deviceName, advertisedName_, sizeof(advertisedName_));
}

void BleService::updateAdvertisingName() {
    char previousName[sizeof(advertisedName_)];
    strlcpy(previousName, advertisedName_, sizeof(previousName));
    refreshAdvertisedName();
    if (strcmp(previousName, advertisedName_) == 0) return;

    NimBLEDevice::setDeviceName(advertisedName_);
    if (NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising()) {
        advertising->enableScanResponse(true);
        advertising->setName(advertisedName_);
        advertising->refreshAdvertisingData();
    }
}

void BleService::onConnected(uint16_t mtu) {
    portENTER_CRITICAL(&connectionMux_);
    connected_ = true;
    connectionMtu_ = mtu >= 23 ? mtu : 23;
    connectionEventPending_ = true;
    portEXIT_CRITICAL(&connectionMux_);
}

void BleService::onDisconnected() {
    portENTER_CRITICAL(&connectionMux_);
    connected_ = false;
    connectionMtu_ = 23;
    disconnectEventPending_ = true;
    portEXIT_CRITICAL(&connectionMux_);
}

bool BleService::active() {
    portENTER_CRITICAL(&connectionMux_);
    const bool value = started_;
    portEXIT_CRITICAL(&connectionMux_);
    return value;
}

bool BleService::isConnected() {
    portENTER_CRITICAL(&connectionMux_);
    const bool value = connected_;
    portEXIT_CRITICAL(&connectionMux_);
    return value;
}

void BleService::onMtuChanged(uint16_t mtu) {
    portENTER_CRITICAL(&connectionMux_);
    connectionMtu_ = mtu >= 23 ? mtu : 23;
    portEXIT_CRITICAL(&connectionMux_);
}

void BleService::queueCommand(const char* command, size_t length) {
    if (!command || length == 0 || !commandQueue_) return;
    if (length >= CommandMaxLength) {
        portENTER_CRITICAL(&connectionMux_);
        commandOverflowPending_ = true;
        portEXIT_CRITICAL(&connectionMux_);
        return;
    }

    QueuedCommand queued{};
    queued.length = static_cast<uint16_t>(length);
    memcpy(queued.data, command, length);
    queued.data[length] = '\0';
    if (xQueueSend(commandQueue_, &queued, 0) != pdTRUE) {
        portENTER_CRITICAL(&connectionMux_);
        commandOverflowPending_ = true;
        portEXIT_CRITICAL(&connectionMux_);
    }
}

void BleService::handleCommand(const char* command, size_t length) {
    if (!command || length == 0) return;

    JsonDocument doc;
    const char* cmd = nullptr;
    if (strcmp(command, "snapshot") == 0 || strcmp(command, "ping") == 0 || strcmp(command, "getSettings") == 0) {
        cmd = command;
    } else {
        const DeserializationError error = deserializeJson(doc, command, length);
        if (error || !doc.is<JsonObject>()) {
            publishEvent("error", "invalid_json");
            return;
        }
        cmd = doc["cmd"].as<const char*>();
        if (!cmd || !*cmd) {
            publishEvent("error", "command_missing");
            return;
        }
    }

    if (strcmp(cmd, "snapshot") == 0) {
        publishState(true);
        publishEvent("ack", "snapshot");
        return;
    }
    if (strcmp(cmd, "ping") == 0) {
        publishEvent("ack", "pong");
        return;
    }
    if (strcmp(cmd, "getSettings") == 0) {
        publishSettings();
        publishEvent("ack", "settings");
        return;
    }
    if (strcmp(cmd, "getPairingToken") == 0) {
        publishPairingToken();
        return;
    }
    if (strcmp(cmd, "confirmPairing") == 0) {
        if (!pairingTokenIssued_) {
            publishEvent("error", "pairing_not_started");
            return;
        }
        AppSettings& cfg = settingsService().mutableSettings();
        if (!cfg.apiPaired) {
            cfg.apiPaired = true;
            settingsService().save();
            settingsService().flush();
        }
        pairingTokenIssued_ = false;
        publishEvent("ack", "pairing_confirmed");
        return;
    }

    if (strcmp(cmd, "setWifi") == 0) {
        AppSettings& cfg = settingsService().mutableSettings();
        char ssid[sizeof(cfg.wifiSsid)] = "";
        char password[sizeof(cfg.wifiPassword)] = "";
        if (!readBoundedString(doc["ssid"], ssid, sizeof(ssid), true)) {
            publishEvent("error", "wifi_ssid_invalid");
            return;
        }
        const bool passwordProvided = doc["password"].is<const char*>();
        if (passwordProvided && !readBoundedString(doc["password"], password, sizeof(password), false)) {
            publishEvent("error", "wifi_password_invalid");
            return;
        }
        strlcpy(cfg.wifiSsid, ssid, sizeof(cfg.wifiSsid));
        if (passwordProvided) strlcpy(cfg.wifiPassword, password, sizeof(cfg.wifiPassword));
        settingsService().save();
        publishSettings();
        publishEvent("ack", "wifi_saved");
        return;
    }

    if (strcmp(cmd, "setPrinter") == 0) {
        AppSettings& cfg = settingsService().mutableSettings();
        char rawHost[sizeof(cfg.printerHost) + 16] = "";
        char cleanHost[sizeof(cfg.printerHost)] = "";
        char apiKey[sizeof(cfg.printerApiKey)] = "";
        if (!readBoundedString(doc["host"], rawHost, sizeof(rawHost), true)) {
            publishEvent("error", "printer_host_invalid");
            return;
        }

        uint16_t port = cfg.printerPort ? cfg.printerPort : 7125;
        if (doc["port"].is<int>()) {
            const int requestedPort = doc["port"].as<int>();
            if (requestedPort < 1 || requestedPort > 65535) {
                publishEvent("error", "printer_port_invalid");
                return;
            }
            port = static_cast<uint16_t>(requestedPort);
        }
        const bool apiKeyProvided = doc["apiKey"].is<const char*>();
        if (apiKeyProvided && !readBoundedString(doc["apiKey"], apiKey, sizeof(apiKey), false)) {
            publishEvent("error", "printer_key_invalid");
            return;
        }

        normalizePrinterHost(rawHost, cleanHost, sizeof(cleanHost), &port);
        if (!cleanHost[0]) {
            publishEvent("error", "printer_host_invalid");
            return;
        }
        strlcpy(cfg.printerHost, cleanHost, sizeof(cfg.printerHost));
        cfg.printerPort = port;
        if (apiKeyProvided) strlcpy(cfg.printerApiKey, apiKey, sizeof(cfg.printerApiKey));
        settingsService().save();
        publishSettings();
        publishEvent("ack", "printer_saved");
        return;
    }

    if (strcmp(cmd, "testPrinterConnection") == 0) {
        const PrinterTestResult result = printerService().testConnection();
        publishState(true);
        char message[128];
        snprintf(message, sizeof(message), "%s:%d", result.message, result.httpCode);
        publishEvent(result.ok ? "printer_test_ok" : "printer_test_failed", message);
        return;
    }

    if (strcmp(cmd, "setSetupDone") == 0) {
        if (!doc["done"].is<bool>()) {
            publishEvent("error", "setup_done_invalid");
            return;
        }
        AppSettings& cfg = settingsService().mutableSettings();
        cfg.setupDone = doc["done"].as<bool>();
        state().setupDone = cfg.setupDone;
        settingsService().save();
        publishSettings();
        publishState(true);
        publishEvent("ack", cfg.setupDone ? "setup_done" : "setup_open");
        return;
    }

    if (strcmp(cmd, "resetDeviceName") == 0) {
        settingsService().mutableSettings().deviceName[0] = '\0';
        settingsService().save();
        updateAdvertisingName();
        publishState(true);
        publishEvent("ack", "device_name_reset");
        return;
    }

    if (strcmp(cmd, "setDeviceName") == 0 || strcmp(cmd, "setName") == 0) {
        AppSettings& cfg = settingsService().mutableSettings();
        char requestedName[sizeof(cfg.deviceName)] = "";
        char cleanName[sizeof(cfg.deviceName)] = "";
        if (!readBoundedString(doc["name"], requestedName, sizeof(requestedName), true)) {
            publishEvent("error", "device_name_invalid");
            return;
        }
        deviceIdentity().sanitizeName(requestedName, cleanName, sizeof(cleanName));
        strlcpy(cfg.deviceName, cleanName, sizeof(cfg.deviceName));
        settingsService().save();
        updateAdvertisingName();
        publishState(true);
        publishEvent("ack", cleanName[0] ? "device_name_saved" : "device_name_reset");
        return;
    }

    if (strcmp(cmd, "setCompanionTransport") == 0) {
        const char* mode = doc["mode"].as<const char*>();
        if (!mode) {
            publishEvent("error", "transport_mode_missing");
            return;
        }

        AppSettings& cfg = settingsService().mutableSettings();
        if (strcasecmp(mode, "auto") == 0) cfg.companionTransport = CompanionTransport::Auto;
        else if (strcasecmp(mode, "ble") == 0 || strcasecmp(mode, "bt") == 0) cfg.companionTransport = CompanionTransport::Ble;
        else if (strcasecmp(mode, "wifi") == 0) cfg.companionTransport = CompanionTransport::Wifi;
        else {
            publishEvent("error", "transport_mode_invalid");
            return;
        }
        settingsService().save();
        publishSettings();
        publishEvent("ack", "transport_saved");
        return;
    }

    if (strcmp(cmd, "setSettings") == 0) {
        AppSettings& cfg = settingsService().mutableSettings();
        if (doc["displayBrightness"].is<int>()) cfg.displayBrightness = constrain(doc["displayBrightness"].as<int>(), 0, 100);
        if (doc["uiSkin"].is<int>()) cfg.uiSkin = static_cast<UiSkin>(constrain(doc["uiSkin"].as<int>(), 0, 3));
        if (doc["uiColorMode"].is<int>()) cfg.uiColorMode = static_cast<UiColorMode>(constrain(doc["uiColorMode"].as<int>(), 0, 2));
        if (doc["accentHueDegrees"].is<int>()) cfg.accentHueDegrees = constrain(doc["accentHueDegrees"].as<int>(), 0, 359);
        if (doc["screenSaverMode"].is<int>()) cfg.screenSaverMode = static_cast<ScreenSaverMode>(constrain(doc["screenSaverMode"].as<int>(), 0, 2));
        if (doc["screenSaverDelayMinutes"].is<int>()) cfg.screenSaverDelayMinutes = constrain(doc["screenSaverDelayMinutes"].as<int>(), 1, 60);
        if (doc["clockBrightness"].is<int>()) cfg.clockBrightness = constrain(doc["clockBrightness"].as<int>(), 5, 100);
        if (doc["clockStyle"].is<int>()) cfg.clockStyle = static_cast<ClockStyle>(constrain(doc["clockStyle"].as<int>(), 0, static_cast<int>(ClockStyle::Count) - 1));
        if (doc["quietTarget"].is<int>()) cfg.quietTarget = static_cast<QuietTarget>(constrain(doc["quietTarget"].as<int>(), 0, 3));
        if (doc["quietDurationMinutes"].is<int>()) cfg.quietDurationMinutes = constrain(doc["quietDurationMinutes"].as<int>(), 1, 1440);
        if (doc["ledEnabled"].is<bool>()) cfg.ledEnabled = doc["ledEnabled"].as<bool>();
        if (doc["insideColorStyle"].is<int>()) cfg.insideColorStyle = static_cast<InsideColorStyle>(constrain(doc["insideColorStyle"].as<int>(), 0, 1));
        if (doc["mirrorLedLayout"].is<bool>()) cfg.mirrorLedLayout = doc["mirrorLedLayout"].as<bool>();
        if (doc["ledBrightness"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledBrightness"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedSection{}) && i < values.size(); ++i) cfg.ledBrightness[i] = constrain(values[i].as<int>(), 0, 100);
        }
        if (doc["soundVolume"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["soundVolume"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) cfg.soundVolume[i] = constrain(values[i].as<int>(), 0, 100);
        }
        if (doc["ventMode"].is<int>()) cfg.ventMode = static_cast<VentMode>(constrain(doc["ventMode"].as<int>(), 0, 2));
        if (doc["ventTargetTempC"].is<int>()) cfg.ventTargetTempC = constrain(doc["ventTargetTempC"].as<int>(), 20, 80);
        if (doc["manualFanPercent"].is<int>()) cfg.manualFanPercent = constrain(doc["manualFanPercent"].as<int>(), 0, 100);
        if (doc["manualFlapPercent"].is<int>()) cfg.manualFlapPercent = constrain(doc["manualFlapPercent"].as<int>(), 0, 100);
        if (doc["servoClosedUs"].is<int>()) cfg.servoClosedUs = constrain(doc["servoClosedUs"].as<int>(), 500, 2500);
        if (doc["servoOpenUs"].is<int>()) cfg.servoOpenUs = constrain(doc["servoOpenUs"].as<int>(), 500, 2500);
        if (doc["servoReverse"].is<bool>()) cfg.servoReverse = doc["servoReverse"].as<bool>();
        if (doc["pandaEnabled"].is<bool>()) cfg.pandaEnabled = doc["pandaEnabled"].as<bool>();
        if (doc["pandaMode"].is<int>()) cfg.pandaMode = static_cast<PandaBreathMode>(constrain(doc["pandaMode"].as<int>(), 0, static_cast<int>(PandaBreathMode::Count) - 1));
        if (doc["pandaTargetTempC"].is<int>()) cfg.pandaTargetTempC = constrain(doc["pandaTargetTempC"].as<int>(), 30, 60);
        settingsService().save();
        publishSettings();
        publishEvent("ack", "settings_saved");
        return;
    }

    publishEvent("error", "unknown_command");
}

void BleService::publishEvent(const char* type, const char* message) {
    if (!gEventChr || !isConnected()) return;

    char safeType[48];
    char safeMessage[320];
    jsonStringCopy(type ? type : "event", safeType, sizeof(safeType));
    jsonStringCopy(message ? message : "", safeMessage, sizeof(safeMessage));

    char payload[512];
    const int written = snprintf(payload, sizeof(payload),
                                 "{\"v\":%u,\"t\":\"e\",\"r\":%lu,\"type\":\"%s\",\"msg\":\"%s\"}",
                                 bleprotocol::Version,
                                 static_cast<unsigned long>(++revision_),
                                 safeType,
                                 safeMessage);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(payload)) {
        static constexpr char Fallback[] = "{\"v\":1,\"t\":\"e\",\"type\":\"error\",\"msg\":\"event_too_large\"}";
        sendFramed(gEventChr,
                   static_cast<uint8_t>(bleprotocol::MessageType::EventJson),
                   reinterpret_cast<const uint8_t*>(Fallback),
                   strlen(Fallback));
        return;
    }

    sendFramed(gEventChr,
               static_cast<uint8_t>(bleprotocol::MessageType::EventJson),
               reinterpret_cast<const uint8_t*>(payload),
               static_cast<size_t>(written));
}

void BleService::publishSettings() {
    if (!gEventChr || !isConnected()) return;

    const AppSettings& cfg = settingsService().settings();
    char safeName[64];
    char safeSsid[80];
    char safePrinterHost[80];
    jsonStringCopy(advertisedName_, safeName, sizeof(safeName));
    jsonStringCopy(cfg.wifiSsid, safeSsid, sizeof(safeSsid));
    jsonStringCopy(cfg.printerHost, safePrinterHost, sizeof(safePrinterHost));

    char payload[512];
    auto sendPayload = [this, &payload](int written) {
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) {
            publishEvent("error", "settings_too_large");
            return false;
        }
        return sendFramed(gEventChr,
                          static_cast<uint8_t>(bleprotocol::MessageType::SettingsJson),
                          reinterpret_cast<const uint8_t*>(payload),
                          static_cast<size_t>(written));
    };

    int written = snprintf(payload, sizeof(payload),
                                 "{\"v\":%u,\"t\":\"settings\",\"r\":%lu,\"id\":\"%s\",\"name\":\"%s\","
                                 "\"setupDone\":%u,\"bleEnabled\":%u,\"apiPaired\":%u,\"transport\":%u,\"brightness\":%u,"
                                 "\"uiSkin\":%u,\"uiColor\":%u,\"wifiSsid\":\"%s\",\"wifiPasswordSet\":%u,"
                                 "\"printerHost\":\"%s\",\"printerPort\":%u,\"printerApiKeySet\":%u}",
                                 bleprotocol::Version,
                                 static_cast<unsigned long>(++revision_),
                                 deviceId_, safeName,
                                 cfg.setupDone ? 1 : 0,
                                 cfg.bleEnabled ? 1 : 0,
                                 cfg.apiPaired ? 1 : 0,
                                 static_cast<unsigned>(cfg.companionTransport),
                                 static_cast<unsigned>(cfg.displayBrightness),
                                 static_cast<unsigned>(cfg.uiSkin),
                                 static_cast<unsigned>(cfg.uiColorMode),
                                 safeSsid,
                                 cfg.wifiPassword[0] ? 1 : 0,
                                 safePrinterHost,
                                 static_cast<unsigned>(cfg.printerPort),
                                 cfg.printerApiKey[0] ? 1 : 0);
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"appearance\",\"displayBrightness\":%u,"
                       "\"uiSkin\":%u,\"uiColorMode\":%u,\"accentHueDegrees\":%u,\"screenSaverMode\":%u,"
                       "\"screenSaverDelayMinutes\":%u,\"clockBrightness\":%u,\"clockStyle\":%u,"
                       "\"quietTarget\":%u,\"quietDurationMinutes\":%u}",
                       bleprotocol::Version, cfg.displayBrightness, static_cast<unsigned>(cfg.uiSkin),
                       static_cast<unsigned>(cfg.uiColorMode), cfg.accentHueDegrees,
                       static_cast<unsigned>(cfg.screenSaverMode), cfg.screenSaverDelayMinutes,
                       cfg.clockBrightness, static_cast<unsigned>(cfg.clockStyle),
                       static_cast<unsigned>(cfg.quietTarget), cfg.quietDurationMinutes);
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"led_sound\",\"ledEnabled\":%s,"
                       "\"ledBrightness\":[%u,%u,%u,%u],\"insideColorStyle\":%u,\"mirrorLedLayout\":%s,"
                       "\"soundVolume\":[%u,%u,%u,%u,%u]}",
                       bleprotocol::Version, cfg.ledEnabled ? "true" : "false",
                       cfg.ledBrightness[0], cfg.ledBrightness[1], cfg.ledBrightness[2], cfg.ledBrightness[3],
                       static_cast<unsigned>(cfg.insideColorStyle), cfg.mirrorLedLayout ? "true" : "false",
                       cfg.soundVolume[0], cfg.soundVolume[1], cfg.soundVolume[2], cfg.soundVolume[3], cfg.soundVolume[4]);
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"vent\",\"ventMode\":%u,"
                       "\"ventTargetTempC\":%u,\"manualFanPercent\":%u,\"manualFlapPercent\":%u,"
                       "\"servoClosedUs\":%u,\"servoOpenUs\":%u,\"servoReverse\":%s,"
                       "\"pandaEnabled\":%s,\"pandaMode\":%u,\"pandaTargetTempC\":%u}",
                       bleprotocol::Version, static_cast<unsigned>(cfg.ventMode), cfg.ventTargetTempC,
                       cfg.manualFanPercent, cfg.manualFlapPercent, cfg.servoClosedUs, cfg.servoOpenUs,
                       cfg.servoReverse ? "true" : "false", cfg.pandaEnabled ? "true" : "false",
                       static_cast<unsigned>(cfg.pandaMode), cfg.pandaTargetTempC);
    sendPayload(written);
}

void BleService::publishPairingToken() {
    if (!gEventChr || !isConnected()) return;

    const AppSettings& cfg = settingsService().settings();
    if (cfg.apiPaired) {
        publishEvent("error", "pairing_closed");
        return;
    }
    char payload[160];
    const int written = snprintf(payload, sizeof(payload),
                                 "{\"v\":%u,\"t\":\"pairing\",\"id\":\"%s\",\"token\":\"%s\"}",
                                 bleprotocol::Version, deviceId_, cfg.apiToken);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) return;

    pairingTokenIssued_ = sendFramed(gEventChr,
                                     static_cast<uint8_t>(bleprotocol::MessageType::PairingJson),
                                     reinterpret_cast<const uint8_t*>(payload),
                                     static_cast<size_t>(written));
}

void BleService::publishState(bool force) {
    if (!gStateChr || !isConnected()) return;
    const uint32_t now = millis();
    if (!force && !stateDirty_ && now - lastNotifyMs_ < config::BleStateNotifyIntervalMs) return;

    const SystemState& s = state();
    bleprotocol::StateSnapshotV1 snapshot{};
    snapshot.version = bleprotocol::Version;
    snapshot.messageType = static_cast<uint8_t>(bleprotocol::MessageType::StateSnapshot);
    snapshot.size = sizeof(snapshot);
    snapshot.revision = ++revision_;
    snapshot.uptimeMs = s.uptimeMs;
    if (s.setupDone) snapshot.flags |= bleprotocol::SetupDone;
    if (s.wifiConnected) snapshot.flags |= bleprotocol::WifiConnected;
    if (s.webReady) snapshot.flags |= bleprotocol::WebReady;
    if (s.bleConnected) snapshot.flags |= bleprotocol::BleConnected;
    if (s.displayReady) snapshot.flags |= bleprotocol::DisplayReady;
    if (s.touchReady) snapshot.flags |= bleprotocol::TouchReady;
    if (s.printerConfigured) snapshot.flags |= bleprotocol::PrinterConfigured;
    if (s.printerConnected) snapshot.flags |= bleprotocol::PrinterConnected;
    if (s.audioReady) snapshot.flags |= bleprotocol::AudioReady;
    if (fallbackActive_) snapshot.flags |= bleprotocol::BleFallbackActive;
    snapshot.printerState = static_cast<uint8_t>(s.printerState);
    snapshot.printProgress = s.printProgress;
    snapshot.activeTool = s.activeTool;
    snapshot.activeToolTempTenths = tempToTenths(s.activeToolTempC);
    snapshot.bedTempTenths = tempToTenths(s.bedTempC);
    snapshot.chamberTempTenths = tempToTenths(s.chamberTempC);
    strlcpy(snapshot.deviceId, deviceId_, sizeof(snapshot.deviceId));
    strlcpy(snapshot.deviceName, advertisedName_, sizeof(snapshot.deviceName));
    strlcpy(snapshot.printerStatus, s.printerStatusText, sizeof(snapshot.printerStatus));
    strlcpy(snapshot.printFilename, s.printFilename, sizeof(snapshot.printFilename));

    if (sendFramed(gStateChr,
                   static_cast<uint8_t>(bleprotocol::MessageType::StateSnapshot),
                   reinterpret_cast<const uint8_t*>(&snapshot),
                   sizeof(snapshot))) {
        lastNotifyMs_ = now;
        stateDirty_ = false;
    }
}

bool BleService::sendFramed(NimBLECharacteristic* characteristic,
                            uint8_t messageType,
                            const uint8_t* payload,
                            size_t length) {
    if (!characteristic || !payload || length == 0 || length > UINT16_MAX || !isConnected()) return false;

    uint16_t mtu = 23;
    portENTER_CRITICAL(&connectionMux_);
    mtu = connectionMtu_;
    portEXIT_CRITICAL(&connectionMux_);

    const size_t attPayload = std::min<size_t>(bleprotocol::MaxAttPayload, mtu > 3 ? mtu - 3 : 20);
    if (attPayload <= sizeof(bleprotocol::FrameHeader)) return false;
    const size_t chunkPayload = attPayload - sizeof(bleprotocol::FrameHeader);
    const size_t chunkCountRaw = (length + chunkPayload - 1) / chunkPayload;
    if (chunkCountRaw == 0 || chunkCountRaw > UINT8_MAX) return false;

    const uint8_t chunkCount = static_cast<uint8_t>(chunkCountRaw);
    const uint16_t messageId = ++messageId_;
    uint8_t frame[bleprotocol::MaxAttPayload];

    for (uint8_t index = 0; index < chunkCount; ++index) {
        const size_t offset = static_cast<size_t>(index) * chunkPayload;
        const size_t bytes = std::min(chunkPayload, length - offset);
        const bleprotocol::FrameHeader header{
            bleprotocol::Version,
            messageType,
            messageId,
            static_cast<uint16_t>(length),
            index,
            chunkCount,
        };
        memcpy(frame, &header, sizeof(header));
        memcpy(frame + sizeof(header), payload + offset, bytes);
        const size_t frameLength = sizeof(header) + bytes;
        characteristic->setValue(frame, frameLength);
        if (!characteristic->notify(frame, frameLength)) return false;
        if (index + 1 < chunkCount) vTaskDelay(1);
    }
    return true;
}

}
