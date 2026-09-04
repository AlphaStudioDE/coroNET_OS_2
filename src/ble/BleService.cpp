#include "BleService.h"

#include <algorithm>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

#include "BleProtocol.h"
#include "../companion/PairingService.h"
#include "../audio/AudioService.h"
#include "../config/AppConfig.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemHealth.h"
#include "../core/SystemState.h"
#include "../led/LedService.h"
#include "../printer/PrinterService.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

BleService* gActiveService = nullptr;
NimBLECharacteristic* gStateChr = nullptr;
NimBLECharacteristic* gCommandChr = nullptr;
NimBLECharacteristic* gEventChr = nullptr;
BleService gBleService;

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

BleService& bleService() {
    return gBleService;
}

void BleService::begin() {
    logHeapDiagnostics("ble-before-queue");
    if (!commandQueue_) {
        commandQueue_ = xQueueCreate(CommandQueueDepth, sizeof(QueuedCommand));
        if (!commandQueue_) {
            Serial.println("[ble] command queue allocation failed");
            state().bleReady = false;
            return;
        }
    }
    logHeapDiagnostics("ble-after-queue");

    gActiveService = this;
    strlcpy(deviceId_, deviceIdentity().id(), sizeof(deviceId_));
    appliedSettingsRevision_ = settingsService().revision();
    applySettings();
}

void BleService::loop() {
    applySettings();
    if (!started_) return;

    const SystemState& system = state();
    if (observedPrinterTelemetryRevision_ != system.printerTelemetryRevision ||
        observedPrinterConnectionRevision_ != system.printerConnectionRevision ||
        observedPrinterEventSequence_ != system.printerStateEventSequence) {
        observedPrinterTelemetryRevision_ = system.printerTelemetryRevision;
        observedPrinterConnectionRevision_ = system.printerConnectionRevision;
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        stateDirty_ = true;
    }

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
        sessionAuthenticated_ = false;
        publishEvent("ble", fallbackActive_ ? "connected_fallback" : "connected");
        publishState(true);
    }
    if (disconnectedEvent) {
        state().bleConnected = false;
        stateDirty_ = true;
        sessionAuthenticated_ = false;
    }
    if (overflowEvent) publishEvent("error", "command_queue_full");

    QueuedCommand queued{};
    uint8_t handled = 0;
    while (handled < CommandQueueDepth && xQueueReceive(commandQueue_, &queued, 0) == pdTRUE) {
        handleCommand(queued.data, queued.length);
        handled++;
    }

    const PairingSnapshot pairing = pairingService().snapshot();
    if (pairing.revision != observedPairingRevision_) {
        observedPairingRevision_ = pairing.revision;
        if (pairing.sessionId != 0 && pairing.sessionId != observedPairingSessionId_) {
            observedPairingSessionId_ = pairing.sessionId;
            sessionAuthenticated_ = false;
            publishPairingChallenge();
        } else if (pairing.phase == PairingPhase::Cancelled) {
            publishEvent("ack", "pairing_cancelled");
        } else if (pairing.phase == PairingPhase::Expired) {
            publishEvent("error", "pairing_expired");
        }
    }
    if ((pairing.phase == PairingPhase::ReadyToDeliver ||
         pairing.phase == PairingPhase::AwaitingReceipt) &&
        millis() - lastPairingPublishMs_ >= 1000U) {
        publishPairingResult();
    }

    publishState(false);
}

void BleService::startStack() {
    if (started_) return;

    portENTER_CRITICAL(&connectionMux_);
    connectionEventPending_ = false;
    disconnectEventPending_ = false;
    portEXIT_CRITICAL(&connectionMux_);

    logHeapDiagnostics("ble-before-init");
    refreshAdvertisedName();
    NimBLEDevice::init(advertisedName_);
    logHeapDiagnostics("ble-after-init");
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    NimBLEDevice::setMTU(247);

    NimBLEServer* server = NimBLEDevice::createServer();
    if (!server) {
        NimBLEDevice::deinit(true);
        return;
    }
    logHeapDiagnostics("ble-after-server");
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
    logHeapDiagnostics("ble-after-chars");

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
    logHeapDiagnostics("ble-after-advert");
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

    const bool radioAllowed = !state().maintenanceMode && !state().otaTlsWindowActive;
    bool shouldStart = radioAllowed && (!cfg.apiPaired ||
                       (cfg.bleEnabled && cfg.companionTransport != CompanionTransport::Wifi) ||
                       fallbackActive_);
    if (radioAllowed && !shouldStart && isConnected()) shouldStart = true;

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

    if (strcmp(cmd, "authenticate") == 0) {
        const char* token = doc["token"].as<const char*>();
        const AppSettings& cfg = settingsService().settings();
        uint8_t difference = 0;
        const bool validLength = token && strlen(token) == 32 && strlen(cfg.apiToken) == 32;
        if (validLength) {
            for (size_t index = 0; index < 32; ++index) {
                difference |= static_cast<uint8_t>(token[index] ^ cfg.apiToken[index]);
            }
        }
        sessionAuthenticated_ = cfg.apiPaired && validLength && difference == 0;
        publishEvent(sessionAuthenticated_ ? "ack" : "error",
                     sessionAuthenticated_ ? "authenticated" : "authentication_failed");
        return;
    }
    if (strcmp(cmd, "getPairingChallenge") == 0 || strcmp(cmd, "getPairingToken") == 0) {
        publishPairingChallenge();
        return;
    }
    if (strcmp(cmd, "confirmPairingCode") == 0) {
        const uint32_t sessionId = doc["session"] | 0U;
        const uint32_t code = doc["code"] | 0U;
        if (!pairingService().confirmFromPhone(sessionId, code)) {
            publishEvent("error", "pairing_code_rejected");
            return;
        }
        publishEvent("ack", "phone_code_confirmed");
        publishPairingResult();
        return;
    }
    if (strcmp(cmd, "completePairing") == 0) {
        const uint32_t sessionId = doc["session"] | 0U;
        if (!pairingService().completeFromPhone(sessionId)) {
            publishEvent("error", "pairing_receipt_rejected");
            return;
        }
        sessionAuthenticated_ = true;
        publishSettings();
        publishEvent("ack", "pairing_confirmed");
        return;
    }
    if (strcmp(cmd, "cancelPairing") == 0) {
        const uint32_t sessionId = doc["session"] | 0U;
        if (pairingService().cancel(sessionId)) publishEvent("ack", "pairing_cancelled");
        else publishEvent("error", "pairing_cancel_rejected");
        return;
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
    const AppSettings& pairingCfg = settingsService().settings();
    if (pairingCfg.apiPaired && !sessionAuthenticated_) {
        publishEvent("error", "authentication_required");
        return;
    }
    if (!pairingCfg.apiPaired) {
        publishEvent("error", "pairing_required");
        return;
    }

    if (strcmp(cmd, "getSettings") == 0) {
        publishSettings();
        publishEvent("ack", "settings");
        return;
    }
    if (strcmp(cmd, "getSoundLibrary") == 0) {
        publishSoundLibrary(static_cast<uint8_t>(constrain(doc["folder"] | 0, 0, 255)),
                            static_cast<uint8_t>(constrain(doc["page"] | 0, 0, 255)));
        return;
    }
    if (strcmp(cmd, "playSound") == 0) {
        const int scenario = doc["scenario"] | -1;
        if (scenario < 0 || scenario >= static_cast<int>(SoundScenario::Count)) {
            publishEvent("error", "audio_scenario_invalid");
            return;
        }
        const bool started = audioService().playScenario(static_cast<SoundScenario>(scenario));
        publishEvent(started ? "ack" : "error", started ? "audio_started" : "audio_unavailable");
        return;
    }
    if (strcmp(cmd, "stopSound") == 0) {
        audioService().stop();
        publishEvent("ack", "audio_stopped");
        return;
    }
    if (strcmp(cmd, "rescanSounds") == 0) {
        const bool queued = audioService().requestStorageRefresh();
        publishEvent(queued ? "ack" : "error", queued ? "audio_rescan_queued" : "audio_busy");
        return;
    }
    if (strcmp(cmd, "previewLed") == 0) {
        const int category = doc["category"] | -1;
        const int animation = doc["animation"] | -1;
        const uint32_t durationMs = constrain(doc["durationMs"] | 10000U, 1000U, 30000U);
        if (category < 0 || category >= static_cast<int>(LedCategory::Count) || animation < 0 ||
            animation >= ledAnimationCount(static_cast<LedCategory>(category))) {
            publishEvent("error", "led_preview_invalid");
            return;
        }
        const bool started = ledService().requestPreview(static_cast<LedCategory>(category), static_cast<uint8_t>(animation), durationMs);
        publishEvent(started ? "ack" : "error", started ? "led_preview_started" : "led_unavailable");
        return;
    }
    if (strcmp(cmd, "calibrateLed") == 0) {
        const bool active = doc["active"] | false;
        if (!active) {
            ledService().stopColorCalibration();
            publishEvent("ack", "led_calibration_stopped");
            return;
        }
        const int color = doc["color"] | -1;
        if (color < 0 || color >= static_cast<int>(LedCalibrationColor::Count)) {
            publishEvent("error", "led_calibration_invalid");
            return;
        }
        const bool started = ledService().startColorCalibration(static_cast<LedCalibrationColor>(color));
        publishEvent(started ? "ack" : "error", started ? "led_calibration_started" : "led_unavailable");
        return;
    }
    if (strcmp(cmd, "setWifi") == 0) {
        AppSettings cfg = settingsService().snapshot();
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
        settingsService().replace(cfg);
        settingsService().save();
        publishSettings();
        publishEvent("ack", "wifi_saved");
        return;
    }

    if (strcmp(cmd, "setPrinter") == 0) {
        AppSettings cfg = settingsService().snapshot();
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
        settingsService().replace(cfg);
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
        AppSettings cfg = settingsService().snapshot();
        cfg.setupDone = doc["done"].as<bool>();
        settingsService().replace(cfg);
        state().setupDone = cfg.setupDone;
        settingsService().save();
        publishSettings();
        publishState(true);
        publishEvent("ack", cfg.setupDone ? "setup_done" : "setup_open");
        return;
    }

    if (strcmp(cmd, "resetDeviceName") == 0) {
        AppSettings cfg = settingsService().snapshot();
        cfg.deviceName[0] = '\0';
        settingsService().replace(cfg);
        settingsService().save();
        updateAdvertisingName();
        publishState(true);
        publishEvent("ack", "device_name_reset");
        return;
    }

    if (strcmp(cmd, "setDeviceName") == 0 || strcmp(cmd, "setName") == 0) {
        AppSettings cfg = settingsService().snapshot();
        char requestedName[sizeof(cfg.deviceName)] = "";
        char cleanName[sizeof(cfg.deviceName)] = "";
        if (!readBoundedString(doc["name"], requestedName, sizeof(requestedName), true)) {
            publishEvent("error", "device_name_invalid");
            return;
        }
        deviceIdentity().sanitizeName(requestedName, cleanName, sizeof(cleanName));
        strlcpy(cfg.deviceName, cleanName, sizeof(cfg.deviceName));
        settingsService().replace(cfg);
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

        AppSettings cfg = settingsService().snapshot();
        if (strcasecmp(mode, "auto") == 0) cfg.companionTransport = CompanionTransport::Auto;
        else if (strcasecmp(mode, "ble") == 0 || strcasecmp(mode, "bt") == 0) cfg.companionTransport = CompanionTransport::Ble;
        else if (strcasecmp(mode, "wifi") == 0) cfg.companionTransport = CompanionTransport::Wifi;
        else {
            publishEvent("error", "transport_mode_invalid");
            return;
        }
        settingsService().replace(cfg);
        settingsService().save();
        publishSettings();
        publishEvent("ack", "transport_saved");
        return;
    }

    if (strcmp(cmd, "setSettings") == 0) {
        AppSettings cfg = settingsService().snapshot();
        if (doc["displayBrightness"].is<int>()) cfg.displayBrightness = constrain(doc["displayBrightness"].as<int>(), 10, 100);
        if (doc["uiSkin"].is<int>()) cfg.uiSkin = static_cast<UiSkin>(constrain(doc["uiSkin"].as<int>(), 0, 3));
        if (doc["uiColorMode"].is<int>()) cfg.uiColorMode = static_cast<UiColorMode>(constrain(doc["uiColorMode"].as<int>(), 0, 2));
        if (doc["accentHueDegrees"].is<int>()) cfg.accentHueDegrees = constrain(doc["accentHueDegrees"].as<int>(), 0, 359);
        if (doc["screenSaverMode"].is<int>()) cfg.screenSaverMode = static_cast<ScreenSaverMode>(constrain(doc["screenSaverMode"].as<int>(), 0, 2));
        if (doc["screenSaverDelayMinutes"].is<int>()) cfg.screenSaverDelayMinutes = constrain(doc["screenSaverDelayMinutes"].as<int>(), 1, 60);
        if (doc["clockBrightness"].is<int>()) cfg.clockBrightness = constrain(doc["clockBrightness"].as<int>(), 5, 100);
        if (doc["clockStyle"].is<int>()) cfg.clockStyle = static_cast<ClockStyle>(constrain(doc["clockStyle"].as<int>(), 0, static_cast<int>(ClockStyle::Count) - 1));
        if (doc["clock24Hour"].is<bool>()) cfg.clock24Hour = doc["clock24Hour"].as<bool>();
        if (doc["timeZone"].is<const char*>()) {
            char timeZone[sizeof(cfg.timeZone)] = "";
            if (!readBoundedString(doc["timeZone"], timeZone, sizeof(timeZone), true) || !timeZone[0]) {
                publishEvent("error", "time_zone_invalid");
                return;
            }
            strlcpy(cfg.timeZone, timeZone, sizeof(cfg.timeZone));
        }
        if (doc["quietTarget"].is<int>()) cfg.quietTarget = static_cast<QuietTarget>(constrain(doc["quietTarget"].as<int>(), 0, 3));
        if (doc["quietDurationMinutes"].is<int>()) cfg.quietDurationMinutes = constrain(doc["quietDurationMinutes"].as<int>(), 1, 1440);
        if (doc["quietErrorsBypass"].is<bool>()) cfg.quietErrorsBypass = doc["quietErrorsBypass"].as<bool>();
        if (doc["ledEnabled"].is<bool>()) cfg.ledEnabled = doc["ledEnabled"].as<bool>();
        if (doc["ledOtherMode"].is<bool>()) cfg.ledOtherMode = doc["ledOtherMode"].as<bool>();
        if (doc["insideColorStyle"].is<int>()) cfg.insideColorStyle = static_cast<InsideColorStyle>(constrain(doc["insideColorStyle"].as<int>(), 0, 1));
        if (doc["mirrorLedLayout"].is<bool>()) cfg.mirrorLedLayout = doc["mirrorLedLayout"].as<bool>();
        if (doc["ledBrightness"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledBrightness"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedSection{}) && i < values.size(); ++i) cfg.ledBrightness[i] = constrain(values[i].as<int>(), 0, 100);
        }
        if (doc["ledDimmEnabled"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledDimmEnabled"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedSection{}) && i < values.size(); ++i) cfg.ledDimmEnabled[i] = values[i].as<bool>();
        }
        if (doc["ledDimmPercent"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledDimmPercent"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedSection{}) && i < values.size(); ++i) cfg.ledDimmPercent[i] = constrain(values[i].as<int>(), 0, 100);
        }
        if (doc["ledAnimation"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledAnimation"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedCategory{}) && i < values.size(); ++i) {
                const int animation = values[i].as<int>();
                if (animation < 0 || animation >= ledAnimationCount(static_cast<LedCategory>(i))) {
                    publishEvent("error", "led_animation_invalid");
                    return;
                }
                cfg.ledAnimation[i] = static_cast<uint8_t>(animation);
            }
        }
        if (doc["ledColorRemixDegrees"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledColorRemixDegrees"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(LedCategory{}) && i < values.size(); ++i) cfg.ledColorRemixDegrees[i] = constrain(values[i].as<int>(), -180, 180);
        }
        if (doc["ledCalibrationHue"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledCalibrationHue"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < 8 && i < values.size(); ++i) cfg.ledCalibrationHue[i] = constrain(values[i].as<int>(), -45, 45);
        }
        if (doc["ledCalibrationSaturation"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledCalibrationSaturation"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < 8 && i < values.size(); ++i) cfg.ledCalibrationSaturation[i] = constrain(values[i].as<int>(), 50, 150);
        }
        if (doc["ledCalibrationBrightness"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["ledCalibrationBrightness"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < 8 && i < values.size(); ++i) cfg.ledCalibrationBrightness[i] = constrain(values[i].as<int>(), 50, 150);
        }
        if (doc["soundVolume"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["soundVolume"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) cfg.soundVolume[i] = constrain(values[i].as<int>(), 0, 100);
        }
        if (doc["soundRepeat"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["soundRepeat"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) cfg.soundRepeat[i] = values[i].as<bool>();
        }
        if (doc["soundPath"].is<JsonArrayConst>()) {
            JsonArrayConst values = doc["soundPath"].as<JsonArrayConst>();
            for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) {
                if (values[i].is<const char*>()) readBoundedString(values[i], cfg.soundPath[i], sizeof(cfg.soundPath[i]), true);
            }
        }
        if (doc["ventMode"].is<int>()) cfg.ventMode = static_cast<VentMode>(constrain(doc["ventMode"].as<int>(), 0, 2));
        if (doc["ventTargetTempC"].is<int>()) cfg.ventTargetTempC = constrain(doc["ventTargetTempC"].as<int>(), 20, 80);
        if (doc["manualFanPercent"].is<int>()) cfg.manualFanPercent = constrain(doc["manualFanPercent"].as<int>(), 0, 100);
        if (doc["manualFlapPercent"].is<int>()) cfg.manualFlapPercent = constrain(doc["manualFlapPercent"].as<int>(), 0, 100);
        const bool hasFanMin = doc["fanMinPercent"].is<int>();
        const bool hasFanMax = doc["fanMaxPercent"].is<int>();
        const uint8_t requestedFanMin = hasFanMin
                                            ? static_cast<uint8_t>(constrain(doc["fanMinPercent"].as<int>(), 0, 100))
                                            : cfg.fanMinPercent;
        const uint8_t requestedFanMax = hasFanMax
                                            ? static_cast<uint8_t>(constrain(doc["fanMaxPercent"].as<int>(), 0, 100))
                                            : cfg.fanMaxPercent;
        if (hasFanMin && hasFanMax && requestedFanMin > requestedFanMax) {
            publishEvent("error", "fan_range_invalid");
            return;
        }
        if (hasFanMin) cfg.fanMinPercent = min(requestedFanMin, requestedFanMax);
        if (hasFanMax) cfg.fanMaxPercent = max(requestedFanMax, cfg.fanMinPercent);
        if (doc["failsafeFanPercent"].is<int>()) cfg.failsafeFanPercent = constrain(doc["failsafeFanPercent"].as<int>(), 0, 100);
        if (doc["failsafeFlapPercent"].is<int>()) cfg.failsafeFlapPercent = constrain(doc["failsafeFlapPercent"].as<int>(), 0, 100);
        if (doc["servoClosedUs"].is<int>()) cfg.servoClosedUs = constrain(doc["servoClosedUs"].as<int>(), 500, 2500);
        if (doc["servoOpenUs"].is<int>()) cfg.servoOpenUs = constrain(doc["servoOpenUs"].as<int>(), 500, 2500);
        if (doc["servoReverse"].is<bool>()) cfg.servoReverse = doc["servoReverse"].as<bool>();
        if (doc["diyHeaterOutputHigh"].is<bool>()) cfg.diyHeaterOutputHigh = doc["diyHeaterOutputHigh"].as<bool>();
        if (doc["pandaEnabled"].is<bool>()) cfg.pandaEnabled = doc["pandaEnabled"].as<bool>();
        if (doc["pandaHost"].is<const char*>()) {
            readBoundedString(doc["pandaHost"], cfg.pandaHost, sizeof(cfg.pandaHost), false);
        }
        if (doc["pandaMode"].is<int>()) cfg.pandaMode = static_cast<PandaBreathMode>(constrain(doc["pandaMode"].as<int>(), 0, static_cast<int>(PandaBreathMode::Count) - 1));
        if (doc["pandaTargetTempC"].is<int>()) cfg.pandaTargetTempC = constrain(doc["pandaTargetTempC"].as<int>(), 30, 60);
        if (doc["pandaPrintTargetTempC"].is<int>()) cfg.pandaPrintTargetTempC = constrain(doc["pandaPrintTargetTempC"].as<int>(), 30, 60);
        if (doc["pandaDryPreset"].is<int>()) cfg.pandaDryPreset = static_cast<PandaDryPreset>(constrain(doc["pandaDryPreset"].as<int>(), 0, static_cast<int>(PandaDryPreset::Count) - 1));
        if (doc["pandaDryHours"].is<int>()) cfg.pandaDryHours = constrain(doc["pandaDryHours"].as<int>(), 1, 24);
        if (doc["pandaPreheatHoldMinutes"].is<int>()) cfg.pandaPreheatHoldMinutes = constrain(doc["pandaPreheatHoldMinutes"].as<int>(), 1, 180);
        if (doc["pandaTemperingDurationMinutes"].is<int>()) cfg.pandaTemperingDurationMinutes = constrain(doc["pandaTemperingDurationMinutes"].as<int>(), 1, 180);
        if (doc["pandaTemperingEndTempC"].is<int>()) cfg.pandaTemperingEndTempC = constrain(doc["pandaTemperingEndTempC"].as<int>(), 0, 60);
        if (doc["pandaTemperingAfterPrint"].is<bool>()) cfg.pandaTemperingAfterPrint = doc["pandaTemperingAfterPrint"].as<bool>();
        settingsService().replace(cfg);
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

void BleService::publishSoundLibrary(uint8_t requestedFolder, uint8_t requestedPage) {
    if (!gEventChr || !isConnected()) return;
    constexpr uint8_t PageSize = 8;
    const uint8_t folderCount = audioService().folderCount();
    const uint8_t folder = folderCount == 0U ? 0U : min<uint8_t>(requestedFolder, folderCount - 1U);
    const uint8_t fileCount = folderCount == 0U ? 0U : audioService().folderFileCount(folder);
    const uint8_t pageCount = max<uint8_t>(1U, static_cast<uint8_t>((fileCount + PageSize - 1U) / PageSize));
    const uint8_t page = min<uint8_t>(requestedPage, pageCount - 1U);
    const uint8_t first = static_cast<uint8_t>(page * PageSize);

    JsonDocument doc;
    doc["v"] = bleprotocol::Version;
    doc["t"] = "sound_library";
    doc["sdReady"] = state().sdReady;
    doc["folder"] = folder;
    doc["folderCount"] = folderCount;
    doc["folderName"] = folderCount ? audioService().folderName(folder) : "";
    doc["page"] = page;
    doc["pageCount"] = pageCount;
    doc["fileCount"] = fileCount;
    JsonArray files = doc["files"].to<JsonArray>();
    for (uint8_t index = first; index < fileCount && index < first + PageSize; ++index) {
        const char* path = audioService().folderFilePath(folder, index);
        if (!path) continue;
        JsonObject item = files.add<JsonObject>();
        item["path"] = path;
        const char* slash = strrchr(path, '/');
        item["name"] = slash ? slash + 1 : path;
    }

    String payload;
    serializeJson(doc, payload);
    if (payload.length() > 4096U ||
        !sendFramed(gEventChr,
                    static_cast<uint8_t>(bleprotocol::MessageType::EventJson),
                    reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length())) {
        publishEvent("error", "audio_library_too_large");
    }
}

void BleService::publishSettings() {
    if (!gEventChr || !isConnected()) return;

    const AppSettings& cfg = settingsService().settings();
    const unsigned long settingsRevision = static_cast<unsigned long>(settingsService().revision());
    char safeName[64];
    char safeSsid[80];
    char safePrinterHost[80];
    char safeTimeZone[96];
    jsonStringCopy(advertisedName_, safeName, sizeof(safeName));
    jsonStringCopy(cfg.wifiSsid, safeSsid, sizeof(safeSsid));
    jsonStringCopy(cfg.printerHost, safePrinterHost, sizeof(safePrinterHost));
    jsonStringCopy(cfg.timeZone, safeTimeZone, sizeof(safeTimeZone));

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
                                 "{\"v\":%u,\"t\":\"settings\",\"r\":%lu,\"sr\":%lu,\"id\":\"%s\",\"name\":\"%s\","
                                 "\"setupDone\":%u,\"bleEnabled\":%u,\"apiPaired\":%u,\"transport\":%u,\"brightness\":%u,"
                                 "\"uiSkin\":%u,\"uiColor\":%u,\"wifiSsid\":\"%s\",\"wifiPasswordSet\":%u,"
                                 "\"printerHost\":\"%s\",\"printerPort\":%u,\"printerApiKeySet\":%u}",
                                 bleprotocol::Version,
                                 static_cast<unsigned long>(++revision_),
                                 settingsRevision,
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

    char safePandaHost[132];
    jsonStringCopy(cfg.pandaHost, safePandaHost, sizeof(safePandaHost));
    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"appearance\",\"sr\":%lu,\"displayBrightness\":%u,"
                       "\"uiSkin\":%u,\"uiColorMode\":%u,\"accentHueDegrees\":%u,\"screenSaverMode\":%u,"
                       "\"screenSaverDelayMinutes\":%u,\"clockBrightness\":%u,\"clockStyle\":%u,"
                       "\"clock24Hour\":%s,\"timeZone\":\"%s\","
                       "\"quietTarget\":%u,\"quietDurationMinutes\":%u,\"quietErrorsBypass\":%s}",
                       bleprotocol::Version, settingsRevision, cfg.displayBrightness, static_cast<unsigned>(cfg.uiSkin),
                       static_cast<unsigned>(cfg.uiColorMode), cfg.accentHueDegrees,
                       static_cast<unsigned>(cfg.screenSaverMode), cfg.screenSaverDelayMinutes,
                       cfg.clockBrightness, static_cast<unsigned>(cfg.clockStyle),
                       cfg.clock24Hour ? "true" : "false", safeTimeZone,
                       static_cast<unsigned>(cfg.quietTarget), cfg.quietDurationMinutes,
                       cfg.quietErrorsBypass ? "true" : "false");
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"led\",\"sr\":%lu,\"ledEnabled\":%s,"
                       "\"ledOtherMode\":%s,\"ledBrightness\":[%u,%u,%u,%u],"
                       "\"ledDimmEnabled\":[%s,%s,%s,%s],\"ledDimmPercent\":[%u,%u,%u,%u],"
                       "\"insideColorStyle\":%u,\"mirrorLedLayout\":%s,"
                       "\"ledAnimation\":[%u,%u,%u,%u,%u,%u],"
                       "\"ledColorRemixDegrees\":[%d,%d,%d,%d,%d,%d]}",
                       bleprotocol::Version, settingsRevision, cfg.ledEnabled ? "true" : "false", cfg.ledOtherMode ? "true" : "false",
                       cfg.ledBrightness[0], cfg.ledBrightness[1], cfg.ledBrightness[2], cfg.ledBrightness[3],
                       cfg.ledDimmEnabled[0] ? "true" : "false", cfg.ledDimmEnabled[1] ? "true" : "false",
                       cfg.ledDimmEnabled[2] ? "true" : "false", cfg.ledDimmEnabled[3] ? "true" : "false",
                       cfg.ledDimmPercent[0], cfg.ledDimmPercent[1], cfg.ledDimmPercent[2], cfg.ledDimmPercent[3],
                       static_cast<unsigned>(cfg.insideColorStyle), cfg.mirrorLedLayout ? "true" : "false",
                       cfg.ledAnimation[0], cfg.ledAnimation[1], cfg.ledAnimation[2], cfg.ledAnimation[3], cfg.ledAnimation[4], cfg.ledAnimation[5],
                       cfg.ledColorRemixDegrees[0], cfg.ledColorRemixDegrees[1], cfg.ledColorRemixDegrees[2],
                       cfg.ledColorRemixDegrees[3], cfg.ledColorRemixDegrees[4], cfg.ledColorRemixDegrees[5]);
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"led_calibration\",\"sr\":%lu,"
                       "\"ledCalibrationHue\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                       "\"ledCalibrationSaturation\":[%u,%u,%u,%u,%u,%u,%u,%u],"
                       "\"ledCalibrationBrightness\":[%u,%u,%u,%u,%u,%u,%u,%u]}",
                       bleprotocol::Version, settingsRevision,
                       cfg.ledCalibrationHue[0], cfg.ledCalibrationHue[1], cfg.ledCalibrationHue[2], cfg.ledCalibrationHue[3],
                       cfg.ledCalibrationHue[4], cfg.ledCalibrationHue[5], cfg.ledCalibrationHue[6], cfg.ledCalibrationHue[7],
                       cfg.ledCalibrationSaturation[0], cfg.ledCalibrationSaturation[1], cfg.ledCalibrationSaturation[2], cfg.ledCalibrationSaturation[3],
                       cfg.ledCalibrationSaturation[4], cfg.ledCalibrationSaturation[5], cfg.ledCalibrationSaturation[6], cfg.ledCalibrationSaturation[7],
                       cfg.ledCalibrationBrightness[0], cfg.ledCalibrationBrightness[1], cfg.ledCalibrationBrightness[2], cfg.ledCalibrationBrightness[3],
                       cfg.ledCalibrationBrightness[4], cfg.ledCalibrationBrightness[5], cfg.ledCalibrationBrightness[6], cfg.ledCalibrationBrightness[7]);
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"sound\",\"sr\":%lu,"
                       "\"soundVolume\":[%u,%u,%u,%u,%u],\"soundRepeat\":[%s,%s,%s,%s,%s]}",
                       bleprotocol::Version, settingsRevision,
                       cfg.soundVolume[0], cfg.soundVolume[1], cfg.soundVolume[2], cfg.soundVolume[3], cfg.soundVolume[4],
                       cfg.soundRepeat[0] ? "true" : "false", cfg.soundRepeat[1] ? "true" : "false",
                       cfg.soundRepeat[2] ? "true" : "false", cfg.soundRepeat[3] ? "true" : "false",
                       cfg.soundRepeat[4] ? "true" : "false");
    if (!sendPayload(written)) return;

    for (uint8_t i = 0; i < enumCount(SoundScenario{}); ++i) {
        char safePath[132];
        jsonStringCopy(cfg.soundPath[i], safePath, sizeof(safePath));
        written = snprintf(payload, sizeof(payload),
                           "{\"v\":%u,\"t\":\"settings\",\"group\":\"sound_path\",\"sr\":%lu,"
                           "\"soundPathIndex\":%u,\"soundPathValue\":\"%s\"}",
                           bleprotocol::Version, settingsRevision, i, safePath);
        if (!sendPayload(written)) return;
    }

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"vent\",\"sr\":%lu,\"ventMode\":%u,"
                       "\"ventTargetTempC\":%u,\"manualFanPercent\":%u,\"manualFlapPercent\":%u,"
                       "\"fanMinPercent\":%u,\"fanMaxPercent\":%u,\"failsafeFanPercent\":%u,\"failsafeFlapPercent\":%u,"
                       "\"servoClosedUs\":%u,\"servoOpenUs\":%u,\"servoReverse\":%s,\"diyHeaterOutputHigh\":%s}",
                       bleprotocol::Version, settingsRevision, static_cast<unsigned>(cfg.ventMode), cfg.ventTargetTempC,
                       cfg.manualFanPercent, cfg.manualFlapPercent, cfg.fanMinPercent, cfg.fanMaxPercent,
                       cfg.failsafeFanPercent, cfg.failsafeFlapPercent, cfg.servoClosedUs, cfg.servoOpenUs,
                       cfg.servoReverse ? "true" : "false", cfg.diyHeaterOutputHigh ? "true" : "false");
    if (!sendPayload(written)) return;

    written = snprintf(payload, sizeof(payload),
                       "{\"v\":%u,\"t\":\"settings\",\"group\":\"panda\",\"sr\":%lu,\"pandaEnabled\":%s,"
                       "\"pandaHost\":\"%s\",\"pandaMode\":%u,\"pandaTargetTempC\":%u,\"pandaPrintTargetTempC\":%u,"
                       "\"pandaDryPreset\":%u,\"pandaDryHours\":%u,\"pandaPreheatHoldMinutes\":%u,"
                       "\"pandaTemperingDurationMinutes\":%u,\"pandaTemperingEndTempC\":%u,\"pandaTemperingAfterPrint\":%s}",
                       bleprotocol::Version, settingsRevision, cfg.pandaEnabled ? "true" : "false", safePandaHost,
                       static_cast<unsigned>(cfg.pandaMode), cfg.pandaTargetTempC, cfg.pandaPrintTargetTempC,
                       static_cast<unsigned>(cfg.pandaDryPreset), cfg.pandaDryHours, cfg.pandaPreheatHoldMinutes,
                       cfg.pandaTemperingDurationMinutes, cfg.pandaTemperingEndTempC,
                       cfg.pandaTemperingAfterPrint ? "true" : "false");
    sendPayload(written);
}

void BleService::publishPairingChallenge() {
    if (!gEventChr || !isConnected()) return;

    const PairingSnapshot pairing = pairingService().snapshot();
    if (pairing.phase == PairingPhase::Inactive || pairing.phase == PairingPhase::Completed ||
        pairing.phase == PairingPhase::Cancelled || pairing.phase == PairingPhase::Expired) {
        publishEvent("error", "pairing_closed");
        return;
    }
    char safeName[64];
    jsonStringCopy(advertisedName_, safeName, sizeof(safeName));
    const uint32_t remainingMs = static_cast<int32_t>(pairing.expiresAtMs - millis()) > 0
                                     ? pairing.expiresAtMs - millis() : 0;
    char payload[224];
    const int written = snprintf(payload, sizeof(payload),
                                 "{\"v\":%u,\"t\":\"pairing_challenge\",\"id\":\"%s\",\"name\":\"%s\","
                                 "\"session\":%lu,\"code\":%06lu,\"expiresMs\":%lu}",
                                 bleprotocol::Version, deviceId_, safeName,
                                 static_cast<unsigned long>(pairing.sessionId),
                                 static_cast<unsigned long>(pairing.code),
                                 static_cast<unsigned long>(remainingMs));
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) return;

    sendFramed(gEventChr,
               static_cast<uint8_t>(bleprotocol::MessageType::PairingJson),
               reinterpret_cast<const uint8_t*>(payload),
               static_cast<size_t>(written));
}

void BleService::publishPairingResult() {
    if (!gEventChr || !isConnected()) return;
    const PairingSnapshot pairing = pairingService().snapshot();
    if (pairing.phase != PairingPhase::ReadyToDeliver &&
        pairing.phase != PairingPhase::AwaitingReceipt) return;

    const AppSettings& cfg = settingsService().settings();
    char localIp[16] = "";
    if (WiFi.status() == WL_CONNECTED) {
        const IPAddress ip = WiFi.localIP();
        snprintf(localIp, sizeof(localIp), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    char payload[224];
    const int written = snprintf(payload, sizeof(payload),
                                 "{\"v\":%u,\"t\":\"pairing_result\",\"id\":\"%s\",\"session\":%lu,"
                                 "\"token\":\"%s\",\"ip\":\"%s\"}",
                                 bleprotocol::Version, deviceId_,
                                 static_cast<unsigned long>(pairing.sessionId), cfg.apiToken, localIp);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) return;

    lastPairingPublishMs_ = millis();
    if (sendFramed(gEventChr,
                   static_cast<uint8_t>(bleprotocol::MessageType::PairingJson),
                   reinterpret_cast<const uint8_t*>(payload),
                   static_cast<size_t>(written))) {
        pairingService().markResultSent(pairing.sessionId);
    }
}

void BleService::publishState(bool force) {
    if (!gStateChr || !isConnected()) return;
    const uint32_t now = millis();
    if (!force && !stateDirty_ && now - lastNotifyMs_ < config::BleStateNotifyIntervalMs) return;

    const SystemState& s = state();
    bleprotocol::StateSnapshotV2 snapshot{};
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
    if (s.printerTelemetryValid) snapshot.flags |= bleprotocol::PrinterTelemetryValid;
    snapshot.printerState = static_cast<uint8_t>(s.printerState);
    snapshot.printProgress = s.printProgress;
    snapshot.activeTool = s.activeTool;
    if (s.audioPlaying) snapshot.runtimeFlags |= bleprotocol::AudioPlaying;
    if (s.quietActive) snapshot.runtimeFlags |= bleprotocol::QuietActive;
    snapshot.activeToolTempTenths = tempToTenths(s.activeToolTempC);
    snapshot.bedTempTenths = tempToTenths(s.bedTempC);
    snapshot.chamberTempTenths = tempToTenths(s.chamberTempC);
    strlcpy(snapshot.deviceId, deviceId_, sizeof(snapshot.deviceId));
    strlcpy(snapshot.deviceName, advertisedName_, sizeof(snapshot.deviceName));
    strlcpy(snapshot.printerStatus, s.printerStatusText, sizeof(snapshot.printerStatus));
    strlcpy(snapshot.printFilename, s.printFilename, sizeof(snapshot.printFilename));
    snapshot.printerTelemetryRevision = s.printerTelemetryRevision;
    snapshot.printerStateEventSequence = s.printerStateEventSequence;
    snapshot.printerEventFrom = static_cast<uint8_t>(s.printerEventFrom);
    snapshot.printerEventTo = static_cast<uint8_t>(s.printerEventTo);
    snapshot.fanPercent = s.fanPercent;
    snapshot.flapPercent = s.flapPercent;

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
