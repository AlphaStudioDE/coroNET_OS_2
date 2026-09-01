#include "WebControlService.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "../config/AppConfig.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemState.h"
#include "../printer/PrinterService.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

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

const char* transportName(CompanionTransport transport) {
    switch (transport) {
        case CompanionTransport::Auto: return "auto";
        case CompanionTransport::Ble: return "ble";
        case CompanionTransport::Wifi: return "wifi";
        default: return "auto";
    }
}

bool parseTransport(JsonVariantConst value, CompanionTransport& out) {
    if (value.isNull()) return false;

    if (value.is<uint8_t>() || value.is<int>()) {
        const int raw = value.as<int>();
        if (raw < static_cast<int>(CompanionTransport::Auto) ||
            raw > static_cast<int>(CompanionTransport::Wifi)) {
            return false;
        }
        out = static_cast<CompanionTransport>(raw);
        return true;
    }

    const char* text = value.as<const char*>();
    if (!text) return false;
    String mode(text);
    mode.trim();
    mode.toLowerCase();
    if (mode == "auto") out = CompanionTransport::Auto;
    else if (mode == "ble" || mode == "bt") out = CompanionTransport::Ble;
    else if (mode == "wifi") out = CompanionTransport::Wifi;
    else return false;
    return true;
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

void addCommonState(JsonDocument& doc) {
    const SystemState& s = state();
    const AppSettings& cfg = settingsService().settings();

    char name[25];
    deviceIdentity().effectiveName(cfg.deviceName, name, sizeof(name));

    doc["id"] = deviceIdentity().id();
    doc["name"] = name;
    doc["hostname"] = deviceIdentity().hostname();
    doc["firmware"] = config::FirmwareVersion;
    doc["uptimeMs"] = s.uptimeMs;
    doc["setupDone"] = s.setupDone;
    doc["wifiConnected"] = s.wifiConnected;
    doc["bleReady"] = s.bleReady;
    doc["bleConnected"] = s.bleConnected;
    doc["webReady"] = s.webReady;
    doc["displayReady"] = s.displayReady;
    doc["touchReady"] = s.touchReady;
    doc["audioReady"] = s.audioReady;
}

void addPrinterState(JsonDocument& doc) {
    const SystemState& s = state();
    JsonObject printer = doc["printer"].to<JsonObject>();
    printer["configured"] = s.printerConfigured;
    printer["connected"] = s.printerConnected;
    printer["state"] = printerStateName(s.printerState);
    printer["status"] = s.printerStatusText;
    printer["progress"] = s.printProgress;
    printer["filename"] = s.printFilename;
    printer["activeTool"] = s.activeTool;
    if (isnan(s.activeToolTempC)) printer["activeToolTempC"] = nullptr;
    else printer["activeToolTempC"] = s.activeToolTempC;
    if (isnan(s.bedTempC)) printer["bedTempC"] = nullptr;
    else printer["bedTempC"] = s.bedTempC;
    if (isnan(s.chamberTempC)) printer["chamberTempC"] = nullptr;
    else printer["chamberTempC"] = s.chamberTempC;
    printer["lastUpdateMs"] = s.lastPrinterUpdateMs;
}

}

void WebControlService::begin() {
    registerRoutes();
    updateRuntimeState();
}

void WebControlService::loop() {
    updateRuntimeState();
    if (serverRunning_) {
        server_.handleClient();
    }
}

void WebControlService::registerRoutes() {
    if (routesReady_) return;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
    server_.on("/api/settings", HTTP_GET, [this]() { handleSettings(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleUpdateSettings(); });
    server_.on("/api/printer/test", HTTP_POST, [this]() { handlePrinterTest(); });
    server_.on("/api/state", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/settings", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/printer/test", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.onNotFound([this]() { handleNotFound(); });

    routesReady_ = true;
}

void WebControlService::updateRuntimeState() {
    if (shouldRun()) {
        if (!serverRunning_) start();
    } else if (serverRunning_ || mdnsRunning_) {
        stop();
    }
    state().webReady = serverRunning_;
}

void WebControlService::start() {
    server_.begin(80);
    serverRunning_ = true;

    if (!mdnsRunning_) {
        mdnsRunning_ = MDNS.begin(deviceIdentity().hostname());
        if (mdnsRunning_) {
            MDNS.addService("http", "tcp", 80);
        }
    }

    IPAddress ip = WiFi.localIP();
    Serial.printf("[web] ready http://%s.local/ ip=%u.%u.%u.%u\n",
                  deviceIdentity().hostname(),
                  ip[0], ip[1], ip[2], ip[3]);
}

void WebControlService::stop() {
    server_.stop();
    serverRunning_ = false;
    if (mdnsRunning_) {
        MDNS.end();
        mdnsRunning_ = false;
    }
    state().webReady = false;
    Serial.println("[web] stopped");
}

bool WebControlService::shouldRun() const {
    const AppSettings& cfg = settingsService().settings();
    if (cfg.companionTransport == CompanionTransport::Ble) return false;
    return WiFi.status() == WL_CONNECTED;
}

void WebControlService::sendCors() {
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server_.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server_.sendHeader("Cache-Control", "no-store");
}

void WebControlService::sendJson(int code, const String& payload) {
    sendCors();
    server_.send(code, "application/json", payload);
}

void WebControlService::sendNoContent() {
    sendCors();
    server_.send(204);
}

void WebControlService::handleRoot() {
    JsonDocument doc;
    addCommonState(doc);
    doc["api"] = "/api/state";
    doc["settings"] = "/api/settings";
    doc["printerTest"] = "/api/printer/test";

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleState() {
    JsonDocument doc;
    addCommonState(doc);
    addPrinterState(doc);
    doc["wifiIp"] = WiFi.localIP().toString();
    doc["wifiRssi"] = WiFi.RSSI();

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleSettings() {
    const AppSettings& cfg = settingsService().settings();

    char name[25];
    deviceIdentity().effectiveName(cfg.deviceName, name, sizeof(name));

    JsonDocument doc;
    doc["id"] = deviceIdentity().id();
    doc["name"] = name;
    doc["defaultName"] = deviceIdentity().defaultName();
    doc["hostname"] = deviceIdentity().hostname();
    doc["setupDone"] = cfg.setupDone;
    doc["bleEnabled"] = cfg.bleEnabled;
    doc["transport"] = transportName(cfg.companionTransport);
    doc["transportValue"] = static_cast<uint8_t>(cfg.companionTransport);
    doc["displayBrightness"] = cfg.displayBrightness;
    doc["uiSkin"] = static_cast<uint8_t>(cfg.uiSkin);
    doc["uiColorMode"] = static_cast<uint8_t>(cfg.uiColorMode);
    doc["wifiSsid"] = cfg.wifiSsid;
    doc["wifiPasswordSet"] = cfg.wifiPassword[0] != '\0';
    doc["printerHost"] = cfg.printerHost;
    doc["printerPort"] = cfg.printerPort;
    doc["printerApiKeySet"] = cfg.printerApiKey[0] != '\0';

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleUpdateSettings() {
    const String body = server_.arg("plain");
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return;
    }

    AppSettings& cfg = settingsService().mutableSettings();

    if (doc["deviceName"].is<const char*>()) {
        char cleanName[sizeof(cfg.deviceName)] = "";
        deviceIdentity().sanitizeName(doc["deviceName"].as<const char*>(), cleanName, sizeof(cleanName));
        strlcpy(cfg.deviceName, cleanName, sizeof(cfg.deviceName));
    }
    if (doc["resetDeviceName"].as<bool>()) {
        cfg.deviceName[0] = '\0';
    }
    if (doc["setupDone"].is<bool>()) {
        cfg.setupDone = doc["setupDone"].as<bool>();
        state().setupDone = cfg.setupDone;
    }
    if (doc["bleEnabled"].is<bool>()) {
        cfg.bleEnabled = doc["bleEnabled"].as<bool>();
    }
    CompanionTransport transport;
    if (parseTransport(doc["transport"], transport) || parseTransport(doc["companionTransport"], transport)) {
        cfg.companionTransport = transport;
    }
    if (doc["displayBrightness"].is<uint8_t>() || doc["displayBrightness"].is<int>()) {
        const int value = doc["displayBrightness"].as<int>();
        cfg.displayBrightness = static_cast<uint8_t>(constrain(value, 0, 100));
    }
    if (doc["uiSkin"].is<uint8_t>() || doc["uiSkin"].is<int>()) {
        const int value = doc["uiSkin"].as<int>();
        if (value >= static_cast<int>(UiSkin::Coronet) && value <= static_cast<int>(UiSkin::Minimal)) {
            cfg.uiSkin = static_cast<UiSkin>(value);
        }
    }
    if (doc["uiColorMode"].is<uint8_t>() || doc["uiColorMode"].is<int>()) {
        const int value = doc["uiColorMode"].as<int>();
        if (value >= static_cast<int>(UiColorMode::Dark) && value <= static_cast<int>(UiColorMode::Auto)) {
            cfg.uiColorMode = static_cast<UiColorMode>(value);
        }
    }
    if (doc["wifiSsid"].is<const char*>()) {
        strlcpy(cfg.wifiSsid, doc["wifiSsid"].as<const char*>(), sizeof(cfg.wifiSsid));
    }
    if (doc["wifiPassword"].is<const char*>()) {
        strlcpy(cfg.wifiPassword, doc["wifiPassword"].as<const char*>(), sizeof(cfg.wifiPassword));
    }
    if (doc["printerHost"].is<const char*>() || doc["host"].is<const char*>()) {
        const char* rawHost = doc["printerHost"].is<const char*>() ? doc["printerHost"].as<const char*>() : doc["host"].as<const char*>();
        uint16_t port = cfg.printerPort ? cfg.printerPort : 7125;
        if (doc["printerPort"].is<uint16_t>() || doc["printerPort"].is<int>()) {
            port = static_cast<uint16_t>(constrain(doc["printerPort"].as<int>(), 1, 65535));
        } else if (doc["port"].is<uint16_t>() || doc["port"].is<int>()) {
            port = static_cast<uint16_t>(constrain(doc["port"].as<int>(), 1, 65535));
        }

        char cleanHost[sizeof(cfg.printerHost)] = "";
        normalizePrinterHost(rawHost, cleanHost, sizeof(cleanHost), &port);
        if (cleanHost[0]) {
            strlcpy(cfg.printerHost, cleanHost, sizeof(cfg.printerHost));
            cfg.printerPort = port;
        }
    } else if (doc["printerPort"].is<uint16_t>() || doc["printerPort"].is<int>() ||
               doc["port"].is<uint16_t>() || doc["port"].is<int>()) {
        const int value = doc["printerPort"].isNull() ? doc["port"].as<int>() : doc["printerPort"].as<int>();
        cfg.printerPort = static_cast<uint16_t>(constrain(value, 1, 65535));
    }
    if (doc["printerApiKey"].is<const char*>()) {
        strlcpy(cfg.printerApiKey, doc["printerApiKey"].as<const char*>(), sizeof(cfg.printerApiKey));
    } else if (doc["apiKey"].is<const char*>()) {
        strlcpy(cfg.printerApiKey, doc["apiKey"].as<const char*>(), sizeof(cfg.printerApiKey));
    }

    settingsService().save();

    JsonDocument reply;
    reply["ok"] = true;
    reply["transport"] = transportName(cfg.companionTransport);
    reply["wifiReconnectMayFollow"] = doc["wifiSsid"].is<const char*>() || doc["wifiPassword"].is<const char*>();

    String payload;
    serializeJson(reply, payload);
    sendJson(200, payload);
}

void WebControlService::handlePrinterTest() {
    const PrinterTestResult result = printerService().testConnection();
    JsonDocument doc;
    doc["ok"] = result.ok;
    doc["httpCode"] = result.httpCode;
    doc["message"] = result.message;
    addPrinterState(doc);

    String payload;
    serializeJson(doc, payload);
    sendJson(result.ok ? 200 : 503, payload);
}

void WebControlService::handleNotFound() {
    sendJson(404, "{\"ok\":false,\"error\":\"not_found\"}");
}

}
