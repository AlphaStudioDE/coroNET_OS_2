#pragma once

#include <Arduino.h>

namespace coronet::hw {

static constexpr uint16_t LedCount = 60;
static constexpr uint16_t RightStart = 0;
static constexpr uint16_t RightEnd = 10;
static constexpr uint16_t CenterStart = 11;
static constexpr uint16_t CenterEnd = 30;
static constexpr uint16_t LeftStart = 31;
static constexpr uint16_t LeftEnd = 41;
static constexpr uint16_t OuterStart = 0;
static constexpr uint16_t OuterEnd = 41;
static constexpr uint16_t InsideStart = 42;
static constexpr uint16_t InsideEnd = 59;
static constexpr uint16_t RightCount = RightEnd - RightStart + 1;
static constexpr uint16_t CenterCount = CenterEnd - CenterStart + 1;
static constexpr uint16_t LeftCount = LeftEnd - LeftStart + 1;
static constexpr uint16_t OuterCount = OuterEnd - OuterStart + 1;
static constexpr uint16_t InsideCount = InsideEnd - InsideStart + 1;

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
static constexpr uint8_t DiyChamberHeaterPin = 46;
static constexpr uint16_t ServoPulseMinUs = 500;
static constexpr uint16_t ServoPulseMaxUs = 2500;
static constexpr uint32_t ServoFrequencyHz = 50;
static constexpr uint32_t FanPwmFrequencyHz = 25000;

static_assert(LedCount == 60, "coroNET hardware uses exactly 60 LEDs");
static_assert(OuterEnd + 1 == InsideStart, "Inside LEDs must follow outer LEDs");
static_assert(RightCount == 11, "Right LED count must be 11");
static_assert(CenterCount == 20, "Center LED count must be 20");
static_assert(LeftCount == 11, "Left LED count must be 11");
static_assert(OuterCount == 42, "Outer LED count must be 42");
static_assert(InsideCount == 18, "Inside LED count must be 18");

}
