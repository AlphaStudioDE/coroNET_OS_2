#include "MemoryService.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../config/AppConfig.h"
#include "SystemHealth.h"
#include "SystemState.h"

#if F_CPU != 240000000L
#error "coroNET OS 2 requires a 240 MHz CPU configuration"
#endif

#if !defined(CONFIG_ESPTOOLPY_FLASHMODE_QIO)
#error "coroNET OS 2 requires QIO flash mode"
#endif

#if !defined(CONFIG_ESPTOOLPY_FLASHFREQ_80M)
#error "coroNET OS 2 requires an 80 MHz flash configuration"
#endif

#if !defined(CONFIG_SPIRAM_MODE_OCT)
#error "coroNET OS 2 requires Octal (OPI) PSRAM"
#endif

#if !defined(CONFIG_SPIRAM_SPEED_80M)
#error "coroNET OS 2 requires the stable 80 MHz OPI PSRAM configuration"
#endif

namespace coronet {

namespace {
MemoryService gMemoryService;

const char* flashModeName(FlashMode_t mode) {
    switch (mode) {
        case FM_QIO: return "QIO";
        case FM_QOUT: return "QOUT";
        case FM_DIO: return "DIO";
        case FM_DOUT: return "DOUT";
        case FM_FAST_READ: return "FAST_READ";
        case FM_SLOW_READ: return "SLOW_READ";
        default: return "UNKNOWN";
    }
}
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

    const uint32_t cpuMhz = ESP.getCpuFreqMHz();
    const uint32_t flashSize = ESP.getFlashChipSize();
    const uint32_t flashSpeed = ESP.getFlashChipSpeed();
    const FlashMode_t flashMode = ESP.getFlashChipMode();
    const uint32_t psramSize = ESP.getPsramSize();

    Serial.printf("MemoryService: cpu=%luMHz flash=%luB/%luMHz/%s psram=%s/%luB threshold=%uB external_malloc=%s\n",
                  static_cast<unsigned long>(cpuMhz),
                  static_cast<unsigned long>(flashSize),
                  static_cast<unsigned long>(flashSpeed / 1000000U),
                  flashModeName(flashMode),
                  psramReady ? "ready" : "missing",
                  static_cast<unsigned long>(psramSize),
                  static_cast<unsigned>(config::PsramMallocThresholdBytes),
                  externalMallocEnabled ? "enabled" : "disabled");

    if (cpuMhz != config::ExpectedCpuFrequencyMhz ||
        flashSize != config::ExpectedFlashSizeBytes ||
        flashSpeed != config::ExpectedFlashFrequencyHz ||
        flashMode != FM_QIO ||
        psramSize != config::ExpectedPsramSizeBytes) {
        Serial.println("MemoryService WARNING: runtime hardware profile differs from the coroNET JC3248W535 profile");
    }
}

void MemoryService::sampleState(bool externalMallocEnabled) {
    SystemState& s = state();
    s.psramReady = psramFound() && ESP.getPsramSize() > 0;
    s.externalMallocEnabled = externalMallocEnabled;
    s.externalMallocThreshold = externalMallocEnabled ? config::PsramMallocThresholdBytes : 0;
}

bool MemoryService::reserveStartupDma(size_t bytes) {
    if (startupDmaReservation_ || bytes == 0) return startupDmaReservation_ != nullptr;
    startupDmaReservation_ = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!startupDmaReservation_) {
        Serial.printf("[memory] startup DMA reservation failed: %uB\n", static_cast<unsigned>(bytes));
        return false;
    }
    startupDmaReservationBytes_ = bytes;
    Serial.printf("[memory] startup DMA reservation=%uB\n", static_cast<unsigned>(bytes));
    logHeapDiagnostics("startup-dma-held");
    return true;
}

void MemoryService::runtimeLoop(bool webReady) {
    if (!startupDmaReservation_) return;
    const uint32_t now = millis();
    if (runtimeStartedMs_ == 0) runtimeStartedMs_ = now;
    if (webReady) {
        releaseStartupDma("web-ready");
    } else if (now - runtimeStartedMs_ >= 10000U) {
        releaseStartupDma("runtime-timeout");
    }
}

void MemoryService::releaseStartupDma(const char* reason) {
    if (!startupDmaReservation_) return;
    heap_caps_free(startupDmaReservation_);
    startupDmaReservation_ = nullptr;
    Serial.printf("[memory] startup DMA reservation released=%uB reason=%s\n",
                  static_cast<unsigned>(startupDmaReservationBytes_), reason ? reason : "-");
    startupDmaReservationBytes_ = 0;
    logHeapDiagnostics("startup-dma-free");
}

}
