#pragma once

#include <stdint.h>

namespace coronet {

class MemoryService {
public:
    void begin();

private:
    void sampleState(bool externalMallocEnabled);
};

MemoryService& memoryService();

}
