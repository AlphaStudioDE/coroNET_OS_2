#pragma once

#include <Arduino.h>

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
    bool started_ = false;
    uint32_t lastPollMs_ = 0;
    uint32_t lastTestMs_ = 0;

    bool configured() const;
    String baseUrl() const;
    bool requestInfo(PrinterTestResult* result);
    bool pollStatus();
    void setOffline(const char* message, int httpCode = 0);
};

PrinterService& printerService();

}
