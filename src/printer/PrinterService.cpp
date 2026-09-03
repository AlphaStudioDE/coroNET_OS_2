#include "PrinterService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <mdns.h>

#include "../config/AppConfig.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"
#include "../wifi/WifiService.h"

namespace coronet {

namespace {

constexpr uint32_t kPollIdleMs = 5000;
constexpr uint32_t kPollActiveMs = 2000;
constexpr uint32_t kPollOfflineMs = 8000;
constexpr uint32_t kPollRealtimeAuditMs = 30000;
constexpr uint32_t kHttpTimeoutMs = 700;
constexpr uint8_t kFailuresBeforeOffline = 3;
constexpr uint32_t kWorkerStackBytes = 6144;
constexpr UBaseType_t kWorkerPriority = 2;
constexpr BaseType_t kWorkerCore = 0;
constexpr uint16_t kDiscoveryPort = 7125;
constexpr uint32_t kDiscoveryTcpTimeoutMs = 140;
constexpr uint32_t kDiscoveryHttpTimeoutMs = 900;
constexpr uint32_t kDiscoveryMdnsReadyTimeoutMs = 1800;
constexpr uint32_t kDiscoveryMdnsQueryTimeoutMs = 1200;
constexpr uint16_t kQuickDiscoveryTargetLimit = 60;
constexpr uint16_t kDiscoveryTargetLimit = 254;
constexpr uint32_t kRealtimeWorkerTickMs = 10;
constexpr uint32_t kRealtimeReconnectMs = 5000;
constexpr uint32_t kRealtimeProbeTimeoutMs = 600;
constexpr uint32_t kRealtimeHandshakeTimeoutMs = 1500;
constexpr uint32_t kRealtimeRequestTimeoutMs = 3000;
constexpr uint32_t kRealtimeStaleMs = 15000;
constexpr uint32_t kChamberFilterTimeConstantMs = 6000;
constexpr uint32_t kChamberFilterResetGapMs = 30000;

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
    if (!stateText || !*stateText) return PrinterState::Unknown;
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

bool parseFilamentColor(const char* text, uint32_t& output) {
    if (!text) return false;
    while (*text == '#' || *text == ' ') ++text;
    if (strlen(text) < 6) return false;
    char rgb[7] = {};
    memcpy(rgb, text, 6);
    char* end = nullptr;
    const unsigned long value = strtoul(rgb, &end, 16);
    if (end != rgb + 6) return false;
    output = static_cast<uint32_t>(value);
    return true;
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

void normalizePrinterHost(const char* input, char output[65]) {
    String host = input ? input : "";
    host.trim();
    if (host.startsWith("http://")) host.remove(0, 7);
    else if (host.startsWith("https://")) host.remove(0, 8);
    const int slash = host.indexOf('/');
    if (slash >= 0) host.remove(slash);
    host.trim();
    strlcpy(output, host.c_str(), 65);
}

bool payloadContains(const uint8_t* payload, size_t length, const char* literal) {
    if (!payload || !literal) return false;
    const size_t literalLength = strlen(literal);
    if (!literalLength || length < literalLength) return false;
    for (size_t index = 0; index + literalLength <= length; ++index) {
        if (memcmp(payload + index, literal, literalLength) == 0) return true;
    }
    return false;
}

bool objectListContains(JsonArrayConst objects, const char* name) {
    if (objects.isNull() || !name || !name[0]) return false;
    for (JsonVariantConst value : objects) {
        const char* objectName = value.as<const char*>();
        if (objectName && strcmp(objectName, name) == 0) return true;
    }
    return false;
}

bool isNewerSequence(uint32_t candidate, uint32_t reference) {
    return reference == 0 || static_cast<int32_t>(candidate - reference) > 0;
}

String mdnsTxtValueByKey(const mdns_result_t* result, const char* key) {
    if (!result || !key || !key[0]) return String();
    for (size_t index = 0; index < result->txt_count; ++index) {
        const char* candidateKey = result->txt[index].key;
        if (candidateKey && strcasecmp(candidateKey, key) == 0) {
            String value(result->txt[index].value ? result->txt[index].value : "");
            value.trim();
            return value;
        }
    }
    return String();
}

}

PrinterService& printerService() {
    return gPrinterService;
}

void PrinterService::begin() {
    requestQueue_ = xQueueCreate(3, sizeof(WorkerRequest));
    resultQueue_ = xQueueCreate(1, sizeof(PollResult));
    realtimeResultQueue_ = xQueueCreate(1, sizeof(PollResult));
    httpMutex_ = xSemaphoreCreateMutex();
    discoveredPrinters_ = static_cast<DiscoveredPrinter*>(heap_caps_calloc(
        MaxDiscoveredPrinters, sizeof(DiscoveredPrinter), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!requestQueue_ || !resultQueue_ || !realtimeResultQueue_ || !httpMutex_ ||
        !discoveredPrinters_) {
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
    observedSettingsRevision_ = settingsService().revision();
    const AppSettings& settings = settingsService().settings();
    normalizePrinterHost(settings.printerHost, configuredHost_);
    configuredPort_ = settings.printerPort ? settings.printerPort : 7125;
    strlcpy(configuredApiKey_, settings.printerApiKey, sizeof(configuredApiKey_));
    enqueueConfiguration();
    lastPollMs_ = millis() - kPollOfflineMs;
    SystemState& system = state();
    system.setupDone = settingsService().settings().setupDone;
    system.printerConfigured = configured();
    strlcpy(system.printerStatusText,
            system.printerConfigured ? "waiting_for_wifi" : "not_configured",
            sizeof(system.printerStatusText));
}

void PrinterService::logStatus() const {
    const UBaseType_t stackHeadroom = workerTask_ ? uxTaskGetStackHighWaterMark(workerTask_) : 0;
    Serial.printf("[printer] ready=%u configured=%u connected=%u telemetry=%u ws=%u/%u released=%u stackHeadroom=%uB failures=%u\n",
                  started_ ? 1U : 0U,
                  state().printerConfigured ? 1U : 0U,
                  state().printerConnected ? 1U : 0U,
                  state().printerTelemetryValid ? 1U : 0U,
                  realtimeConnected_ ? 1U : 0U,
                  realtimeSubscribed_ ? 1U : 0U,
                  realtimeResourcesReleased_ ? 1U : 0U,
                  static_cast<unsigned>(stackHeadroom),
                  static_cast<unsigned>(consecutiveFailures_));
}

void PrinterService::loop() {
    if (!started_) return;
    refreshConfiguration();
    if (queuedConfigRevision_ != printerConfigRevision_) enqueueConfiguration();
    consumeResults();
    if (state().maintenanceMode || state().otaTlsWindowActive) return;

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
    const uint32_t interval = realtimeSubscribed_ && realtimeFullTelemetry_
                                  ? kPollRealtimeAuditMs
                                  : (system.printerConnected
                                         ? ((system.printerState == PrinterState::Printing ||
                                             system.printerState == PrinterState::Paused)
                                                ? kPollActiveMs
                                                : kPollIdleMs)
                                         : kPollOfflineMs);
    if (now - lastPollMs_ < interval) return;
    lastPollMs_ = now;

    if (enqueuePoll() && !system.printerConnected && consecutiveFailures_ == 0) {
        strlcpy(system.printerStatusText, "connecting", sizeof(system.printerStatusText));
    }
}

bool PrinterService::requestDiscovery() {
    if (!started_ || state().maintenanceMode || state().otaTlsWindowActive ||
        WiFi.status() != WL_CONNECTED || !discoveredPrinters_) {
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
    refreshConfiguration();

    PollRequest request;
    if (state().maintenanceMode || state().otaTlsWindowActive) {
        strlcpy(result.message, "maintenance_mode", sizeof(result.message));
        return result;
    }
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
        setConnectionState(true);
        if (!state().printerTelemetryValid) {
            state().printerState = PrinterState::Unknown;
            strlcpy(state().printerStatusText, "online_waiting_for_telemetry",
                    sizeof(state().printerStatusText));
        }
        consecutiveFailures_ = 0;
        lastPollMs_ = 0;
    }
    return result;
}

bool PrinterService::configured() const {
    return configuredHost_[0] != '\0' && configuredPort_ > 0;
}

bool PrinterService::captureRequest(PollRequest& request) const {
    if (!configuredHost_[0] || !configuredPort_) return false;
    request.settingsRevision = printerConfigRevision_;
    strlcpy(request.host, configuredHost_, sizeof(request.host));
    request.port = configuredPort_;
    strlcpy(request.apiKey, configuredApiKey_, sizeof(request.apiKey));
    return true;
}

void PrinterService::refreshConfiguration() {
    const uint32_t settingsRevision = settingsService().revision();
    if (settingsRevision == observedSettingsRevision_) return;
    observedSettingsRevision_ = settingsRevision;

    const AppSettings& settings = settingsService().settings();
    char normalizedHost[sizeof(configuredHost_)] = "";
    normalizePrinterHost(settings.printerHost, normalizedHost);
    const uint16_t port = settings.printerPort ? settings.printerPort : 7125;
    if (strcmp(normalizedHost, configuredHost_) == 0 && port == configuredPort_ &&
        strcmp(settings.printerApiKey, configuredApiKey_) == 0) {
        return;
    }

    strlcpy(configuredHost_, normalizedHost, sizeof(configuredHost_));
    configuredPort_ = port;
    strlcpy(configuredApiKey_, settings.printerApiKey, sizeof(configuredApiKey_));
    printerConfigRevision_++;
    queuedConfigRevision_ = 0;
    consecutiveFailures_ = 0;
    lastPollMs_ = millis() - kPollOfflineMs;
    enqueueConfiguration();
    setOffline(configured() ? "printer_config_changed" : "not_configured");
}

bool PrinterService::enqueueConfiguration() {
    if (!requestQueue_ || queuedConfigRevision_ == printerConfigRevision_) return true;
    WorkerRequest request;
    request.type = WorkerJobType::Configure;
    request.poll.settingsRevision = printerConfigRevision_;
    strlcpy(request.poll.host, configuredHost_, sizeof(request.poll.host));
    request.poll.port = configuredPort_;
    strlcpy(request.poll.apiKey, configuredApiKey_, sizeof(request.poll.apiKey));
    if (xQueueSend(requestQueue_, &request, 0) != pdTRUE) return false;
    queuedConfigRevision_ = printerConfigRevision_;
    return true;
}

bool PrinterService::enqueuePoll() {
    if (queuedConfigRevision_ != printerConfigRevision_ && !enqueueConfiguration()) return false;
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
    if (!host || !host[0] || WiFi.status() != WL_CONNECTED || state().maintenanceMode ||
        state().otaTlsWindowActive) return false;

    WiFiClient portProbe;
    if (!portProbe.connect(host, port, kDiscoveryTcpTimeoutMs)) return false;
    portProbe.stop();

    HTTPClient http;
    const String url = baseUrl(host, port) + "/printer/info";
    http.setConnectTimeout(kDiscoveryHttpTimeoutMs);
    http.setTimeout(kDiscoveryHttpTimeoutMs);
    if (!http.begin(url)) return false;
    const int code = http.GET();
    http.end();
    return code == 200;
}

uint8_t PrinterService::discoverMdnsService(const char* service,
                                            const char* defaultName,
                                            bool useAdvertisedPort) {
    if (!service || !service[0] || WiFi.status() != WL_CONNECTED) return 0;
    if (!wifiService().acquireMdns(kDiscoveryMdnsReadyTimeoutMs, pdMS_TO_TICKS(250))) {
        Serial.printf("[printer] mDNS unavailable for _%s._tcp\n", service);
        return 0;
    }

    char serviceType[34] = "_";
    strlcpy(serviceType + 1, service, sizeof(serviceType) - 1);
    mdns_result_t* results = nullptr;
    const esp_err_t queryResult = mdns_query_ptr(serviceType, "_tcp",
                                                  kDiscoveryMdnsQueryTimeoutMs,
                                                  MaxDiscoveredPrinters, &results);
    uint8_t added = 0;
    uint8_t resultCount = 0;
    for (mdns_result_t* result = results; result; result = result->next) ++resultCount;
    Serial.printf("[printer] mDNS _%s._tcp status=%d results=%u\n",
                  service, static_cast<int>(queryResult), static_cast<unsigned>(resultCount));
    for (mdns_result_t* result = results; result; result = result->next) {
        String host = mdnsTxtValueByKey(result, "ip");
        IPAddress parsedAddress;
        if (host.isEmpty() || host == "0.0.0.0" || !parsedAddress.fromString(host)) {
            IPAddress address;
            for (mdns_ip_addr_t* item = result->addr; item; item = item->next) {
                if (item->addr.type == ESP_IPADDR_TYPE_V4) {
                    address = IPAddress(item->addr.u_addr.ip4.addr);
                    break;
                }
            }
            if (!address || address == IPAddress(0, 0, 0, 0)) continue;
            host = address.toString();
        }

        String deviceName = mdnsTxtValueByKey(result, "device_name");
        String machineType = mdnsTxtValueByKey(result, "machine_type");
        String name = deviceName;
        if (name.isEmpty()) name = machineType;
        if (name.isEmpty() && result->instance_name) name = result->instance_name;
        if (name.isEmpty() && result->hostname) name = result->hostname;
        if (name.isEmpty()) name = defaultName ? defaultName : "Moonraker printer";
        if (!deviceName.isEmpty() && !machineType.isEmpty() && deviceName != machineType) {
            name += " - ";
            name += machineType;
        }

        const uint16_t port = useAdvertisedPort && result->port ? result->port : kDiscoveryPort;
        if (addDiscoveredPrinter(host.c_str(), port, name.c_str())) {
            ++added;
            Serial.printf("[printer] mDNS found name=%s host=%s port=%u\n",
                          name.c_str(), host.c_str(), static_cast<unsigned>(port));
        }
    }
    if (results) mdns_query_results_free(results);
    wifiService().releaseMdns();
    return added;
}

void PrinterService::performDiscovery() {
    const uint32_t discoveryStartedMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
        updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi connection was lost");
        return;
    }

    auto completeDiscovery = [&]() {
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
        Serial.printf("[printer] discovery complete found=%u elapsed=%lums\n",
                      static_cast<unsigned>(completed.count),
                      static_cast<unsigned long>(millis() - discoveryStartedMs));
    };

    const AppSettings& settings = settingsService().settings();
    if (settings.printerHost[0] && settings.printerPort) {
        updateDiscovery(PrinterDiscoveryStatus::Scanning, 2, "Checking saved printer...");
        if (probeMoonraker(settings.printerHost, settings.printerPort)) {
            addDiscoveredPrinter(settings.printerHost, settings.printerPort, "Configured printer");
            Serial.printf("[printer] saved printer reachable host=%s port=%u\n",
                          settings.printerHost, static_cast<unsigned>(settings.printerPort));
            completeDiscovery();
            return;
        }
    }

    updateDiscovery(PrinterDiscoveryStatus::Scanning, 6, "Searching local printer services...");
    discoverMdnsService("snapmaker", "Snapmaker", false);

    PrinterDiscoverySnapshot afterFastServices;
    discoverySnapshot(afterFastServices);
    if (afterFastServices.count > 0) {
        completeDiscovery();
        return;
    }
    discoverMdnsService("moonraker", "Moonraker printer", true);
    discoverySnapshot(afterFastServices);
    if (afterFastServices.count > 0) {
        completeDiscovery();
        return;
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
    for (int host = 2; host <= 60 && targetCount < kQuickDiscoveryTargetLimit; ++host) {
        addTarget(host);
    }

    char host[16] = "";
    updateDiscovery(PrinterDiscoveryStatus::Scanning, 15, "Running quick local scan...");
    for (uint16_t index = 0; index < targetCount; ++index) {
        if (WiFi.status() != WL_CONNECTED || state().maintenanceMode ||
            state().otaTlsWindowActive) {
            updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi connection was lost");
            return;
        }
        snprintf(host, sizeof(host), "%u.%u.%u.%u",
                 local[0], local[1], local[2], targets[index]);
        if (probeMoonraker(host, kDiscoveryPort)) {
            addDiscoveredPrinter(host, kDiscoveryPort, "Moonraker printer");
        }
        if ((index % 6) == 0 || index + 1 == targetCount) {
            const uint8_t progress = static_cast<uint8_t>(15 +
                (static_cast<uint16_t>(index + 1) * 30U / (targetCount ? targetCount : 1)));
            char message[72] = "";
            snprintf(message, sizeof(message), "Quick local scan... %u%%",
                     static_cast<unsigned>(progress));
            updateDiscovery(PrinterDiscoveryStatus::Scanning, progress, message);
        }
    }

    PrinterDiscoverySnapshot afterQuickScan;
    discoverySnapshot(afterQuickScan);
    if (afterQuickScan.count > 0) {
        completeDiscovery();
        return;
    }

    const uint16_t quickTargetCount = targetCount;
    for (int address = 1; address <= 254; ++address) addTarget(address);
    updateDiscovery(PrinterDiscoveryStatus::Scanning, 46, "Quick scan empty. Running full scan...");
    for (uint16_t index = quickTargetCount; index < targetCount; ++index) {
        if (WiFi.status() != WL_CONNECTED || state().maintenanceMode ||
            state().otaTlsWindowActive) {
            updateDiscovery(PrinterDiscoveryStatus::Failed, 0, "Wi-Fi connection was lost");
            return;
        }
        snprintf(host, sizeof(host), "%u.%u.%u.%u",
                 local[0], local[1], local[2], targets[index]);
        if (probeMoonraker(host, kDiscoveryPort)) {
            addDiscoveredPrinter(host, kDiscoveryPort, "Moonraker printer");
        }
        if (((index - quickTargetCount) % 12) == 0 || index + 1 == targetCount) {
            const uint16_t fullCount = targetCount - quickTargetCount;
            const uint16_t fullDone = index - quickTargetCount + 1;
            const uint8_t progress = static_cast<uint8_t>(46 + fullDone * 53U /
                (fullCount ? fullCount : 1));
            char message[72] = "";
            snprintf(message, sizeof(message), "Full local scan... %u%%",
                     static_cast<unsigned>(progress));
            updateDiscovery(PrinterDiscoveryStatus::Scanning, progress, message);
        }
    }

    completeDiscovery();
}

void PrinterService::consumeResults() {
    PollResult pending[2];
    uint8_t count = 0;
    if (xQueueReceive(resultQueue_, &pending[count], 0) == pdTRUE) {
        pollInFlight_ = false;
        count++;
    }
    if (xQueueReceive(realtimeResultQueue_, &pending[count], 0) == pdTRUE) count++;
    if (count == 2 && isNewerSequence(pending[0].sequence, pending[1].sequence)) {
        const PollResult swap = pending[0];
        pending[0] = pending[1];
        pending[1] = swap;
    }
    for (uint8_t index = 0; index < count; ++index) {
        const PollResult& result = pending[index];
        if (result.settingsRevision != printerConfigRevision_ ||
            !isNewerSequence(result.sequence, lastAppliedResultSequence_)) {
            if (result.settingsRevision != printerConfigRevision_) lastPollMs_ = 0;
            continue;
        }
        lastAppliedResultSequence_ = result.sequence;
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
    const PrinterState nextState = static_cast<PrinterState>(result.printerState);
    const bool hadContinuousTelemetry = system.printerConnected && system.printerTelemetryValid;
    const PrinterState previousState = system.printerState;

    system.printProgress = result.printProgress;
    system.activeTool = result.activeTool;
    system.activeToolTempC = result.activeToolTempC;
    system.bedTempC = result.bedTempC;
    system.chamberTempC = result.chamberTempC;
    system.printDurationSec = result.printDurationSec;
    system.printEtaSec = result.printEtaSec;
    system.filamentColorRgb = result.filamentColorRgb;
    memcpy(system.filamentColorsRgb, result.filamentColorsRgb,
           sizeof(system.filamentColorsRgb));
    system.filamentColorMask = result.filamentColorMask;
    setConnectionState(true);
    const uint32_t now = millis();
    system.printerState = nextState;
    system.printerTelemetryValid = true;
    system.lastPrinterUpdateMs = now;
    system.printerTelemetryRevision++;
    strlcpy(system.printFilename, result.filename, sizeof(system.printFilename));
    strlcpy(system.materialName, result.material, sizeof(system.materialName));
    strlcpy(system.printerStatusText, printerStateName(system.printerState), sizeof(system.printerStatusText));

    if (hadContinuousTelemetry && previousState != PrinterState::Unknown &&
        nextState != PrinterState::Unknown && previousState != nextState) {
        system.printerEventFrom = previousState;
        system.printerEventTo = nextState;
        system.printerStateChangedMs = now;
        system.printerStateEventSequence++;
    }
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
        "\"print_stats\":[\"state\",\"filename\",\"print_duration\"],"
        "\"display_status\":[\"progress\"],"
        "\"toolhead\":[\"extruder\"],"
        "\"extruder\":[\"temperature\"],"
        "\"extruder1\":[\"temperature\"],"
        "\"extruder2\":[\"temperature\"],"
        "\"extruder3\":[\"temperature\"],"
        "\"heater_bed\":[\"temperature\"],"
        "\"temperature_sensor cavity\":[\"temperature\"],"
        "\"print_task_config\":[\"filament_color_rgba\",\"filament_type\"]}}";

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
    if (!status.is<JsonObjectConst>() || !status["print_stats"].is<JsonObjectConst>() ||
        !status["print_stats"]["state"].is<const char*>()) {
        strlcpy(result.message, "poll_missing_printer_state", sizeof(result.message));
        return false;
    }

    const char* rawState = status["print_stats"]["state"].as<const char*>();
    if (!rawState || !rawState[0]) {
        strlcpy(result.message, "poll_empty_printer_state", sizeof(result.message));
        return false;
    }
    const char* filename = status["print_stats"]["filename"] | "-";
    const float progress = status["display_status"]["progress"] | 0.0f;
    const char* extruder = status["toolhead"]["extruder"] | "extruder";
    const uint8_t tool = toolIndexFromObject(extruder);
    const float duration = status["print_stats"]["print_duration"] | 0.0f;

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
    const float rawChamberTempC = status["temperature_sensor cavity"]["temperature"] | NAN;
    result.chamberTempC = filterChamberTemperature(rawChamberTempC);
    result.printDurationSec = duration > 0.0f ? static_cast<uint32_t>(duration + 0.5f) : 0;
    if (progress > 0.001f && progress < 1.0f && duration > 0.0f) {
        const double eta = static_cast<double>(duration) / static_cast<double>(progress) - duration;
        result.printEtaSec = isfinite(eta) && eta > 0.0 && eta <= static_cast<double>(UINT32_MAX)
                                 ? static_cast<uint32_t>(eta + 0.5)
                                 : 0;
    }
    JsonArrayConst colors = status["print_task_config"]["filament_color_rgba"].as<JsonArrayConst>();
    if (!colors.isNull()) {
        for (uint8_t index = 0; index < 4U && index < colors.size(); ++index) {
            uint32_t color = 0;
            if (parseFilamentColor(colors[index] | "", color)) {
                result.filamentColorsRgb[index] = color;
                result.filamentColorMask |= static_cast<uint8_t>(1U << index);
            }
        }
        if (tool < 4U && (result.filamentColorMask & (1U << tool))) {
            result.filamentColorRgb = result.filamentColorsRgb[tool];
        }
    }
    JsonArrayConst materials = status["print_task_config"]["filament_type"].as<JsonArrayConst>();
    const char* material = (!materials.isNull() && tool < materials.size()) ? (materials[tool] | "-") : "-";
    strlcpy(result.filename, filename, sizeof(result.filename));
    strlcpy(result.material, material, sizeof(result.material));
    strlcpy(result.message, "ok", sizeof(result.message));
    return true;
}

void PrinterService::configureRealtime(const PollRequest& request) {
    const bool unchanged = workerConfigValid_ &&
                           request.settingsRevision == workerConfig_.settingsRevision &&
                           request.port == workerConfig_.port &&
                           strcmp(request.host, workerConfig_.host) == 0 &&
                           strcmp(request.apiKey, workerConfig_.apiKey) == 0;
    if (unchanged) return;

    releaseRealtime("printer configuration changed");
    workerConfig_ = request;
    workerConfigValid_ = request.host[0] != '\0' && request.port > 0;
    workerSnapshot_ = PollResult{};
    workerSnapshot_.settingsRevision = request.settingsRevision;
    workerSnapshot_.filamentColorRgb = 0xFFFFFF;
    strlcpy(workerSnapshot_.filename, "-", sizeof(workerSnapshot_.filename));
    strlcpy(workerSnapshot_.material, "-", sizeof(workerSnapshot_.material));
    memset(workerToolMaterials_, 0, sizeof(workerToolMaterials_));
    for (float& temperature : workerToolTemperatures_) temperature = NAN;
    resetChamberFilter();
    strlcpy(workerChamberObject_, "temperature_sensor cavity", sizeof(workerChamberObject_));
    workerSnapshotValid_ = false;
    webSocketLastConnectTryMs_ = millis() - kRealtimeReconnectMs;
}

void PrinterService::releaseRealtime(const char* reason) {
    const bool wasActive = webSocketProbeSocket_ >= 0 || webSocketHandshakePending_ ||
                           realtimeConnected_ || realtimeSubscribed_ ||
                           webSocketServerInfoPending_ || webSocketObjectListPending_ ||
                           webSocketSubscribePending_;
    closeRealtimeProbe();
    webSocket_.disconnect();
    realtimeConnected_ = false;
    realtimeSubscribed_ = false;
    realtimeFullTelemetry_ = false;
    webSocketHandshakePending_ = false;
    webSocketServerInfoPending_ = false;
    webSocketObjectListPending_ = false;
    webSocketSubscribePending_ = false;
    webSocketKlippyReady_ = false;
    webSocketCoreFallbackAttempted_ = false;
    webSocketAbortRequested_ = false;
    webSocketFullSubscriptionRequested_ = false;
    webSocketLastMessageMs_ = 0;
    realtimeResourcesReleased_ = true;
    if (wasActive && reason && reason[0]) Serial.printf("[printer-ws] released: %s\n", reason);
}

bool PrinterService::startRealtimeProbe() {
    if (webSocketProbeSocket_ >= 0 || !workerConfigValid_) return webSocketProbeSocket_ >= 0;

    IPAddress address;
    if (!address.fromString(workerConfig_.host)) {
        if (WiFi.hostByName(workerConfig_.host, address) != 1 || !address) return false;
    }
    const String resolved = address.toString();
    strlcpy(webSocketConnectHost_, resolved.c_str(), sizeof(webSocketConnectHost_));

    sockaddr_in endpoint = {};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(workerConfig_.port);
    if (!inet_aton(webSocketConnectHost_, &endpoint.sin_addr)) return false;

    const int socket = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket < 0) return false;
    const int flags = lwip_fcntl(socket, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        lwip_close(socket);
        return false;
    }
    const int connected = lwip_connect(socket, reinterpret_cast<sockaddr*>(&endpoint),
                                       sizeof(endpoint));
    if (connected < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
        lwip_close(socket);
        return false;
    }
    webSocketProbeSocket_ = socket;
    webSocketProbeStartedMs_ = millis();
    realtimeResourcesReleased_ = false;
    return true;
}

int PrinterService::checkRealtimeProbe() const {
    if (webSocketProbeSocket_ < 0) return -1;
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(webSocketProbeSocket_, &writeSet);
    FD_SET(webSocketProbeSocket_, &errorSet);
    timeval timeout = {0, 0};
    const int selected = lwip_select(webSocketProbeSocket_ + 1, nullptr, &writeSet,
                                     &errorSet, &timeout);
    if (selected > 0 && (FD_ISSET(webSocketProbeSocket_, &writeSet) ||
                         FD_ISSET(webSocketProbeSocket_, &errorSet))) {
        int socketError = 0;
        socklen_t length = sizeof(socketError);
        if (lwip_getsockopt(webSocketProbeSocket_, SOL_SOCKET, SO_ERROR,
                            &socketError, &length) != 0 || socketError != 0) {
            return -1;
        }
        return 1;
    }
    if (millis() - webSocketProbeStartedMs_ >= kRealtimeProbeTimeoutMs) return -1;
    return 0;
}

void PrinterService::closeRealtimeProbe() {
    if (webSocketProbeSocket_ >= 0) {
        lwip_close(webSocketProbeSocket_);
        webSocketProbeSocket_ = -1;
    }
}

void PrinterService::beginRealtimeHandshake() {
    closeRealtimeProbe();
    webSocket_.disconnect();
    webSocket_.begin(webSocketConnectHost_, workerConfig_.port, "/websocket", "");
    if (workerConfig_.apiKey[0]) {
        String header = "X-Api-Key: ";
        header += workerConfig_.apiKey;
        webSocket_.setExtraHeaders(header.c_str());
    } else {
        webSocket_.setExtraHeaders("");
    }
    webSocket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleRealtimeEvent(type, payload, length);
    });
    webSocket_.setReconnectInterval(0);
    webSocket_.enableHeartbeat(0, 0, 0);
    webSocketHandshakePending_ = true;
    webSocketHandshakeStartedMs_ = millis();
    webSocketLastConnectTryMs_ = webSocketHandshakeStartedMs_;
    realtimeResourcesReleased_ = false;
}

void PrinterService::requestRealtimeServerInfo() {
    static constexpr char Request[] =
        "{\"jsonrpc\":\"2.0\",\"method\":\"server.info\",\"id\":100}";
    webSocketServerInfoPending_ = webSocket_.sendTXT(Request);
    webSocketRequestStartedMs_ = millis();
}

void PrinterService::requestRealtimeObjectList() {
    static constexpr char Request[] =
        "{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.list\",\"id\":103}";
    webSocketObjectListPending_ = webSocket_.sendTXT(Request);
    webSocketRequestStartedMs_ = millis();
}

void PrinterService::subscribeRealtime(JsonArrayConst objects) {
    if (!realtimeConnected_) return;
    const bool detectedObjects = !objects.isNull();
    webSocketFullSubscriptionRequested_ = detectedObjects;
    realtimeFullTelemetry_ = false;
    char previousChamberObject[sizeof(workerChamberObject_)] = "";
    strlcpy(previousChamberObject, workerChamberObject_, sizeof(previousChamberObject));
    workerChamberObject_[0] = '\0';

    if (detectedObjects) {
        static constexpr const char* PreferredChamberObjects[] = {
            "temperature_sensor cavity",
            "temperature_sensor chamber",
            "temperature_sensor enclosure",
            "temperature_sensor chamber_temp",
            "temperature_sensor enclosure_temp",
        };
        for (const char* candidate : PreferredChamberObjects) {
            if (objectListContains(objects, candidate)) {
                strlcpy(workerChamberObject_, candidate, sizeof(workerChamberObject_));
                break;
            }
        }
        if (!workerChamberObject_[0]) {
            for (JsonVariantConst value : objects) {
                const char* candidate = value.as<const char*>();
                if (!candidate) continue;
                String lower(candidate);
                lower.toLowerCase();
                const bool temperatureObject = lower.startsWith("temperature_sensor ") ||
                                               lower.startsWith("temperature_fan ");
                const bool chamberAlias = lower.indexOf("cavity") >= 0 ||
                                          lower.indexOf("chamber") >= 0 ||
                                          lower.indexOf("enclosure") >= 0;
                if (temperatureObject && chamberAlias) {
                    strlcpy(workerChamberObject_, candidate, sizeof(workerChamberObject_));
                    break;
                }
            }
        }
    }
    if (strcmp(previousChamberObject, workerChamberObject_) != 0) resetChamberFilter();

    JsonDocument document;
    document["jsonrpc"] = "2.0";
    document["method"] = "printer.objects.subscribe";
    document["id"] = 101;
    JsonObject subscribed = document["params"]["objects"].to<JsonObject>();

    auto addFields = [&](const char* object, std::initializer_list<const char*> fields) {
        if (detectedObjects && !objectListContains(objects, object)) return;
        JsonArray array = subscribed[object].to<JsonArray>();
        for (const char* field : fields) array.add(field);
    };
    addFields("print_stats", {"state", "filename", "print_duration", "total_duration"});
    addFields("display_status", {"progress"});
    addFields("toolhead", {"extruder"});
    if (detectedObjects) {
        addFields("print_task_config", {"filament_color_rgba", "filament_type"});
        addFields("extruder", {"temperature"});
        addFields("extruder1", {"temperature"});
        addFields("extruder2", {"temperature"});
        addFields("extruder3", {"temperature"});
        addFields("heater_bed", {"temperature"});
        if (workerChamberObject_[0]) addFields(workerChamberObject_, {"temperature"});
    }

    String payload;
    payload.reserve(768);
    serializeJson(document, payload);
    webSocketSubscribePending_ = webSocket_.sendTXT(payload);
    webSocketObjectListPending_ = false;
    webSocketRequestStartedMs_ = millis();
}

void PrinterService::publishRealtimeSnapshot() {
    if (!workerSnapshotValid_ || !realtimeResultQueue_) return;
    workerSnapshot_.ok = true;
    workerSnapshot_.httpCode = 101;
    workerSnapshot_.settingsRevision = workerConfig_.settingsRevision;
    workerSnapshot_.sequence = ++workerResultSequence_;
    strlcpy(workerSnapshot_.message, "websocket", sizeof(workerSnapshot_.message));
    xQueueOverwrite(realtimeResultQueue_, &workerSnapshot_);
}

void PrinterService::queuePollResult(PollResult& result) {
    if (!resultQueue_) return;
    result.sequence = ++workerResultSequence_;
    xQueueOverwrite(resultQueue_, &result);
}

void PrinterService::resetChamberFilter() {
    workerChamberFilteredC_ = NAN;
    workerChamberFilterMs_ = 0;
}

float PrinterService::filterChamberTemperature(float rawTemperatureC) {
    if (!isfinite(rawTemperatureC)) return workerChamberFilteredC_;

    const uint32_t now = millis();
    if (!isfinite(workerChamberFilteredC_) || workerChamberFilterMs_ == 0U ||
        now - workerChamberFilterMs_ >= kChamberFilterResetGapMs) {
        workerChamberFilteredC_ = rawTemperatureC;
        workerChamberFilterMs_ = now;
        return workerChamberFilteredC_;
    }

    const uint32_t elapsedMs = now - workerChamberFilterMs_;
    if (elapsedMs == 0U) return workerChamberFilteredC_;
    const float alpha = static_cast<float>(elapsedMs) /
        static_cast<float>(kChamberFilterTimeConstantMs + elapsedMs);
    workerChamberFilteredC_ += alpha * (rawTemperatureC - workerChamberFilteredC_);
    workerChamberFilterMs_ = now;
    return workerChamberFilteredC_;
}

void PrinterService::applyRealtimeStatus(JsonVariantConst status) {
    if (!status.is<JsonObjectConst>()) return;

    JsonVariantConst printStats = status["print_stats"];
    if (!printStats.isNull()) {
        const char* rawState = printStats["state"] | nullptr;
        if (rawState && rawState[0]) {
            workerSnapshot_.printerState = static_cast<uint8_t>(normalizePrinterState(rawState));
            workerSnapshotValid_ = true;
        }
        const char* filename = printStats["filename"] | nullptr;
        if (filename) strlcpy(workerSnapshot_.filename, filename, sizeof(workerSnapshot_.filename));
        if (!printStats["print_duration"].isNull()) {
            const float duration = printStats["print_duration"].as<float>();
            workerSnapshot_.printDurationSec = duration > 0.0f
                                                   ? static_cast<uint32_t>(duration + 0.5f)
                                                   : 0;
        }
    }

    JsonVariantConst displayStatus = status["display_status"];
    if (!displayStatus.isNull() && !displayStatus["progress"].isNull()) {
        workerSnapshot_.printProgress = clampProgress(displayStatus["progress"].as<float>());
    }

    JsonVariantConst toolhead = status["toolhead"];
    if (!toolhead.isNull()) {
        const char* extruder = toolhead["extruder"] | nullptr;
        if (extruder) workerSnapshot_.activeTool = toolIndexFromObject(extruder);
    }

    JsonVariantConst taskConfig = status["print_task_config"];
    if (!taskConfig.isNull()) {
        JsonArrayConst colors = taskConfig["filament_color_rgba"].as<JsonArrayConst>();
        if (!colors.isNull()) {
            for (uint8_t index = 0; index < 4U && index < colors.size(); ++index) {
                uint32_t color = 0;
                if (parseFilamentColor(colors[index] | "", color)) {
                    workerSnapshot_.filamentColorsRgb[index] = color;
                    workerSnapshot_.filamentColorMask |= static_cast<uint8_t>(1U << index);
                }
            }
        }
        JsonArrayConst materials = taskConfig["filament_type"].as<JsonArrayConst>();
        if (!materials.isNull()) {
            for (uint8_t index = 0; index < 4U && index < materials.size(); ++index) {
                const char* material = materials[index] | nullptr;
                if (material) strlcpy(workerToolMaterials_[index], material,
                                      sizeof(workerToolMaterials_[index]));
            }
        }
    }

    static constexpr const char* ToolObjects[] = {
        "extruder", "extruder1", "extruder2", "extruder3",
    };
    for (uint8_t index = 0; index < 4; ++index) {
        JsonVariantConst extruderStatus = status[ToolObjects[index]];
        if (!extruderStatus.isNull() && !extruderStatus["temperature"].isNull()) {
            workerToolTemperatures_[index] = extruderStatus["temperature"].as<float>();
        }
    }
    const uint8_t tool = workerSnapshot_.activeTool < 4 ? workerSnapshot_.activeTool : 0;
    workerSnapshot_.activeToolTempC = workerToolTemperatures_[tool];
    JsonVariantConst bedStatus = status["heater_bed"];
    if (!bedStatus.isNull() && !bedStatus["temperature"].isNull()) {
        workerSnapshot_.bedTempC = bedStatus["temperature"].as<float>();
    }
    if (workerChamberObject_[0]) {
        JsonVariantConst chamberStatus = status[workerChamberObject_];
        if (!chamberStatus.isNull() && !chamberStatus["temperature"].isNull()) {
            workerSnapshot_.chamberTempC = filterChamberTemperature(
                chamberStatus["temperature"].as<float>());
        }
    }

    if (workerSnapshot_.filamentColorMask & (1U << tool)) {
        workerSnapshot_.filamentColorRgb = workerSnapshot_.filamentColorsRgb[tool];
    }
    strlcpy(workerSnapshot_.material,
            workerToolMaterials_[tool][0] ? workerToolMaterials_[tool] : "-",
            sizeof(workerSnapshot_.material));

    const float progress = static_cast<float>(workerSnapshot_.printProgress) / 100.0f;
    const uint32_t duration = workerSnapshot_.printDurationSec;
    workerSnapshot_.printEtaSec = 0;
    if (progress > 0.001f && progress < 1.0f && duration > 0) {
        const double eta = static_cast<double>(duration) / progress - duration;
        if (isfinite(eta) && eta > 0.0 && eta <= static_cast<double>(UINT32_MAX)) {
            workerSnapshot_.printEtaSec = static_cast<uint32_t>(eta + 0.5);
        }
    }
    publishRealtimeSnapshot();
    if (realtimeSubscribed_ && webSocketFullSubscriptionRequested_) {
        realtimeFullTelemetry_ = workerSnapshotValid_;
    }
}

void PrinterService::handleRealtimeJson(const uint8_t* payload, size_t length) {
    JsonDocument document;
    if (deserializeJson(document, payload, length) != DeserializationError::Ok) return;

    const uint32_t id = document["id"] | 0U;
    if (id == 100U) {
        webSocketServerInfoPending_ = false;
        const char* klippyState = document["result"]["klippy_state"] | "";
        webSocketKlippyReady_ = strcmp(klippyState, "ready") == 0;
        if (webSocketKlippyReady_) requestRealtimeObjectList();
        return;
    }
    if (id == 103U) {
        webSocketObjectListPending_ = false;
        webSocketCoreFallbackAttempted_ = false;
        JsonArrayConst objects = document["result"]["objects"].as<JsonArrayConst>();
        subscribeRealtime(objects);
        return;
    }
    if (id == 101U) {
        webSocketSubscribePending_ = false;
        if (!document["error"].isNull()) {
            realtimeSubscribed_ = false;
            realtimeFullTelemetry_ = false;
            if (!webSocketCoreFallbackAttempted_) {
                webSocketCoreFallbackAttempted_ = true;
                Serial.println("[printer-ws] subscription rejected; retrying core objects");
                subscribeRealtime();
            } else {
                webSocketAbortRequested_ = true;
            }
            return;
        }
        realtimeSubscribed_ = true;
        JsonVariantConst status = document["result"]["status"];
        if (!status.isNull()) applyRealtimeStatus(status);
        realtimeFullTelemetry_ = webSocketFullSubscriptionRequested_ && workerSnapshotValid_;
        Serial.println("[printer-ws] realtime telemetry subscribed");
        return;
    }

    const char* method = document["method"] | "";
    if (strcmp(method, "notify_status_update") == 0) {
        JsonArrayConst params = document["params"].as<JsonArrayConst>();
        if (!params.isNull() && params.size() > 0) applyRealtimeStatus(params[0]);
    } else if (strcmp(method, "notify_klippy_ready") == 0) {
        webSocketKlippyReady_ = true;
        realtimeSubscribed_ = false;
        requestRealtimeObjectList();
    } else if (strcmp(method, "notify_klippy_shutdown") == 0 ||
               strcmp(method, "notify_klippy_disconnected") == 0) {
        webSocketKlippyReady_ = false;
        realtimeSubscribed_ = false;
        webSocketSubscribePending_ = false;
    }
}

void PrinterService::handleRealtimeEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            realtimeConnected_ = true;
            realtimeSubscribed_ = false;
            realtimeFullTelemetry_ = false;
            realtimeResourcesReleased_ = false;
            webSocketHandshakePending_ = false;
            webSocketServerInfoPending_ = false;
            webSocketObjectListPending_ = false;
            webSocketSubscribePending_ = false;
            webSocketKlippyReady_ = false;
            webSocketCoreFallbackAttempted_ = false;
            webSocketAbortRequested_ = false;
            webSocketFullSubscriptionRequested_ = false;
            webSocketLastMessageMs_ = millis();
            char identify[384];
            snprintf(identify, sizeof(identify),
                     "{\"jsonrpc\":\"2.0\",\"method\":\"server.connection.identify\","
                     "\"params\":{\"client_name\":\"coroNET OS 2\",\"version\":\"%s\","
                     "\"type\":\"display\",\"url\":\"https://github.com/AlphaStudioDE/coroNET_OS_2\"},\"id\":1}",
                     config::FirmwareVersion);
            webSocket_.sendTXT(identify);
            requestRealtimeServerInfo();
            Serial.printf("[printer-ws] connected to %s:%u\n", webSocketConnectHost_,
                          static_cast<unsigned>(workerConfig_.port));
            break;
        }
        case WStype_TEXT:
            if (payload && length) {
                webSocketLastMessageMs_ = millis();
                if (!payloadContains(payload, length,
                                     "\"method\":\"notify_proc_stat_update\"")) {
                    handleRealtimeJson(payload, length);
                }
            }
            break;
        case WStype_PONG:
        case WStype_PING:
            webSocketLastMessageMs_ = millis();
            break;
        case WStype_DISCONNECTED:
        case WStype_ERROR:
            realtimeConnected_ = false;
            realtimeSubscribed_ = false;
            realtimeFullTelemetry_ = false;
            realtimeResourcesReleased_ = true;
            webSocketHandshakePending_ = false;
            webSocketServerInfoPending_ = false;
            webSocketObjectListPending_ = false;
            webSocketSubscribePending_ = false;
            webSocketKlippyReady_ = false;
            webSocketLastConnectTryMs_ = millis();
            break;
        default:
            break;
    }
}

void PrinterService::serviceRealtime() {
    if (!workerConfigValid_ || WiFi.status() != WL_CONNECTED || state().maintenanceMode ||
        state().otaTlsWindowActive) {
        releaseRealtime(state().otaTlsWindowActive ? "OTA TLS window" :
                        state().maintenanceMode ? "maintenance" :
                        WiFi.status() != WL_CONNECTED ? "Wi-Fi offline" : "not configured");
        return;
    }

    const uint32_t now = millis();
    if (!realtimeConnected_) {
        if (webSocketProbeSocket_ >= 0) {
            const int probe = checkRealtimeProbe();
            if (probe > 0) beginRealtimeHandshake();
            else if (probe < 0) {
                closeRealtimeProbe();
                realtimeResourcesReleased_ = true;
                webSocketLastConnectTryMs_ = now;
            }
            return;
        }
        if (webSocketHandshakePending_) {
            const uint32_t started = millis();
            webSocket_.loop();
            if (!realtimeConnected_ &&
                (millis() - started > 500U || now - webSocketHandshakeStartedMs_ >=
                                                  kRealtimeHandshakeTimeoutMs)) {
                releaseRealtime("handshake timeout");
                webSocketLastConnectTryMs_ = millis();
            }
            return;
        }
        if (now - webSocketLastConnectTryMs_ >= kRealtimeReconnectMs) {
            if (!startRealtimeProbe()) webSocketLastConnectTryMs_ = now;
        }
        return;
    }

    webSocket_.loop();
    if (webSocketAbortRequested_) {
        releaseRealtime("subscription rejected");
        webSocketLastConnectTryMs_ = millis();
        return;
    }
    if (!realtimeConnected_) return;
    if (webSocketLastMessageMs_ && millis() - webSocketLastMessageMs_ > kRealtimeStaleMs) {
        releaseRealtime("stale connection");
        webSocketLastConnectTryMs_ = millis();
        return;
    }

    if ((webSocketServerInfoPending_ || webSocketObjectListPending_ ||
         webSocketSubscribePending_) &&
        millis() - webSocketRequestStartedMs_ >= kRealtimeRequestTimeoutMs) {
        if (webSocketObjectListPending_) {
            webSocketObjectListPending_ = false;
            subscribeRealtime();
        } else if (webSocketSubscribePending_) {
            webSocketSubscribePending_ = false;
            subscribeRealtime();
        } else {
            webSocketServerInfoPending_ = false;
            requestRealtimeObjectList();
        }
    }
}

void PrinterService::setOffline(const char* message, int httpCode) {
    SystemState& system = state();
    setConnectionState(false);
    system.printerTelemetryValid = false;
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

void PrinterService::setConnectionState(bool connected) {
    SystemState& system = state();
    if (system.printerConnected == connected) return;
    system.printerConnected = connected;
    system.printerConnectionRevision++;
}

void PrinterService::workerTaskEntry(void* context) {
    static_cast<PrinterService*>(context)->workerLoop();
}

void PrinterService::workerLoop() {
    WorkerRequest request;
    for (;;) {
        const bool suspended = state().maintenanceMode || state().otaTlsWindowActive;
        if (suspended) releaseRealtime("system network window");

        if (xQueueReceive(requestQueue_, &request, pdMS_TO_TICKS(kRealtimeWorkerTickMs)) != pdTRUE) {
            serviceRealtime();
            continue;
        }

        if (request.type == WorkerJobType::Configure) {
            configureRealtime(request.poll);
            serviceRealtime();
            continue;
        }
        if (state().maintenanceMode || state().otaTlsWindowActive) {
            if (request.type == WorkerJobType::Discover) {
                updateDiscovery(PrinterDiscoveryStatus::Failed, 0,
                                "Printer discovery paused for system update");
            } else {
                PollResult paused;
                paused.settingsRevision = request.poll.settingsRevision;
                strlcpy(paused.message, "printer_poll_paused", sizeof(paused.message));
                queuePollResult(paused);
            }
            continue;
        }

        if (request.type == WorkerJobType::Discover) {
            releaseRealtime("printer discovery");
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
        configureRealtime(request.poll);
        if (xSemaphoreTake(httpMutex_, portMAX_DELAY) == pdTRUE) {
            result.ok = performPoll(request.poll, result);
            xSemaphoreGive(httpMutex_);
        } else {
            strlcpy(result.message, "http_mutex_failed", sizeof(result.message));
        }
        if (result.ok) {
            workerSnapshot_ = result;
            workerSnapshotValid_ = true;
            if (result.activeTool < 4) {
                workerToolTemperatures_[result.activeTool] = result.activeToolTempC;
            }
            if (result.activeTool < 4 && result.material[0]) {
                strlcpy(workerToolMaterials_[result.activeTool], result.material,
                        sizeof(workerToolMaterials_[result.activeTool]));
            }
        } else if (realtimeConnected_ && realtimeSubscribed_ && workerSnapshotValid_) {
            result = workerSnapshot_;
            result.settingsRevision = workerConfig_.settingsRevision;
            result.ok = true;
            strlcpy(result.message, "websocket_http_audit_deferred", sizeof(result.message));
        }
        queuePollResult(result);
        serviceRealtime();
    }
}

}
