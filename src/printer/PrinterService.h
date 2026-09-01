#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace coronet {

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

    bool started_ = false;
    bool pollInFlight_ = false;
    uint8_t consecutiveFailures_ = 0;
    uint32_t lastPollMs_ = 0;
    uint32_t lastTestMs_ = 0;
    QueueHandle_t requestQueue_ = nullptr;
    QueueHandle_t resultQueue_ = nullptr;
    SemaphoreHandle_t httpMutex_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;

    bool configured() const;
    bool captureRequest(PollRequest& request) const;
    bool enqueuePoll();
    void consumeResults();
    void applyResult(const PollResult& result);
    bool requestInfo(const PollRequest& request, PrinterTestResult* result);
    bool performPoll(const PollRequest& request, PollResult& result);
    void setOffline(const char* message, int httpCode = 0);
    static void workerTaskEntry(void* context);
    void workerLoop();
};

PrinterService& printerService();

}
