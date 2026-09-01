#pragma once

#include <cstddef>
#include <cstdint>

namespace coronet::bleprotocol {

static constexpr uint8_t Version = 1;
static constexpr size_t MaxAttPayload = 244;

enum class MessageType : uint8_t {
    StateSnapshot = 1,
    EventJson = 2,
    SettingsJson = 3,
    PairingJson = 4,
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

struct StateSnapshotV1 {
    uint8_t version;
    uint8_t messageType;
    uint16_t size;
    uint32_t revision;
    uint32_t uptimeMs;
    uint16_t flags;
    uint8_t printerState;
    uint8_t printProgress;
    uint8_t activeTool;
    uint8_t reserved;
    int16_t activeToolTempTenths;
    int16_t bedTempTenths;
    int16_t chamberTempTenths;
    char deviceId[13];
    char deviceName[25];
    char printerStatus[48];
    char printFilename[65];
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 8, "BLE frame header layout changed");
static_assert(sizeof(StateSnapshotV1) <= 180, "BLE state snapshot no longer fits the preferred MTU");

}
