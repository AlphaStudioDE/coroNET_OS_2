#pragma once

#include <Arduino.h>

#include "LedAnimations.h"

namespace coronet {

// Preserved coroNET 1 renderer. LedService still owns output brightness,
// physical layout mirroring and the inside-light policy.
void renderLegacyLedAnimation(LedCategory category, uint8_t animation,
                              const LedAnimationContext& context,
                              int16_t colorRemixDegrees,
                              RgbwColor* output, size_t outputCount);

}
