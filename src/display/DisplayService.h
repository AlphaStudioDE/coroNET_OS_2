#pragma once

#include <stdint.h>

namespace coronet {

class DisplayService {
public:
    void begin();
    void loop();

private:
    void buildBootScreen();
    void applyBrightness(uint8_t percent);

    bool started_ = false;
    uint32_t lastUiUpdateMs_ = 0;
};

}
