#pragma once

#include <cstddef>
#include <cstdint>

namespace coronet::bleprotocol {

static constexpr uint8_t Version = 2;
static constexpr size_t MaxAttPayload = 244;

enum class MessageType : uint8_t {
    StateSnapshot = 1,
    EventJson = 2,
    SettingsJson = 3,
    PairingJson = 4,
    LedFrame = 5,
};

enum StateFlag : uint16_t {
    SetupDone = 1U << 0,
    WifiConnected = 1U << 1,
    WebReady = 1U << 2,
    BleConnected = 1U << 3,
    DisplayReady = 1U << 4,
    TouchReady = 1U << 5,
    PrinterConfigured = 1U << 6,
    PrinterConnected = 1U << 7,
    AudioReady = 1U << 8,
    BleFallbackActive = 1U << 9,
    PrinterTelemetryValid = 1U << 10,
};

enum RuntimeFlag : uint8_t {
    AudioPlaying = 1U << 0,
    QuietActive = 1U << 1,
};

#pragma pack(push, 1)
struct FrameHeader {
    uint8_t version;
    uint8_t messageType;
    uint16_t messageId;
    uint16_t totalLength;
    uint8_t chunkIndex;
    uint8_t chunkCount;
};

struct StateSnapshotV2 {
    uint8_t version;
    uint8_t messageType;
    uint16_t size;
    uint32_t revision;
    uint32_t uptimeMs;
    uint16_t flags;
    uint8_t printerState;
    uint8_t printProgress;
    uint8_t activeTool;
    uint8_t runtimeFlags;
    int16_t activeToolTempTenths;
    int16_t bedTempTenths;
    int16_t chamberTempTenths;
    char deviceId[13];
    char deviceName[25];
    char printerStatus[48];
    char printFilename[65];
    uint32_t printerTelemetryRevision;
    uint32_t printerStateEventSequence;
    uint8_t printerEventFrom;
    uint8_t printerEventTo;
    uint8_t fanPercent;
    uint8_t flapPercent;
    int16_t toolTempTenths[4];
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 8, "BLE frame header layout changed");
static_assert(sizeof(StateSnapshotV2) == 195, "BLE V2 state snapshot layout changed");
static_assert(sizeof(StateSnapshotV2) <= 236, "BLE state snapshot no longer fits one preferred-MTU frame");

}
