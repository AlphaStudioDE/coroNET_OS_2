#include "PrinterService.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

constexpr uint32_t kPollIdleMs = 5000;
constexpr uint32_t kPollActiveMs = 2000;
constexpr uint32_t kPollOfflineMs = 8000;
constexpr uint32_t kHttpTimeoutMs = 700;
constexpr uint8_t kFailuresBeforeOffline = 3;
constexpr uint32_t kWorkerStackBytes = 8192;
constexpr UBaseType_t kWorkerPriority = 2;
constexpr BaseType_t kWorkerCore = 0;
constexpr uint16_t kDiscoveryPort = 7125;
constexpr uint32_t kDiscoveryHttpTimeoutMs = 120;
constexpr uint16_t kDiscoveryTargetLimit = 254;

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

PrinterState normalizePrinterState(const char* stateText) {
    if (!stateText || !*stateText) return PrinterState::Idle;
    String value(stateText);
    value.trim();
    value.toLowerCase();

    if (value == "standby" || value == "ready" || value == "operational" || value == "idle") return PrinterState::Idle;
    if (value == "printing" || value == "print" || value == "busy") return PrinterState::Printing;
    if (value == "paused" || value == "pause") return PrinterState::Paused;
    if (value == "complete" || value == "completed" || value == "finished" || value == "finish") return PrinterState::Complete;
    if (value == "error" || value == "cancelled" || value == "canceled" || value == "timeout") return PrinterState::Error;
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

String baseUrl(const char* host, uint16_t port) {
    String cleanHost = host ? host : "";
    cleanHost.trim();
    if (cleanHost.startsWith("http://")) cleanHost.remove(0, 7);
    if (cleanHost.startsWith("https://")) cleanHost.remove(0, 8);
    const int slash = cleanHost.indexOf('/');
    if (slash >= 0) cleanHost.remove(slash);
    return String("http://") + cleanHost + ":" + String(port);
}

void addAuthHeader(HTTPClient& http, const char* apiKey) {
    if (apiKey && apiKey[0]) http.addHeader("X-Api-Key", apiKey);
}

}

PrinterService& printerService() {
    return gPrinterService;
}

void PrinterService::begin() {
    requestQueue_ = xQueueCreate(2, sizeof(WorkerRequest));
    resultQueue_ = xQueueCreate(1, sizeof(PollResult));
    httpMutex_ = xSemaphoreCreateMutex();
    discoveredPrinters_ = static_cast<DiscoveredPrinter*>(heap_caps_calloc(
        MaxDiscoveredPrinters, sizeof(DiscoveredPrinter), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!requestQueue_ || !resultQueue_ || !httpMutex_ || !discoveredPrinters_) {
        Serial.println("[printer] queue or mutex allocation failed");
        setOffline("printer_worker_alloc_failed");
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        workerTaskEntry,
        "coronet-printer",
        kWorkerStackBytes,
        this,
        kWorkerPriority,
        &workerTask_,
        kWorkerCore);
    if (created != pdPASS) {
        Serial.println("[printer] worker task creation failed");
        setOffline("printer_worker_start_failed");
        return;
    }

    started_ = true;
    SystemState& system = state();
    system.setupDone = settingsService().settings().setupDone;
    system.printerConfigured = configured();
    strlcpy(system.printerStatusText,
            system.printerConfigured ? "waiting_for_wifi" : "not_configured",
            sizeof(system.printerStatusText));
}

void PrinterService::loop() {
    if (!started_) return;
    consumeResults();

    SystemState& system = state();
    system.setupDone = settingsService().settings().setupDone;
    system.printerConfigured = configured();

    if (!system.printerConfigured) {
        consecutiveFailures_ = 0;
        setOffline("not_configured");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        consecutiveFailures_ = 0;
        setOffline("wifi_offline");
        return;
    }
    PrinterDiscoverySnapshot discovery;
    discoverySnapshot(discovery);
    if (discovery.status == PrinterDiscoveryStatus::Scanning) return;
    if (pollInFlight_) return;

    const uint32_t now = millis();
    const uint32_t interval = system.printerConnected
                                  ? ((system.printerState == PrinterState::Printing || system.printerState == PrinterState::Paused)
                                         ? kPollActiveMs
                                         : kPollIdleMs)
                                  : kPollOfflineMs;
    if (now - lastPollMs_ < interval) return;
    lastPollMs_ = now;

    if (enqueuePoll() && !system.printerConnected && consecutiveFailures_ == 0) {
        strlcpy(system.printerStatusText, "connecting", sizeof(system.printerStatusText));
    }
}

bool PrinterService::requestDiscovery() {
    if (!started_ || WiFi.status() != WL_CONNECTED || !discoveredPrinters_) {
        updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi is not connected");
        return false;
    }

    PrinterDiscoverySnapshot current;
    discoverySnapshot(current);
    if (current.status == PrinterDiscoveryStatus::Scanning) return false;

    portENTER_CRITICAL(&discoveryMux_);
    discoveryCount_ = 0;
    discoveryProgress_ = 0;
    discoveryStatus_ = PrinterDiscoveryStatus::Scanning;
    strlcpy(discoveryMessage_, "Starting printer discovery...", sizeof(discoveryMessage_));
    discoveryRevision_++;
    portEXIT_CRITICAL(&discoveryMux_);

    WorkerRequest request;
    request.type = WorkerJobType::Discover;
    if (xQueueSend(requestQueue_, &request, 0) != pdTRUE) {
        updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Printer service is busy. Try again.");
        return false;
    }
    Serial.println("[printer] discovery queued");
    return true;
}

void PrinterService::discoverySnapshot(PrinterDiscoverySnapshot& output) const {
    portENTER_CRITICAL(&discoveryMux_);
    output.status = discoveryStatus_;
    output.count = discoveryCount_;
    output.progress = discoveryProgress_;
    output.revision = discoveryRevision_;
    strlcpy(output.message, discoveryMessage_, sizeof(output.message));
    portEXIT_CRITICAL(&discoveryMux_);
}

bool PrinterService::discoveredPrinter(uint8_t index, DiscoveredPrinter& output) const {
    bool valid = false;
    portENTER_CRITICAL(&discoveryMux_);
    if (discoveredPrinters_ && index < discoveryCount_) {
        output = discoveredPrinters_[index];
        valid = true;
    }
    portEXIT_CRITICAL(&discoveryMux_);
    return valid;
}

PrinterTestResult PrinterService::testConnection() {
    PrinterTestResult result;
    lastTestMs_ = millis();

    PollRequest request;
    if (!captureRequest(request)) {
        strlcpy(result.message, "printer_not_configured", sizeof(result.message));
        setOffline(result.message);
        return result;
    }
    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(result.message, "wifi_offline", sizeof(result.message));
        setOffline(result.message);
        return result;
    }

    if (xSemaphoreTake(httpMutex_, pdMS_TO_TICKS(kHttpTimeoutMs + 250)) != pdTRUE) {
        strlcpy(result.message, "printer_busy", sizeof(result.message));
        return result;
    }
    result.ok = requestInfo(request, &result);
    xSemaphoreGive(httpMutex_);

    if (result.ok) {
        state().printerConnected = true;
        state().lastPrinterUpdateMs = millis();
        strlcpy(state().printerStatusText, "online", sizeof(state().printerStatusText));
        consecutiveFailures_ = 0;
        lastPollMs_ = 0;
    }
    return result;
}

bool PrinterService::configured() const {
    const AppSettings& cfg = settingsService().settings();
    return cfg.printerHost[0] != '\0' && cfg.printerPort > 0;
}

bool PrinterService::captureRequest(PollRequest& request) const {
    const AppSettings& cfg = settingsService().settings();
    if (!cfg.printerHost[0] || !cfg.printerPort) return false;

    request.settingsRevision = settingsService().revision();
    strlcpy(request.host, cfg.printerHost, sizeof(request.host));
    request.port = cfg.printerPort;
    strlcpy(request.apiKey, cfg.printerApiKey, sizeof(request.apiKey));
    return true;
}

bool PrinterService::enqueuePoll() {
    WorkerRequest request;
    request.type = WorkerJobType::Poll;
    if (!captureRequest(request.poll)) return false;
    if (xQueueSend(requestQueue_, &request, 0) != pdTRUE) return false;
    pollInFlight_ = true;
    return true;
}

void PrinterService::updateDiscovery(PrinterDiscoveryStatus status,
                                     uint8_t progress,
                                     const char* message) {
    portENTER_CRITICAL(&discoveryMux_);
    discoveryStatus_ = status;
    discoveryProgress_ = progress;
    strlcpy(discoveryMessage_, message ? message : "", sizeof(discoveryMessage_));
    discoveryRevision_++;
    portEXIT_CRITICAL(&discoveryMux_);
}

bool PrinterService::addDiscoveredPrinter(const char* host, uint16_t port, const char* name) {
    if (!host || !host[0] || !discoveredPrinters_) return false;
    bool added = false;
    portENTER_CRITICAL(&discoveryMux_);
    bool duplicate = false;
    for (uint8_t index = 0; index < discoveryCount_; ++index) {
        if (strncmp(discoveredPrinters_[index].host, host,
                    sizeof(discoveredPrinters_[index].host)) == 0) {
            duplicate = true;
            break;
        }
    }
    if (!duplicate && discoveryCount_ < MaxDiscoveredPrinters) {
        DiscoveredPrinter& printer = discoveredPrinters_[discoveryCount_++];
        strlcpy(printer.host, host, sizeof(printer.host));
        strlcpy(printer.name, name && name[0] ? name : "Moonraker printer", sizeof(printer.name));
        printer.port = port ? port : kDiscoveryPort;
        discoveryRevision_++;
        added = true;
    }
    portEXIT_CRITICAL(&discoveryMux_);
    return added;
}

bool PrinterService::probeMoonraker(const char* host, uint16_t port) {
    if (!host || !host[0] || WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    const String url = baseUrl(host, port) + "/printer/info";
    http.setConnectTimeout(kDiscoveryHttpTimeoutMs);
    http.setTimeout(kDiscoveryHttpTimeoutMs);
    if (!http.begin(url)) return false;
    const int code = http.GET();
    http.end();
    return code == 200;
}

void PrinterService::performDiscovery() {
    if (WiFi.status() != WL_CONNECTED) {
        updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi connection was lost");
        return;
    }

    updateDiscovery(PrinterDiscoveryStatus::Scanning, 2, "Checking Snapmaker discovery...");
    const int mdnsCount = MDNS.queryService("snapmaker", "tcp");
    for (int index = 0; index < mdnsCount; ++index) {
        const IPAddress address = MDNS.address(index);
        if (!address) continue;
        const String host = address.toString();
        if (!probeMoonraker(host.c_str(), kDiscoveryPort)) continue;
        String name = MDNS.instanceName(index);
        if (name.isEmpty()) name = MDNS.hostname(index);
        addDiscoveredPrinter(host.c_str(), kDiscoveryPort,
                             name.isEmpty() ? "Snapmaker" : name.c_str());
    }

    IPAddress local = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    uint8_t targets[kDiscoveryTargetLimit] = {};
    uint16_t targetCount = 0;
    auto addTarget = [&](int host) {
        if (host < 1 || host > 254 || targetCount >= kDiscoveryTargetLimit) return;
        for (uint16_t index = 0; index < targetCount; ++index) {
            if (targets[index] == host) return;
        }
        targets[targetCount++] = static_cast<uint8_t>(host);
    };

    const AppSettings& settings = settingsService().settings();
    IPAddress saved;
    if (saved.fromString(settings.printerHost) && saved[0] == local[0] &&
        saved[1] == local[1] && saved[2] == local[2]) {
        addTarget(saved[3]);
    }
    addTarget(gateway[3]);
    addTarget(local[3]);
    for (int distance = 1; distance <= 12; ++distance) {
        addTarget(static_cast<int>(local[3]) - distance);
        addTarget(static_cast<int>(local[3]) + distance);
    }
    static constexpr uint8_t CommonHosts[] = {
        2, 3, 4, 5, 6, 8, 10, 11, 12, 15, 20, 25, 30, 40, 50, 60, 75, 80, 100, 120, 150, 200,
    };
    for (uint8_t host : CommonHosts) addTarget(host);
    for (int host = 1; host <= 254; ++host) addTarget(host);

    char host[16] = "";
    for (uint16_t index = 0; index < targetCount; ++index) {
        if (WiFi.status() != WL_CONNECTED) {
            updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi connection was lost");
            return;
        }
        snprintf(host, sizeof(host), "%u.%u.%u.%u",
                 local[0], local[1], local[2], targets[index]);
        if (probeMoonraker(host, kDiscoveryPort)) {
            addDiscoveredPrinter(host, kDiscoveryPort, "Moonraker printer");
        }
        if ((index % 16) == 0 || index + 1 == targetCount) {
            const uint8_t progress = static_cast<uint8_t>(5 +
                (static_cast<uint16_t>(index + 1) * 94U / (targetCount ? targetCount : 1)));
            char message[72] = "";
            snprintf(message, sizeof(message), "Scanning local network... %u%%",
                     static_cast<unsigned>(progress));
            updateDiscovery(PrinterDiscoveryStatus::Scanning, progress, message);
        }
    }

    PrinterDiscoverySnapshot completed;
    discoverySnapshot(completed);
    char message[72] = "";
    if (completed.count > 0) {
        snprintf(message, sizeof(message), "Found %u compatible printer%s",
                 static_cast<unsigned>(completed.count), completed.count == 1 ? "" : "s");
    } else {
        strlcpy(message, "No printer found. You can enter it manually.", sizeof(message));
    }
    updateDiscovery(PrinterDiscoveryStatus::Complete, 100, message);
    Serial.printf("[printer] discovery complete found=%u\n", static_cast<unsigned>(completed.count));
}

void PrinterService::consumeResults() {
    PollResult result;
    while (xQueueReceive(resultQueue_, &result, 0) == pdTRUE) {
        pollInFlight_ = false;
        if (result.settingsRevision != settingsService().revision()) {
            lastPollMs_ = 0;
            continue;
        }
        applyResult(result);
    }
}

void PrinterService::applyResult(const PollResult& result) {
    if (!result.ok) {
        if (consecutiveFailures_ < UINT8_MAX) consecutiveFailures_++;
        if (consecutiveFailures_ >= kFailuresBeforeOffline) {
            setOffline(result.message[0] ? result.message : "poll_failed", result.httpCode);
        } else if (!state().printerConnected) {
            snprintf(state().printerStatusText,
                     sizeof(state().printerStatusText),
                     "connecting_retry_%u",
                     static_cast<unsigned>(consecutiveFailures_));
        }
        return;
    }

    consecutiveFailures_ = 0;
    SystemState& system = state();
    system.printerState = static_cast<PrinterState>(result.printerState);
    system.printProgress = result.printProgress;
    system.activeTool = result.activeTool;
    system.activeToolTempC = result.activeToolTempC;
    system.bedTempC = result.bedTempC;
    system.chamberTempC = result.chamberTempC;
    system.printerConnected = true;
    system.lastPrinterUpdateMs = millis();
    strlcpy(system.printFilename, result.filename, sizeof(system.printFilename));
    strlcpy(system.printerStatusText, printerStateName(system.printerState), sizeof(system.printerStatusText));
}

bool PrinterService::requestInfo(const PollRequest& request, PrinterTestResult* result) {
    HTTPClient http;
    const String url = baseUrl(request.host, request.port) + "/printer/info";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(url)) {
        if (result) strlcpy(result->message, "http_begin_failed", sizeof(result->message));
        return false;
    }
    addAuthHeader(http, request.apiKey);

    const int code = http.GET();
    if (result) result->httpCode = code;
    http.end();
    if (code == 200) {
        if (result) strlcpy(result->message, "moonraker_online", sizeof(result->message));
        return true;
    }
    if (result) snprintf(result->message, sizeof(result->message), "moonraker_http_%d", code);
    return false;
}

bool PrinterService::performPoll(const PollRequest& request, PollResult& result) {
    result.settingsRevision = request.settingsRevision;
    HTTPClient http;
    const String url = baseUrl(request.host, request.port) + "/printer/objects/query";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(url)) {
        strlcpy(result.message, "http_begin_failed", sizeof(result.message));
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    addAuthHeader(http, request.apiKey);

    static constexpr char Body[] =
        "{\"objects\":{"
        "\"print_stats\":[\"state\",\"filename\",\"print_duration\",\"total_duration\"],"
        "\"display_status\":[\"progress\"],"
        "\"toolhead\":[\"extruder\"],"
        "\"extruder\":[\"temperature\"],"
        "\"extruder1\":[\"temperature\"],"
        "\"extruder2\":[\"temperature\"],"
        "\"extruder3\":[\"temperature\"],"
        "\"heater_bed\":[\"temperature\"],"
        "\"temperature_sensor cavity\":[\"temperature\"]}}";

    const int code = http.POST(Body);
    result.httpCode = code;
    if (code != 200) {
        snprintf(result.message, sizeof(result.message), "poll_http_%d", code);
        http.end();
        return false;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    if (error) {
        strlcpy(result.message, "poll_invalid_json", sizeof(result.message));
        return false;
    }

    const JsonVariantConst status = doc["result"]["status"];
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

    result.ok = true;
    result.printerState = static_cast<uint8_t>(normalizePrinterState(rawState));
    result.printProgress = clampProgress(progress);
    result.activeTool = tool;
    result.activeToolTempC = toolTemp;
    result.bedTempC = status["heater_bed"]["temperature"] | NAN;
    result.chamberTempC = status["temperature_sensor cavity"]["temperature"] | NAN;
    strlcpy(result.filename, filename, sizeof(result.filename));
    strlcpy(result.message, "ok", sizeof(result.message));
    return true;
}

void PrinterService::setOffline(const char* message, int httpCode) {
    SystemState& system = state();
    system.printerConnected = false;
    system.printerState = configured() ? PrinterState::Unknown : PrinterState::Idle;
    if (httpCode) {
        snprintf(system.printerStatusText,
                 sizeof(system.printerStatusText),
                 "%s_%d",
                 message ? message : "offline",
                 httpCode);
    } else {
        strlcpy(system.printerStatusText, message ? message : "offline", sizeof(system.printerStatusText));
    }
}

void PrinterService::workerTaskEntry(void* context) {
    static_cast<PrinterService*>(context)->workerLoop();
}

void PrinterService::workerLoop() {
    WorkerRequest request;
    for (;;) {
        if (xQueueReceive(requestQueue_, &request, portMAX_DELAY) != pdTRUE) continue;

        if (request.type == WorkerJobType::Discover) {
            if (xSemaphoreTake(httpMutex_, portMAX_DELAY) == pdTRUE) {
                performDiscovery();
                xSemaphoreGive(httpMutex_);
            } else {
                updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Printer discovery failed");
            }
            continue;
        }

        PollResult result;
        result.settingsRevision = request.poll.settingsRevision;
        if (xSemaphoreTake(httpMutex_, portMAX_DELAY) == pdTRUE) {
            result.ok = performPoll(request.poll, result);
            xSemaphoreGive(httpMutex_);
        } else {
            strlcpy(result.message, "http_mutex_failed", sizeof(result.message));
        }
        xQueueOverwrite(resultQueue_, &result);
    }
}

}
