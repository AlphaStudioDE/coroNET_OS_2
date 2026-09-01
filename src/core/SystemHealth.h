#pragma once

#include <stdint.h>

namespace coronet {

class SystemHealth {
public:
    void begin();
    void loop();
    void sample();
    void log() const;
    void checkpoint(const char* label);

private:
    unsigned long lastSampleMs_ = 0;
    unsigned long lastLogMs_ = 0;
    uint32_t checkpointInternalFree_ = 0;
    uint32_t checkpointDmaFree_ = 0;
    uint32_t checkpointPsramFree_ = 0;
    bool checkpointReady_ = false;
};

}
