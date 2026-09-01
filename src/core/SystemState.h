#pragma once

#include <Arduino.h>

namespace coronet {

enum class PrinterState : uint8_t {
    Unknown,
    Idle,
    Printing,
    Paused,
    Error,
    Complete,
};

struct SystemState {
    uint32_t bootMs = 0;
    uint32_t uptimeMs = 0;
    bool wifiConnected = false;
    bool bleReady = false;
    bool bleConnected = false;
    bool webReady = false;
    bool audioReady = false;
    bool displayReady = false;
    bool touchReady = false;
    bool setupDone = false;
    bool psramReady = false;
    bool externalMallocEnabled = false;
    uint32_t externalMallocThreshold = 0;
    uint32_t touchCount = 0;
    uint32_t lastTouchMs = 0;
    bool printerConfigured = false;
    bool printerConnected = false;
    uint32_t lastPrinterUpdateMs = 0;
    PrinterState printerState = PrinterState::Unknown;
    uint8_t printProgress = 0;
    uint8_t activeTool = 0;
    float activeToolTempC = NAN;
    float bedTempC = NAN;
    float chamberTempC = NAN;
    char printFilename[65] = "";
    char printerStatusText[96] = "";

    uint32_t heapFree = 0;
    uint32_t heapLargest = 0;
    uint32_t internalFree = 0;
    uint32_t internalLargest = 0;
    uint32_t internalMinFree = 0;
    uint32_t dmaFree = 0;
    uint32_t dmaLargest = 0;
    uint32_t dmaMinFree = 0;
    uint32_t psramFree = 0;
    uint32_t psramLargest = 0;
    uint32_t psramMinFree = 0;
};

SystemState& state();

}
