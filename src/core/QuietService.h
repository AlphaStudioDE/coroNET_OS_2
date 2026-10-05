#pragma once

#include <Arduino.h>

#include "ProductTypes.h"

namespace coronet {

class QuietService {
public:
    void begin();
    void loop();

private:
    QuietTarget observedTarget_ = QuietTarget::Off;
    uint16_t observedDurationMinutes_ = 60;
    uint32_t activeSinceMs_ = 0;
};

QuietService& quietService();

}
