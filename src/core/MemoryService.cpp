#include "MemoryService.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../config/AppConfig.h"
#include "SystemState.h"

namespace coronet {

namespace {
MemoryService gMemoryService;
}

MemoryService& memoryService() {
    return gMemoryService;
}

void MemoryService::begin() {
    const bool psramReady = psramFound() && ESP.getPsramSize() > 0;
    bool externalMallocEnabled = false;

    if (psramReady) {
        heap_caps_malloc_extmem_enable(config::PsramMallocThresholdBytes);
        externalMallocEnabled = true;
    }

    sampleState(externalMallocEnabled);

    Serial.printf("MemoryService: psram=%s size=%lu threshold=%uB external_malloc=%s\n",
                  psramReady ? "ready" : "missing",
                  static_cast<unsigned long>(ESP.getPsramSize()),
                  static_cast<unsigned>(config::PsramMallocThresholdBytes),
                  externalMallocEnabled ? "enabled" : "disabled");
}

void MemoryService::sampleState(bool externalMallocEnabled) {
    SystemState& s = state();
    s.psramReady = psramFound() && ESP.getPsramSize() > 0;
    s.externalMallocEnabled = externalMallocEnabled;
    s.externalMallocThreshold = externalMallocEnabled ? config::PsramMallocThresholdBytes : 0;
}

}
