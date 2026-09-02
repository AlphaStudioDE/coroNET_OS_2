#pragma once

#include <stddef.h>
#include <stdint.h>

namespace coronet {

class MemoryService {
public:
    void begin();
    bool reserveStartupDma(size_t bytes);
    void runtimeLoop(bool webReady);

private:
    void sampleState(bool externalMallocEnabled);
    void releaseStartupDma(const char* reason);

    void* startupDmaReservation_ = nullptr;
    size_t startupDmaReservationBytes_ = 0;
    uint32_t runtimeStartedMs_ = 0;
};

MemoryService& memoryService();

}
