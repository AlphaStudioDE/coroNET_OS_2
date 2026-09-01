#include "PrinterService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {
constexpr uint32_t kPollIdleMs = 5000;
constexpr uint32_t kPollActiveMs = 2000;
constexpr uint32_t kPollOfflineMs = 8000;
constexpr uint32_t kHttpTimeoutMs = 700;

PrinterService gPrinterService;

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

PrinterState normalizePrinterState(const char* state) {
    if (!state || !*state) return PrinterState::Idle;
    String s(state);
    s.trim();
    s.toLowerCase();

    if (s == "standby" || s == "ready" || s == "operational" || s == "idle") return PrinterState::Idle;
    if (s == "printing" || s == "print" || s == "busy") return PrinterState::Printing;
    if (s == "paused" || s == "pause") return PrinterState::Paused;
    if (s == "complete" || s == "completed" || s == "finished" || s == "finish") return PrinterState::Complete;
    if (s == "error" || s == "cancelled" || s == "canceled" || s == "timeout") return PrinterState::Error;
    return PrinterState::Unknown;
}

uint8_t clampProgress(float progress) {
    if (isnan(progress) || progress < 0.0f) return 0;
    if (progress > 1.0f) progress = 1.0f;
    return static_cast<uint8_t>(progress * 100.0f + 0.5f);
}

uint8_t toolIndexFromObject(const char* objectName) {
    if (!objectName || strcmp(objectName, "extruder") == 0) return 0;
    if (strcmp(objectName, "extruder1") == 0) return 1;
    if (strcmp(objectName, "extruder2") == 0) return 2;
    if (strcmp(objectName, "extruder3") == 0) return 3;
    return 0;
}

void addAuthHeader(HTTPClient& http) {
    const AppSettings& cfg = settingsService().settings();
    if (cfg.printerApiKey[0]) {
        http.addHeader("X-Api-Key", cfg.printerApiKey);
    }
}
}

PrinterService& printerService() {
    return gPrinterService;
}

void PrinterService::begin() {
    started_ = true;
    SystemState& s = state();
    s.setupDone = settingsService().settings().setupDone;
    s.printerConfigured = configured();
    strlcpy(s.printerStatusText, s.printerConfigured ? "waiting_for_wifi" : "not_configured", sizeof(s.printerStatusText));
}

void PrinterService::loop() {
    if (!started_) return;

    SystemState& s = state();
    s.setupDone = settingsService().settings().setupDone;
    s.printerConfigured = configured();

    if (!s.printerConfigured) {
        setOffline("not_configured");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        setOffline("wifi_offline");
        return;
    }

    const uint32_t now = millis();
    const uint32_t interval =
        s.printerConnected
            ? ((s.printerState == PrinterState::Printing || s.printerState == PrinterState::Paused) ? kPollActiveMs : kPollIdleMs)
            : kPollOfflineMs;

    if (now - lastPollMs_ < interval) return;
    lastPollMs_ = now;

    if (!pollStatus()) {
        setOffline("poll_failed");
    }
}

PrinterTestResult PrinterService::testConnection() {
    PrinterTestResult result;
    lastTestMs_ = millis();

    if (!configured()) {
        strlcpy(result.message, "printer_not_configured", sizeof(result.message));
        setOffline(result.message);
        return result;
    }

    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(result.message, "wifi_offline", sizeof(result.message));
        setOffline(result.message);
        return result;
    }

    result.ok = requestInfo(&result);
    if (result.ok) {
        state().printerConnected = true;
        state().lastPrinterUpdateMs = millis();
        strlcpy(state().printerStatusText, "online", sizeof(state().printerStatusText));
        pollStatus();
    }
    return result;
}

bool PrinterService::configured() const {
    const AppSettings& cfg = settingsService().settings();
    return cfg.printerHost[0] != '\0' && cfg.printerPort > 0;
}

String PrinterService::baseUrl() const {
    const AppSettings& cfg = settingsService().settings();
    String host = cfg.printerHost;
    host.trim();
    if (host.startsWith("http://")) host.remove(0, 7);
    if (host.startsWith("https://")) host.remove(0, 8);
    const int slash = host.indexOf('/');
    if (slash >= 0) host.remove(slash);
    return String("http://") + host + ":" + String(cfg.printerPort);
}

bool PrinterService::requestInfo(PrinterTestResult* result) {
    HTTPClient http;
    const String url = baseUrl() + "/printer/info";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(url)) {
        if (result) strlcpy(result->message, "http_begin_failed", sizeof(result->message));
        return false;
    }
    addAuthHeader(http);

    const int code = http.GET();
    if (result) result->httpCode = code;
    http.end();

    if (code == 200) {
        if (result) strlcpy(result->message, "moonraker_online", sizeof(result->message));
        return true;
    }

    if (result) {
        snprintf(result->message, sizeof(result->message), "moonraker_http_%d", code);
    }
    return false;
}

bool PrinterService::pollStatus() {
    HTTPClient http;
    const String url = baseUrl() + "/printer/objects/query";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(url)) return false;
    http.addHeader("Content-Type", "application/json");
    addAuthHeader(http);

    const char* body =
        "{"
        "\"objects\":{"
        "\"print_stats\":[\"state\",\"filename\",\"print_duration\",\"total_duration\"],"
        "\"display_status\":[\"progress\"],"
        "\"toolhead\":[\"extruder\"],"
        "\"extruder\":[\"temperature\"],"
        "\"extruder1\":[\"temperature\"],"
        "\"extruder2\":[\"temperature\"],"
        "\"extruder3\":[\"temperature\"],"
        "\"heater_bed\":[\"temperature\"],"
        "\"temperature_sensor cavity\":[\"temperature\"]"
        "}"
        "}";

    const int code = http.POST(body);
    if (code != 200) {
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;

    JsonVariant status = doc["result"]["status"];
    const char* rawState = status["print_stats"]["state"] | "standby";
    const char* filename = status["print_stats"]["filename"] | "-";
    const float progress = status["display_status"]["progress"] | 0.0f;
    const char* extruder = status["toolhead"]["extruder"] | "extruder";
    const uint8_t tool = toolIndexFromObject(extruder);

    float toolTemp = NAN;
    if (tool == 0) toolTemp = status["extruder"]["temperature"] | NAN;
    else if (tool == 1) toolTemp = status["extruder1"]["temperature"] | NAN;
    else if (tool == 2) toolTemp = status["extruder2"]["temperature"] | NAN;
    else if (tool == 3) toolTemp = status["extruder3"]["temperature"] | NAN;

    SystemState& s = state();
    s.printerState = normalizePrinterState(rawState);
    s.printProgress = clampProgress(progress);
    s.activeTool = tool;
    s.activeToolTempC = toolTemp;
    s.bedTempC = status["heater_bed"]["temperature"] | NAN;
    s.chamberTempC = status["temperature_sensor cavity"]["temperature"] | NAN;
    s.printerConnected = true;
    s.lastPrinterUpdateMs = millis();
    strlcpy(s.printFilename, filename, sizeof(s.printFilename));
    strlcpy(s.printerStatusText, printerStateName(s.printerState), sizeof(s.printerStatusText));
    return true;
}

void PrinterService::setOffline(const char* message, int httpCode) {
    SystemState& s = state();
    s.printerConnected = false;
    s.printerState = configured() ? PrinterState::Unknown : PrinterState::Idle;
    if (httpCode) {
        snprintf(s.printerStatusText, sizeof(s.printerStatusText), "%s_%d", message ? message : "offline", httpCode);
    } else {
        strlcpy(s.printerStatusText, message ? message : "offline", sizeof(s.printerStatusText));
    }
}

}
