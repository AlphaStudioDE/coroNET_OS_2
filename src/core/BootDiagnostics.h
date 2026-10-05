#pragma once

#include <Arduino.h>

namespace coronet {

struct BootCrashSnapshot {
    int32_t resetReason = 0;
    int32_t imageError = 0;
    int32_t summaryError = 0;
    uint32_t imageAddress = 0;
    uint32_t imageSize = 0;
    bool imagePresent = false;
    bool imageValid = false;
    bool summaryValid = false;
    char task[17] = "";
    uint32_t exceptionPc = 0;
    uint32_t exceptionCause = 0;
    uint32_t exceptionAddress = 0;
    uint32_t backtrace[16] = {};
    uint8_t backtraceDepth = 0;
    bool backtraceCorrupted = false;
    char appElfSha256[65] = "";
};

class BootDiagnostics {
public:
    void begin();
    const BootCrashSnapshot& snapshot() const { return snapshot_; }

private:
    BootCrashSnapshot snapshot_;
};

BootDiagnostics& bootDiagnostics();

}
