#pragma once

#include <Arduino.h>

namespace coronet {
namespace ledcurve {

// Animation values describe perceived brightness. These helpers translate
// between that space and the linear PWM values used by the physical LEDs.
uint8_t encode(uint8_t perceived);
uint8_t decode(uint8_t pwm);

}  // namespace ledcurve
}  // namespace coronet
