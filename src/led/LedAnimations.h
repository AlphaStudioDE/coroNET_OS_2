#pragma once

#include <Arduino.h>

#include "../core/ProductTypes.h"

namespace coronet {

enum class PrintAnimation : uint8_t {
    ProgressBar = 0,
    Laser,
    Wave,
    Thermal,
    Stripes,
    ProgressPulse,
    Comet,
    ActiveSection,
    Running,
    Breathe,
    Count,
};

struct LedAnimationContext {
    uint32_t nowMs = 0;
    uint8_t progress = 0;
    uint8_t activeTool = 0;
    float activeToolTempC = NAN;
    float bedTempC = NAN;
    float chamberTempC = NAN;
    uint32_t filamentRgb = 0xFFFFFF;
    uint32_t printDurationSec = 0;
    uint32_t printEtaSec = 0;
    bool preview = false;
};

uint8_t ledAnimationCount(LedCategory category);
const char* ledAnimationName(LedCategory category, uint8_t animation);
uint8_t normalizeLedAnimation(LedCategory category, uint8_t animation);

}
