#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace coronet {

enum class PrinterDiscoveryStatus : uint8_t {
    Idle = 0,
    Scanning,
    Complete,
    Failed,
};

struct DiscoveredPrinter {
    char name[49] = "";
    char host[65] = "";
    uint16_t port = 7125;
};

struct PrinterDiscoverySnapshot {
    PrinterDiscoveryStatus status = PrinterDiscoveryStatus::Idle;
    uint8_t count = 0;
    uint8_t progress = 0;
    uint32_t revision = 0;
    char message[72] = "";
};

struct PrinterTestResult {
    bool ok = false;
    int httpCode = 0;
    char message[96] = "";
};

class PrinterService {
public:
    void begin();
    void loop();
    PrinterTestResult testConnection();
    bool requestDiscovery();
    void discoverySnapshot(PrinterDiscoverySnapshot& output) const;
    bool discoveredPrinter(uint8_t index, DiscoveredPrinter& output) const;
    bool realtimeResourcesReleased() const { return realtimeResourcesReleased_; }
    void logStatus() const;

private:
    struct PollRequest {
        uint32_t settingsRevision = 0;
        char host[65] = "";
        uint16_t port = 7125;
        char apiKey[97] = "";
    };

    struct PollResult {
        bool ok = false;
        int httpCode = 0;
        uint32_t settingsRevision = 0;
        uint8_t printerState = 0;
        uint8_t printProgress = 0;
        uint8_t activeTool = 0;
        float activeToolTempC = NAN;
        float toolTemperaturesC[4] = {NAN, NAN, NAN, NAN};
        float bedTempC = NAN;
        float chamberTempC = NAN;
        uint32_t printDurationSec = 0;
        uint32_t printEtaSec = 0;
        uint32_t filamentColorRgb = 0xFFFFFF;
        uint32_t filamentColorsRgb[4] = {};
        uint8_t filamentColorMask = 0;
        uint32_t sequence = 0;
        char filename[65] = "";
        char material[25] = "";
        char message[96] = "";
    };

    enum class WorkerJobType : uint8_t {
        Poll = 0,
        Discover,
        Configure,
    };

    struct WorkerRequest {
        WorkerJobType type = WorkerJobType::Poll;
        PollRequest poll;
    };

    static constexpr uint8_t MaxDiscoveredPrinters = 8;

    bool started_ = false;
    bool pollInFlight_ = false;
    uint8_t consecutiveFailures_ = 0;
    uint32_t lastPollMs_ = 0;
    uint32_t lastTestMs_ = 0;
    QueueHandle_t requestQueue_ = nullptr;
    QueueHandle_t resultQueue_ = nullptr;
    QueueHandle_t realtimeResultQueue_ = nullptr;
    SemaphoreHandle_t httpMutex_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;
    DiscoveredPrinter* discoveredPrinters_ = nullptr;
    mutable portMUX_TYPE discoveryMux_ = portMUX_INITIALIZER_UNLOCKED;
    PrinterDiscoveryStatus discoveryStatus_ = PrinterDiscoveryStatus::Idle;
    uint8_t discoveryCount_ = 0;
    uint8_t discoveryProgress_ = 0;
    uint32_t discoveryRevision_ = 0;
    char discoveryMessage_[72] = "";
    char configuredHost_[65] = "";
    char configuredApiKey_[97] = "";
    uint16_t configuredPort_ = 7125;
    uint32_t observedSettingsRevision_ = 0;
    uint32_t printerConfigRevision_ = 1;
    uint32_t queuedConfigRevision_ = 0;
    uint32_t lastAppliedResultSequence_ = 0;

    WebSocketsClient webSocket_;
    PollRequest workerConfig_;
    PollResult workerSnapshot_;
    char workerToolMaterials_[4][25] = {};
    float workerToolTemperatures_[4] = {NAN, NAN, NAN, NAN};
    float workerChamberFilteredC_ = NAN;
    char workerChamberObject_[65] = "temperature_sensor cavity";
    char webSocketConnectHost_[65] = "";
    int webSocketProbeSocket_ = -1;
    volatile bool realtimeConnected_ = false;
    volatile bool realtimeSubscribed_ = false;
    volatile bool realtimeFullTelemetry_ = false;
    volatile bool realtimeResourcesReleased_ = true;
    bool workerConfigValid_ = false;
    bool workerSnapshotValid_ = false;
    bool webSocketHandshakePending_ = false;
    bool webSocketServerInfoPending_ = false;
    bool webSocketObjectListPending_ = false;
    bool webSocketSubscribePending_ = false;
    bool webSocketKlippyReady_ = false;
    bool webSocketCoreFallbackAttempted_ = false;
    bool webSocketAbortRequested_ = false;
    bool webSocketFullSubscriptionRequested_ = false;
    uint32_t webSocketLastConnectTryMs_ = 0;
    uint32_t webSocketProbeStartedMs_ = 0;
    uint32_t webSocketHandshakeStartedMs_ = 0;
    uint32_t webSocketRequestStartedMs_ = 0;
    uint32_t webSocketLastMessageMs_ = 0;
    uint32_t workerChamberFilterMs_ = 0;
    uint32_t workerResultSequence_ = 0;

    bool configured() const;
    bool captureRequest(PollRequest& request) const;
    void refreshConfiguration();
    bool enqueueConfiguration();
    bool enqueuePoll();
    void consumeResults();
    void applyResult(const PollResult& result);
    bool requestInfo(const PollRequest& request, PrinterTestResult* result);
    bool performPoll(const PollRequest& request, PollResult& result);
    void setConnectionState(bool connected);
    void performDiscovery();
    bool probeMoonraker(const char* host, uint16_t port);
    uint8_t discoverMdnsService(const char* service,
                                const char* defaultName,
                                bool useAdvertisedPort);
    bool addDiscoveredPrinter(const char* host, uint16_t port, const char* name);
    void updateDiscovery(PrinterDiscoveryStatus status, uint8_t progress, const char* message);
    void setOffline(const char* message, int httpCode = 0);
    void configureRealtime(const PollRequest& request);
    void serviceRealtime();
    void releaseRealtime(const char* reason);
    bool startRealtimeProbe();
    int checkRealtimeProbe() const;
    void closeRealtimeProbe();
    void beginRealtimeHandshake();
    void handleRealtimeEvent(WStype_t type, uint8_t* payload, size_t length);
    void handleRealtimeJson(const uint8_t* payload, size_t length);
    void requestRealtimeServerInfo();
    void requestRealtimeObjectList();
    void subscribeRealtime(JsonArrayConst objects = JsonArrayConst());
    void resetChamberFilter();
    float filterChamberTemperature(float rawTemperatureC);
    void applyRealtimeStatus(JsonVariantConst status);
    void publishRealtimeSnapshot();
    void queuePollResult(PollResult& result);
    static void workerTaskEntry(void* context);
    void workerLoop();
};

PrinterService& printerService();

}
