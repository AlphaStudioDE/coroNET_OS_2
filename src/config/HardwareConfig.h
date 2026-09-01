#pragma once

#include <Arduino.h>

namespace coronet::hw {

static constexpr uint16_t LedCount = 60;
static constexpr uint16_t OuterStart = 0;
static constexpr uint16_t OuterEnd = 41;
static constexpr uint16_t InsideStart = 42;
static constexpr uint16_t InsideEnd = 59;

static constexpr uint8_t LedDataPin = 15;
static constexpr uint8_t LedSpiSckDummyPin = 16;

static constexpr int I2sMckPin = -1;
static constexpr uint8_t I2sBckPin = 42;
static constexpr uint8_t I2sLrckPin = 2;
static constexpr uint8_t I2sDoutPin = 41;

static constexpr uint8_t SdMmcClkPin = 12;
static constexpr uint8_t SdMmcCmdPin = 11;
static constexpr uint8_t SdMmcD0Pin = 13;

static constexpr uint8_t ServoPin = 9;
static constexpr uint8_t FanPwmPin = 14;
static constexpr uint8_t ChamberHeaterPin = 46;

static_assert(LedCount == 60, "coroNET hardware uses exactly 60 LEDs");
static_assert(OuterEnd + 1 == InsideStart, "Inside LEDs must follow outer LEDs");
static_assert((OuterEnd - OuterStart + 1) == 42, "Outer LED count must be 42");
static_assert((InsideEnd - InsideStart + 1) == 18, "Inside LED count must be 18");

}
