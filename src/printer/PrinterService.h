#pragma once

#include <Arduino.h>
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
    char host[16] = "";
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
        float bedTempC = NAN;
        float chamberTempC = NAN;
        char filename[65] = "";
        char message[96] = "";
    };

    enum class WorkerJobType : uint8_t {
        Poll = 0,
        Discover,
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
    SemaphoreHandle_t httpMutex_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;
    DiscoveredPrinter* discoveredPrinters_ = nullptr;
    mutable portMUX_TYPE discoveryMux_ = portMUX_INITIALIZER_UNLOCKED;
    PrinterDiscoveryStatus discoveryStatus_ = PrinterDiscoveryStatus::Idle;
    uint8_t discoveryCount_ = 0;
    uint8_t discoveryProgress_ = 0;
    uint32_t discoveryRevision_ = 0;
    char discoveryMessage_[72] = "";

    bool configured() const;
    bool captureRequest(PollRequest& request) const;
    bool enqueuePoll();
    void consumeResults();
    void applyResult(const PollResult& result);
    bool requestInfo(const PollRequest& request, PrinterTestResult* result);
    bool performPoll(const PollRequest& request, PollResult& result);
    void performDiscovery();
    bool probeMoonraker(const char* host, uint16_t port);
    bool addDiscoveredPrinter(const char* host, uint16_t port, const char* name);
    void updateDiscovery(PrinterDiscoveryStatus status, uint8_t progress, const char* message);
    void setOffline(const char* message, int httpCode = 0);
    static void workerTaskEntry(void* context);
    void workerLoop();
};

PrinterService& printerService();

}
