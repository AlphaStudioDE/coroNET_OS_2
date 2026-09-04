#pragma once

#include <cstddef>
#include <cstdint>

#include "../config/HardwareConfig.h"

namespace coronet::ledpreview {

static constexpr uint8_t Version = 1;
static constexpr uint8_t PixelFormatRgb888 = 1;
static constexpr size_t HeaderSize = 12;

#pragma pack(push, 1)
struct Frame {
    uint8_t version;
    uint8_t pixelFormat;
    uint16_t size;
    uint32_t sequence;
    uint8_t outerCount;
    uint8_t insideCount;
    uint16_t reserved;
    uint8_t pixels[hw::LedCount * 3U];
};
#pragma pack(pop)

static_assert(sizeof(Frame) == HeaderSize + hw::LedCount * 3U,
              "LED preview frame layout changed");
static_assert(sizeof(Frame) <= 236,
              "LED preview no longer fits one preferred-MTU BLE frame");

}  // namespace coronet::ledpreview
