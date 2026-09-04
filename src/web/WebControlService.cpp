#include "WebControlService.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>

#include "../config/AppConfig.h"
#include "../audio/AudioService.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemHealth.h"
#include "../core/SystemState.h"
#include "../core/TimeZoneCatalog.h"
#include "../led/LedService.h"
#include "../panda/PandaBreathService.h"
#include "../printer/PrinterService.h"
#include "../settings/SettingsService.h"
#include "../update/OtaService.h"
#include "../wifi/WifiService.h"
#include "generated/WebUiAsset.h"

namespace coronet {

namespace {

const char* CollectedHeaders[] = {
    "Authorization",
    "X-coroNET-Token",
    "X-coroNET-Web-Session",
    "Content-Length",
    "Origin",
};

bool constantTimeEquals(const char* expected, const String& supplied) {
    if (!expected || !expected[0] || supplied.isEmpty()) return false;
    const size_t expectedLength = strlen(expected);
    const size_t suppliedLength = supplied.length();
    size_t difference = expectedLength ^ suppliedLength;
    const size_t compareLength = max(expectedLength, suppliedLength);
    for (size_t i = 0; i < compareLength; ++i) {
        const uint8_t a = i < expectedLength ? static_cast<uint8_t>(expected[i]) : 0;
        const uint8_t b = i < suppliedLength ? static_cast<uint8_t>(supplied[i]) : 0;
        difference |= a ^ b;
    }
    return difference == 0;
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
    doc["sdReady"] = s.sdReady;
    doc["audioPlaying"] = s.audioPlaying;
    doc["ledReady"] = s.ledReady;
    doc["fanPercent"] = s.fanPercent;
    doc["flapPercent"] = s.flapPercent;
    doc["diyHeaterReady"] = s.diyHeaterReady;
    doc["diyHeaterHigh"] = s.diyHeaterHigh;
    doc["quietActive"] = s.quietActive;
    doc["pandaConnected"] = s.pandaConnected;
    doc["pandaHeating"] = s.pandaHeating;
    doc["pandaPhase"] = static_cast<uint8_t>(s.pandaPhase);
    if (isnan(s.pandaCurrentTempC)) doc["pandaCurrentTempC"] = nullptr;
    else doc["pandaCurrentTempC"] = s.pandaCurrentTempC;
    doc["pandaStatus"] = s.pandaStatusText;
    doc["maintenanceMode"] = s.maintenanceMode;
    JsonObject ota = doc["ota"].to<JsonObject>();
    ota["state"] = static_cast<uint8_t>(s.otaState);
    ota["progress"] = s.otaProgress;
    ota["available"] = s.otaUpdateAvailable;
    ota["version"] = s.otaAvailableVersion;
    ota["status"] = s.otaStatusText;
}

void addPrinterState(JsonDocument& doc) {
    const SystemState& s = state();
    JsonObject printer = doc["printer"].to<JsonObject>();
    printer["configured"] = s.printerConfigured;
    printer["connected"] = s.printerConnected;
    printer["telemetryValid"] = s.printerTelemetryValid;
    printer["telemetryRevision"] = s.printerTelemetryRevision;
    printer["connectionRevision"] = s.printerConnectionRevision;
    printer["eventSequence"] = s.printerStateEventSequence;
    printer["eventFrom"] = printerStateName(s.printerEventFrom);
    printer["eventTo"] = printerStateName(s.printerEventTo);
    printer["stateChangedMs"] = s.printerStateChangedMs;
    printer["state"] = printerStateName(s.printerState);
    printer["status"] = s.printerStatusText;
    printer["progress"] = s.printProgress;
    printer["filename"] = s.printFilename;
    printer["material"] = s.materialName;
    printer["filamentColorRgb"] = s.filamentColorRgb;
    printer["durationSec"] = s.printDurationSec;
    printer["etaSec"] = s.printEtaSec;
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
    rotateWebSession();
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

    server_.collectHeaders(CollectedHeaders, sizeof(CollectedHeaders) / sizeof(CollectedHeaders[0]));

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api", HTTP_GET, [this]() { if (authorizeRequest()) handleApiDescription(); });
    server_.on("/api/web/session", HTTP_GET, [this]() { handleWebSession(); });
    server_.on("/api/state", HTTP_GET, [this]() { if (authorizeRequest()) handleState(); });
    server_.on("/api/settings", HTTP_GET, [this]() { if (authorizeRequest()) handleSettings(); });
    server_.on("/api/settings", HTTP_POST, [this]() { if (authorizeRequest()) handleUpdateSettings(); });
    server_.on("/api/led/catalog", HTTP_GET, [this]() { if (authorizeRequest()) handleLedCatalog(); });
    server_.on("/api/led/preview", HTTP_POST, [this]() { if (authorizeRequest()) handleLedPreview(); });
    server_.on("/api/led/calibration", HTTP_POST, [this]() { if (authorizeRequest()) handleLedCalibration(); });
    server_.on("/api/timezones", HTTP_GET, [this]() { if (authorizeRequest()) handleTimeZones(); });
    server_.on("/api/audio/library", HTTP_GET, [this]() { if (authorizeRequest()) handleAudioLibrary(); });
    server_.on("/api/audio/play", HTTP_POST, [this]() { if (authorizeRequest()) handleAudioPlay(); });
    server_.on("/api/audio/stop", HTTP_POST, [this]() { if (authorizeRequest()) handleAudioStop(); });
    server_.on("/api/audio/rescan", HTTP_POST, [this]() { if (authorizeRequest()) handleAudioRescan(); });
    server_.on("/api/printer/test", HTTP_POST, [this]() { if (authorizeRequest()) handlePrinterTest(); });
    server_.on("/api/panda/discover", HTTP_POST, [this]() { if (authorizeRequest()) handlePandaDiscover(); });
    server_.on("/api/ota/check", HTTP_POST, [this]() { if (authorizeRequest()) handleOtaCheck(); });
    server_.on("/api/ota/install", HTTP_POST, [this]() { if (authorizeRequest()) handleOtaInstall(false); });
    server_.on("/api/ota/reinstall", HTTP_POST, [this]() { if (authorizeRequest()) handleOtaInstall(true); });
    server_.on("/api/ota/sd", HTTP_POST, [this]() { if (authorizeRequest()) handleOtaSdRecovery(); });
    server_.on("/api/state", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/settings", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/led/catalog", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/led/preview", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/led/calibration", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/timezones", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/audio/library", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/audio/play", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/audio/stop", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/audio/rescan", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/printer/test", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/panda/discover", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/ota/check", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/ota/install", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/ota/reinstall", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.on("/api/ota/sd", HTTP_OPTIONS, [this]() { sendNoContent(); });
    server_.onNotFound([this]() { handleNotFound(); });

    routesReady_ = true;
}

void WebControlService::updateRuntimeState() {
    if (shouldRun()) {
        if (!serverRunning_) start();
        publishMdnsIfNeeded();
    } else if (serverRunning_) {
        stop();
    }
    state().webReady = serverRunning_;
}

void WebControlService::start() {
    logHeapDiagnostics("web-before-server");
    server_.begin(80);
    serverRunning_ = true;
    logHeapDiagnostics("web-after-server");

    publishMdnsIfNeeded();
    logHeapDiagnostics("web-after-mdns");

    IPAddress ip = WiFi.localIP();
    Serial.printf("[web] ready http://%s.local/ ip=%u.%u.%u.%u\n",
                  deviceIdentity().hostname(),
                  ip[0], ip[1], ip[2], ip[3]);
}

void WebControlService::stop() {
    server_.stop();
    serverRunning_ = false;
    state().webReady = false;
    Serial.println("[web] stopped");
}

void WebControlService::publishMdnsIfNeeded() {
    const uint32_t generation = wifiService().mdnsGeneration();
    if (!serverRunning_ || !generation || generation == mdnsGenerationPublished_) return;
    if (wifiService().publishMdnsService("http", "tcp", 80)) {
        mdnsGenerationPublished_ = generation;
        Serial.printf("[web] mDNS HTTP service published generation=%lu\n",
                      static_cast<unsigned long>(generation));
    }
}

bool WebControlService::shouldRun() const {
    if (state().maintenanceMode) return false;
    const AppSettings& cfg = settingsService().settings();
    if (cfg.companionTransport == CompanionTransport::Ble) return false;
    return WiFi.status() == WL_CONNECTED;
}

bool WebControlService::authorizeRequest() {
    const char* expected = settingsService().settings().apiToken;
    String supplied = server_.header("X-coroNET-Token");
    if (supplied.isEmpty()) {
        supplied = server_.header("Authorization");
        static constexpr char BearerPrefix[] = "Bearer ";
        if (supplied.startsWith(BearerPrefix)) supplied.remove(0, strlen(BearerPrefix));
    }
    supplied.trim();
    if (constantTimeEquals(expected, supplied)) return true;

    const String webSession = server_.header("X-coroNET-Web-Session");
    if (authorizeWebOrigin() && constantTimeEquals(webSessionToken_, webSession)) return true;

    server_.sendHeader("WWW-Authenticate", "Bearer");
    sendJson(401, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
}

bool WebControlService::authorizeWebOrigin() {
    String requestHost = server_.hostHeader();
    requestHost.trim();
    requestHost.toLowerCase();
    String host = requestHost;
    host.trim();
    const int portSeparator = host.lastIndexOf(':');
    if (portSeparator > 0) host.remove(portSeparator);

    String hostname = deviceIdentity().hostname();
    hostname.toLowerCase();
    const String localName = hostname + ".local";
    const String localIp = WiFi.localIP().toString();
    if (host != hostname && host != localName && host != localIp) return false;

    String origin = server_.header("Origin");
    origin.trim();
    if (origin.isEmpty()) return true;
    origin.toLowerCase();
    return origin == "http://" + requestHost;
}

void WebControlService::rotateWebSession() {
    static constexpr char Hex[] = "0123456789abcdef";
    for (size_t offset = 0; offset < 16; offset += sizeof(uint32_t)) {
        const uint32_t random = esp_random();
        for (size_t byte = 0; byte < sizeof(random); ++byte) {
            const uint8_t value = static_cast<uint8_t>(random >> (byte * 8U));
            const size_t index = (offset + byte) * 2U;
            webSessionToken_[index] = Hex[value >> 4U];
            webSessionToken_[index + 1U] = Hex[value & 0x0FU];
        }
    }
    webSessionToken_[32] = '\0';
}

void WebControlService::sendCommonHeaders() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
}

void WebControlService::sendJson(int code, const String& payload) {
    sendCommonHeaders();
    server_.send(code, "application/json", payload);
}

void WebControlService::sendNoContent() {
    sendCommonHeaders();
    server_.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server_.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-coroNET-Token");
    server_.send(204);
}

void WebControlService::handleRoot() {
    if (!authorizeWebOrigin()) {
        sendJson(421, "{\"ok\":false,\"error\":\"invalid_host\"}");
        return;
    }
    server_.sendHeader("Cache-Control", "no-cache");
    server_.sendHeader("Content-Encoding", "gzip");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.sendHeader("Content-Security-Policy",
                       "default-src 'self'; connect-src 'self'; img-src 'self' data:; "
                       "style-src 'unsafe-inline'; script-src 'unsafe-inline'; frame-ancestors 'none'");
    server_.send_P(200, PSTR("text/html; charset=utf-8"),
                   reinterpret_cast<PGM_P>(webui::IndexHtmlGzip), webui::IndexHtmlGzipSize);
}

void WebControlService::handleApiDescription() {
    JsonDocument doc;
    addCommonState(doc);
    doc["api"] = "/api/state";
    doc["settings"] = "/api/settings";
    doc["ledPreview"] = "/api/led/preview";
    doc["ledCatalog"] = "/api/led/catalog";
    doc["ledCalibration"] = "/api/led/calibration";
    doc["timeZones"] = "/api/timezones";
    doc["audioLibrary"] = "/api/audio/library";
    doc["audioPlay"] = "/api/audio/play";
    doc["audioStop"] = "/api/audio/stop";
    doc["printerTest"] = "/api/printer/test";
    doc["pandaDiscover"] = "/api/panda/discover";
    doc["otaCheck"] = "/api/ota/check";
    doc["otaInstall"] = "/api/ota/install";
    doc["authentication"] = "Bearer or X-coroNET-Token";

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleWebSession() {
    if (!authorizeWebOrigin()) {
        sendJson(403, "{\"ok\":false,\"error\":\"local_origin_required\"}");
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["token"] = webSessionToken_;
    doc["expires"] = "reboot";
    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleState() {
    JsonDocument doc;
    addCommonState(doc);
    addPrinterState(doc);
    doc["settingsRevision"] = settingsService().revision();
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
    doc["apiPaired"] = cfg.apiPaired;
    doc["transport"] = transportName(cfg.companionTransport);
    doc["transportValue"] = static_cast<uint8_t>(cfg.companionTransport);
    doc["displayBrightness"] = cfg.displayBrightness;
    doc["uiSkin"] = static_cast<uint8_t>(cfg.uiSkin);
    doc["uiColorMode"] = static_cast<uint8_t>(cfg.uiColorMode);
    doc["accentHueDegrees"] = cfg.accentHueDegrees;
    doc["screenSaverMode"] = static_cast<uint8_t>(cfg.screenSaverMode);
    doc["screenSaverDelayMinutes"] = cfg.screenSaverDelayMinutes;
    doc["clockBrightness"] = cfg.clockBrightness;
    doc["clockStyle"] = static_cast<uint8_t>(cfg.clockStyle);
    doc["clock24Hour"] = cfg.clock24Hour;
    doc["timeZone"] = cfg.timeZone;
    doc["settingsRevision"] = settingsService().revision();
    doc["quietTarget"] = static_cast<uint8_t>(cfg.quietTarget);
    doc["quietDurationMinutes"] = cfg.quietDurationMinutes;
    doc["quietErrorsBypass"] = cfg.quietErrorsBypass;
    doc["ledEnabled"] = cfg.ledEnabled;
    doc["ledOtherMode"] = cfg.ledOtherMode;
    doc["insideColorStyle"] = static_cast<uint8_t>(cfg.insideColorStyle);
    doc["mirrorLedLayout"] = cfg.mirrorLedLayout;
    JsonArray ledBrightness = doc["ledBrightness"].to<JsonArray>();
    JsonArray ledDimmEnabled = doc["ledDimmEnabled"].to<JsonArray>();
    JsonArray ledDimmPercent = doc["ledDimmPercent"].to<JsonArray>();
    for (uint8_t i = 0; i < enumCount(LedSection{}); ++i) {
        ledBrightness.add(cfg.ledBrightness[i]); ledDimmEnabled.add(cfg.ledDimmEnabled[i]); ledDimmPercent.add(cfg.ledDimmPercent[i]);
    }
    JsonArray animations = doc["ledAnimation"].to<JsonArray>();
    JsonArray remix = doc["ledColorRemixDegrees"].to<JsonArray>();
    for (uint8_t i = 0; i < enumCount(LedCategory{}); ++i) { animations.add(cfg.ledAnimation[i]); remix.add(cfg.ledColorRemixDegrees[i]); }
    JsonArray calibrationHue = doc["ledCalibrationHue"].to<JsonArray>();
    JsonArray calibrationSaturation = doc["ledCalibrationSaturation"].to<JsonArray>();
    JsonArray calibrationBrightness = doc["ledCalibrationBrightness"].to<JsonArray>();
    for (uint8_t i = 0; i < 8; ++i) {
        calibrationHue.add(cfg.ledCalibrationHue[i]);
        calibrationSaturation.add(cfg.ledCalibrationSaturation[i]);
        calibrationBrightness.add(cfg.ledCalibrationBrightness[i]);
    }
    JsonArray soundVolume = doc["soundVolume"].to<JsonArray>();
    JsonArray soundRepeat = doc["soundRepeat"].to<JsonArray>();
    JsonArray soundPath = doc["soundPath"].to<JsonArray>();
    JsonArray soundAvailable = doc["soundAvailable"].to<JsonArray>();
    JsonArray soundSelectionAvailable = doc["soundSelectionAvailable"].to<JsonArray>();
    for (uint8_t i = 0; i < enumCount(SoundScenario{}); ++i) {
        soundVolume.add(cfg.soundVolume[i]);
        soundRepeat.add(cfg.soundRepeat[i]);
        soundPath.add(cfg.soundPath[i]);
        char resolved[65] = "";
        soundAvailable.add(audioService().resolveScenarioPath(static_cast<SoundScenario>(i), resolved));
        soundSelectionAvailable.add(cfg.soundPath[i][0] == '\0' || audioService().pathAvailable(cfg.soundPath[i]));
    }
    doc["ventMode"] = static_cast<uint8_t>(cfg.ventMode);
    doc["ventTargetTempC"] = cfg.ventTargetTempC;
    doc["manualFanPercent"] = cfg.manualFanPercent;
    doc["manualFlapPercent"] = cfg.manualFlapPercent;
    doc["fanMinPercent"] = cfg.fanMinPercent;
    doc["fanMaxPercent"] = cfg.fanMaxPercent;
    doc["failsafeFanPercent"] = cfg.failsafeFanPercent;
    doc["failsafeFlapPercent"] = cfg.failsafeFlapPercent;
    doc["servoClosedUs"] = cfg.servoClosedUs;
    doc["servoOpenUs"] = cfg.servoOpenUs;
    doc["servoReverse"] = cfg.servoReverse;
    doc["diyHeaterOutputHigh"] = cfg.diyHeaterOutputHigh;
    doc["pandaEnabled"] = cfg.pandaEnabled;
    doc["pandaHost"] = cfg.pandaHost;
    doc["pandaMode"] = static_cast<uint8_t>(cfg.pandaMode);
    doc["pandaTargetTempC"] = cfg.pandaTargetTempC;
    doc["pandaPrintTargetTempC"] = cfg.pandaPrintTargetTempC;
    doc["pandaDryPreset"] = static_cast<uint8_t>(cfg.pandaDryPreset);
    doc["pandaDryHours"] = cfg.pandaDryHours;
    doc["pandaPreheatHoldMinutes"] = cfg.pandaPreheatHoldMinutes;
    doc["pandaTemperingDurationMinutes"] = cfg.pandaTemperingDurationMinutes;
    doc["pandaTemperingEndTempC"] = cfg.pandaTemperingEndTempC;
    doc["pandaTemperingAfterPrint"] = cfg.pandaTemperingAfterPrint;
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
    if (body.length() > config::WebMaxJsonBodyBytes) {
        sendJson(413, "{\"ok\":false,\"error\":\"payload_too_large\"}");
        return;
    }
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return;
    }

    AppSettings cfg = settingsService().snapshot();

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
        cfg.displayBrightness = static_cast<uint8_t>(constrain(value, 10, 100));
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
    if (doc["accentHueDegrees"].is<int>()) cfg.accentHueDegrees = constrain(doc["accentHueDegrees"].as<int>(), 0, 359);
    if (doc["screenSaverMode"].is<int>()) cfg.screenSaverMode = static_cast<ScreenSaverMode>(constrain(doc["screenSaverMode"].as<int>(), 0, 2));
    if (doc["screenSaverDelayMinutes"].is<int>()) cfg.screenSaverDelayMinutes = constrain(doc["screenSaverDelayMinutes"].as<int>(), 1, 60);
    if (doc["clockBrightness"].is<int>()) cfg.clockBrightness = constrain(doc["clockBrightness"].as<int>(), 5, 100);
    if (doc["clockStyle"].is<int>()) cfg.clockStyle = static_cast<ClockStyle>(constrain(doc["clockStyle"].as<int>(), 0, static_cast<int>(ClockStyle::Count) - 1));
    if (doc["clock24Hour"].is<bool>()) cfg.clock24Hour = doc["clock24Hour"].as<bool>();
    if (doc["timeZone"].is<const char*>()) {
        const char* timeZone = doc["timeZone"].as<const char*>();
        if (!timeZone || !timeZone[0] || strlen(timeZone) >= sizeof(cfg.timeZone)) {
            sendJson(400, "{\"ok\":false,\"error\":\"time_zone_invalid\"}");
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
    auto copyPercentArray = [](JsonArrayConst source, uint8_t* target, uint8_t count) {
        for (uint8_t i = 0; i < count && i < source.size(); ++i) target[i] = constrain(source[i].as<int>(), 0, 100);
    };
    if (doc["ledBrightness"].is<JsonArrayConst>()) copyPercentArray(doc["ledBrightness"].as<JsonArrayConst>(), cfg.ledBrightness, enumCount(LedSection{}));
    if (doc["ledDimmPercent"].is<JsonArrayConst>()) copyPercentArray(doc["ledDimmPercent"].as<JsonArrayConst>(), cfg.ledDimmPercent, enumCount(LedSection{}));
    if (doc["ledDimmEnabled"].is<JsonArrayConst>()) {
        JsonArrayConst values = doc["ledDimmEnabled"].as<JsonArrayConst>();
        for (uint8_t i = 0; i < enumCount(LedSection{}) && i < values.size(); ++i) cfg.ledDimmEnabled[i] = values[i].as<bool>();
    }
    if (doc["ledAnimation"].is<JsonArrayConst>()) {
        JsonArrayConst values = doc["ledAnimation"].as<JsonArrayConst>();
        for (uint8_t i = 0; i < enumCount(LedCategory{}) && i < values.size(); ++i) {
            const int animation = values[i].as<int>();
            if (animation < 0 || animation >= ledAnimationCount(static_cast<LedCategory>(i))) {
                sendJson(400, "{\"ok\":false,\"error\":\"led_animation_invalid\"}");
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
    if (doc["soundVolume"].is<JsonArrayConst>()) copyPercentArray(doc["soundVolume"].as<JsonArrayConst>(), cfg.soundVolume, enumCount(SoundScenario{}));
    if (doc["soundRepeat"].is<JsonArrayConst>()) {
        JsonArrayConst values = doc["soundRepeat"].as<JsonArrayConst>();
        for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) cfg.soundRepeat[i] = values[i].as<bool>();
    }
    if (doc["soundPath"].is<JsonArrayConst>()) {
        JsonArrayConst values = doc["soundPath"].as<JsonArrayConst>();
        for (uint8_t i = 0; i < enumCount(SoundScenario{}) && i < values.size(); ++i) {
            if (values[i].is<const char*>()) strlcpy(cfg.soundPath[i], values[i].as<const char*>(), sizeof(cfg.soundPath[i]));
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
        sendJson(400, "{\"ok\":false,\"error\":\"fan_range_invalid\"}");
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
        strlcpy(cfg.pandaHost, doc["pandaHost"].as<const char*>(), sizeof(cfg.pandaHost));
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

    settingsService().replace(cfg);
    state().setupDone = cfg.setupDone;
    settingsService().save();

    JsonDocument reply;
    reply["ok"] = true;
    reply["settingsRevision"] = settingsService().revision();
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

void WebControlService::handlePandaDiscover() {
    if (WiFi.status() != WL_CONNECTED) {
        sendJson(409, "{\"ok\":false,\"error\":\"wifi_unavailable\"}");
        return;
    }
    pandaBreathService().requestDiscovery();
    sendJson(202, "{\"ok\":true,\"queued\":true}");
}

void WebControlService::handleLedCatalog() {
    const int requested = server_.hasArg("category") ? server_.arg("category").toInt() : 0;
    if (requested < 0 || requested >= static_cast<int>(LedCategory::Count)) {
        sendJson(400, "{\"ok\":false,\"error\":\"category_out_of_range\"}");
        return;
    }

    const LedCategory category = static_cast<LedCategory>(requested);
    const uint8_t count = ledAnimationCount(category);
    JsonDocument doc;
    doc["ok"] = true;
    doc["category"] = requested;
    doc["count"] = count;
    JsonArray animations = doc["animations"].to<JsonArray>();
    for (uint8_t index = 0; index < count; ++index) {
        animations.add(ledAnimationName(category, index));
    }

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleLedPreview() {
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain")) || !doc["category"].is<int>() || !doc["animation"].is<int>()) {
        sendJson(400, "{\"ok\":false,\"error\":\"invalid_preview\"}");
        return;
    }
    const int category = doc["category"].as<int>();
    const int animation = doc["animation"].as<int>();
    const uint32_t durationMs = constrain(doc["durationMs"] | 10000U, 1000U, 30000U);
    if (category < 0 || category >= static_cast<int>(LedCategory::Count) || animation < 0 ||
        animation >= ledAnimationCount(static_cast<LedCategory>(category))) {
        sendJson(400, "{\"ok\":false,\"error\":\"preview_out_of_range\"}");
        return;
    }
    const bool started = ledService().requestPreview(static_cast<LedCategory>(category), static_cast<uint8_t>(animation), durationMs);
    sendJson(started ? 202 : 409, started ? "{\"ok\":true,\"preview\":true}" : "{\"ok\":false,\"error\":\"led_unavailable\"}");
}

void WebControlService::handleLedCalibration() {
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain")) || !doc["active"].is<bool>()) {
        sendJson(400, "{\"ok\":false,\"error\":\"invalid_calibration\"}");
        return;
    }
    if (!doc["active"].as<bool>()) {
        ledService().stopColorCalibration();
        sendJson(200, "{\"ok\":true,\"calibration\":false}");
        return;
    }
    const int color = doc["color"] | -1;
    if (color < 0 || color >= static_cast<int>(LedCalibrationColor::Count)) {
        sendJson(400, "{\"ok\":false,\"error\":\"calibration_color_invalid\"}");
        return;
    }
    const bool started = ledService().startColorCalibration(static_cast<LedCalibrationColor>(color));
    sendJson(started ? 200 : 409, started ? "{\"ok\":true,\"calibration\":true}" : "{\"ok\":false,\"error\":\"led_unavailable\"}");
}

void WebControlService::handleTimeZones() {
    JsonDocument doc;
    doc["ok"] = true;
    JsonArray zones = doc["zones"].to<JsonArray>();
    for (size_t index = 0; index < TimeZoneOptionCount; ++index) {
        JsonObject zone = zones.add<JsonObject>();
        zone["label"] = TimeZoneOptions[index].label;
        zone["offset"] = TimeZoneOptions[index].offset;
        zone["spec"] = TimeZoneOptions[index].spec;
    }
    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebControlService::handleAudioLibrary() {
    constexpr uint8_t PageSize = 8;
    const uint8_t folderCount = audioService().folderCount();
    const int requestedFolder = server_.hasArg("folder") ? server_.arg("folder").toInt() : 0;
    const uint8_t folder = folderCount == 0U
        ? 0U
        : static_cast<uint8_t>(constrain(requestedFolder, 0, static_cast<int>(folderCount) - 1));
    const uint8_t fileCount = folderCount == 0U ? 0U : audioService().folderFileCount(folder);
    const uint8_t pageCount = max<uint8_t>(1U, static_cast<uint8_t>((fileCount + PageSize - 1U) / PageSize));
    const int requestedPage = server_.hasArg("page") ? server_.arg("page").toInt() : 0;
    const uint8_t page = static_cast<uint8_t>(constrain(requestedPage, 0, static_cast<int>(pageCount) - 1));
    const uint8_t first = static_cast<uint8_t>(page * PageSize);

    JsonDocument doc;
    doc["ok"] = true;
    doc["sdReady"] = state().sdReady;
    doc["folder"] = folder;
    doc["folderCount"] = folderCount;
    doc["folderName"] = folderCount ? audioService().folderName(folder) : "";
    doc["page"] = page;
    doc["pageCount"] = pageCount;
    doc["fileCount"] = fileCount;
    JsonArray folders = doc["folders"].to<JsonArray>();
    for (uint8_t index = 0; index < folderCount; ++index) {
        folders.add(audioService().folderName(index));
    }
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
    sendJson(200, payload);
}

void WebControlService::handleAudioPlay() {
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain")) || !doc["scenario"].is<int>()) {
        sendJson(400, "{\"ok\":false,\"error\":\"invalid_audio_scenario\"}");
        return;
    }
    const int scenario = doc["scenario"].as<int>();
    if (scenario < 0 || scenario >= static_cast<int>(SoundScenario::Count)) {
        sendJson(400, "{\"ok\":false,\"error\":\"audio_scenario_out_of_range\"}");
        return;
    }
    const bool started = audioService().playScenario(static_cast<SoundScenario>(scenario));
    sendJson(started ? 202 : 409,
             started ? "{\"ok\":true,\"playing\":true}" : "{\"ok\":false,\"error\":\"audio_unavailable\"}");
}

void WebControlService::handleAudioStop() {
    audioService().stop();
    sendJson(200, "{\"ok\":true,\"playing\":false}");
}

void WebControlService::handleAudioRescan() {
    const bool queued = audioService().requestStorageRefresh();
    sendJson(queued ? 202 : 409,
             queued ? "{\"ok\":true,\"queued\":true}" : "{\"ok\":false,\"error\":\"audio_busy\"}");
}

void WebControlService::handleOtaCheck() {
    const bool queued = otaService().requestCheck();
    sendJson(queued ? 202 : 409, queued ? "{\"ok\":true,\"queued\":true}" : "{\"ok\":false,\"error\":\"busy\"}");
}

void WebControlService::handleOtaInstall(bool reinstall) {
    const bool queued = otaService().requestInstall(reinstall);
    sendJson(queued ? 202 : 409, queued ? "{\"ok\":true,\"queued\":true}" : "{\"ok\":false,\"error\":\"busy\"}");
}

void WebControlService::handleOtaSdRecovery() {
    const bool queued = otaService().requestSdRecovery();
    sendJson(queued ? 202 : 409, queued ? "{\"ok\":true,\"queued\":true}" : "{\"ok\":false,\"error\":\"busy\"}");
}

void WebControlService::handleNotFound() {
    sendJson(404, "{\"ok\":false,\"error\":\"not_found\"}");
}

}
