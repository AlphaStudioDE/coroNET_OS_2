#pragma once

#include <Arduino.h>

#include "ProductTypes.h"

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
    bool sdReady = false;
    bool audioPlaying = false;
    SoundScenario activeSoundScenario = SoundScenario::Start;
    char activeSoundPath[65] = "";
    uint8_t audioFileCount = 0;
    bool audioAssetsValid = false;
    char audioAssetStatus[72] = "SD not checked";
    bool ledReady = false;
    uint32_t ledFrameCount = 0;
    uint32_t ledDroppedFrames = 0;
    bool ventReady = false;
    bool fanReady = false;
    bool servoReady = false;
    uint8_t fanPercent = 0;
    uint8_t flapPercent = 0;
    bool ventFailsafe = false;
    char ventStatusText[72] = "disabled";
    bool pandaConnected = false;
    PandaWorkflowPhase pandaPhase = PandaWorkflowPhase::Idle;
    float pandaCurrentTempC = NAN;
    uint8_t pandaTargetTempC = 0;
    bool pandaHeating = false;
    char pandaStatusText[72] = "disabled";
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
    bool printerTelemetryValid = false;
    uint32_t lastPrinterUpdateMs = 0;
    uint32_t printerTelemetryRevision = 0;
    uint32_t printerConnectionRevision = 0;
    uint32_t printerStateEventSequence = 0;
    uint32_t printerStateChangedMs = 0;
    PrinterState printerState = PrinterState::Unknown;
    PrinterState printerEventFrom = PrinterState::Unknown;
    PrinterState printerEventTo = PrinterState::Unknown;
    uint8_t printProgress = 0;
    uint32_t printDurationSec = 0;
    uint32_t printEtaSec = 0;
    uint8_t activeTool = 0;
    float activeToolTempC = NAN;
    float bedTempC = NAN;
    float chamberTempC = NAN;
    char printFilename[65] = "";
    char materialName[25] = "";
    uint32_t filamentColorRgb = 0xFFFFFF;
    char printerStatusText[96] = "";

    bool screenSaverActive = false;
    bool displaySleeping = false;
    bool timeReady = false;
    bool quietActive = false;
    bool maintenanceMode = false;
    bool otaTlsWindowActive = false;
    OtaState otaState = OtaState::Idle;
    uint8_t otaProgress = 0;
    bool otaUpdateAvailable = false;
    char otaAvailableVersion[24] = "";
    char otaStatusText[96] = "Ready";

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
