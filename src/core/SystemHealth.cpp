#include "SystemHealth.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../config/AppConfig.h"
#include "SystemState.h"

namespace coronet {

namespace {

uint32_t fragmentationPercent(const multi_heap_info_t& info) {
    if (info.total_free_bytes == 0) return 100;
    const uint64_t contiguous = static_cast<uint64_t>(info.largest_free_block) * 100ULL;
    const uint32_t contiguousPercent = static_cast<uint32_t>(contiguous / info.total_free_bytes);
    return contiguousPercent >= 100U ? 0U : 100U - contiguousPercent;
}

multi_heap_info_t heapInfo(uint32_t caps) {
    multi_heap_info_t info{};
    heap_caps_get_info(&info, caps);
    return info;
}

}

void logHeapDiagnostics(const char* label) {
    const multi_heap_info_t internal = heapInfo(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const multi_heap_info_t dma = heapInfo(MALLOC_CAP_DMA);
    const multi_heap_info_t psram = heapInfo(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    Serial.printf(
        "[heap-detail] %-18s internal=%lu/%lu frag=%lu%% blocks=%lu/%lu min=%lu "
        "dma=%lu/%lu frag=%lu%% blocks=%lu/%lu min=%lu "
        "psram=%lu/%lu frag=%lu%% blocks=%lu/%lu min=%lu\n",
        label ? label : "-",
        static_cast<unsigned long>(internal.total_free_bytes),
        static_cast<unsigned long>(internal.largest_free_block),
        static_cast<unsigned long>(fragmentationPercent(internal)),
        static_cast<unsigned long>(internal.free_blocks),
        static_cast<unsigned long>(internal.allocated_blocks),
        static_cast<unsigned long>(internal.minimum_free_bytes),
        static_cast<unsigned long>(dma.total_free_bytes),
        static_cast<unsigned long>(dma.largest_free_block),
        static_cast<unsigned long>(fragmentationPercent(dma)),
        static_cast<unsigned long>(dma.free_blocks),
        static_cast<unsigned long>(dma.allocated_blocks),
        static_cast<unsigned long>(dma.minimum_free_bytes),
        static_cast<unsigned long>(psram.total_free_bytes),
        static_cast<unsigned long>(psram.largest_free_block),
        static_cast<unsigned long>(fragmentationPercent(psram)),
        static_cast<unsigned long>(psram.free_blocks),
        static_cast<unsigned long>(psram.allocated_blocks),
        static_cast<unsigned long>(psram.minimum_free_bytes));
}

void SystemHealth::begin() {
    sample();
    log();
}

void SystemHealth::loop() {
    const unsigned long now = millis();
    if (now - lastSampleMs_ >= config::HealthSampleIntervalMs) {
        lastSampleMs_ = now;
        sample();
    }
    if (now - lastLogMs_ >= config::HealthLogIntervalMs) {
        lastLogMs_ = now;
        log();
    }
}

void SystemHealth::sample() {
    SystemState& s = state();
    s.uptimeMs = millis();
    s.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s.internalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s.dmaFree = heap_caps_get_free_size(MALLOC_CAP_DMA);
    s.dmaLargest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    s.psramFree = ESP.getFreePsram();
    s.psramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s.heapFree = s.internalFree;
    s.heapLargest = s.internalLargest;
    if (s.internalMinFree == 0 || s.internalFree < s.internalMinFree) s.internalMinFree = s.internalFree;
    if (s.dmaMinFree == 0 || s.dmaFree < s.dmaMinFree) s.dmaMinFree = s.dmaFree;
    if (s.psramMinFree == 0 || s.psramFree < s.psramMinFree) s.psramMinFree = s.psramFree;
}

void SystemHealth::log() const {
    const SystemState& s = state();
    Serial.printf(
        "[health] up=%lums internal=%lu/%lu min=%lu dma=%lu/%lu min=%lu psram=%lu/%lu min=%lu psramReady=%u extMalloc=%u/%luB wifi=%u web=%u printer=%u/%u/%u telem=%lu event=%lu ble=%u audio=%u display=%u touch=%u touches=%lu\n",
        static_cast<unsigned long>(s.uptimeMs),
        static_cast<unsigned long>(s.internalFree),
        static_cast<unsigned long>(s.internalLargest),
        static_cast<unsigned long>(s.internalMinFree),
        static_cast<unsigned long>(s.dmaFree),
        static_cast<unsigned long>(s.dmaLargest),
        static_cast<unsigned long>(s.dmaMinFree),
        static_cast<unsigned long>(s.psramFree),
        static_cast<unsigned long>(s.psramLargest),
        static_cast<unsigned long>(s.psramMinFree),
        s.psramReady ? 1 : 0,
        s.externalMallocEnabled ? 1 : 0,
        static_cast<unsigned long>(s.externalMallocThreshold),
        s.wifiConnected ? 1 : 0,
        s.webReady ? 1 : 0,
        s.printerConfigured ? 1 : 0,
        s.printerConnected ? 1 : 0,
        s.printerTelemetryValid ? 1 : 0,
        static_cast<unsigned long>(s.printerTelemetryRevision),
        static_cast<unsigned long>(s.printerStateEventSequence),
        s.bleReady ? 1 : 0,
        s.audioReady ? 1 : 0,
        s.displayReady ? 1 : 0,
        s.touchReady ? 1 : 0,
        static_cast<unsigned long>(s.touchCount));
}

void SystemHealth::checkpoint(const char* label) {
    sample();
    const SystemState& s = state();
    const int32_t internalDelta = checkpointReady_
                                      ? static_cast<int32_t>(s.internalFree) - static_cast<int32_t>(checkpointInternalFree_)
                                      : 0;
    const int32_t dmaDelta = checkpointReady_
                                 ? static_cast<int32_t>(s.dmaFree) - static_cast<int32_t>(checkpointDmaFree_)
                                 : 0;
    const int32_t psramDelta = checkpointReady_
                                   ? static_cast<int32_t>(s.psramFree) - static_cast<int32_t>(checkpointPsramFree_)
                                   : 0;

    Serial.printf(
        "[memory-step] %-14s internal=%lu (%+ld) largest=%lu dma=%lu (%+ld) largest=%lu psram=%lu (%+ld) largest=%lu\n",
        label ? label : "-",
        static_cast<unsigned long>(s.internalFree),
        static_cast<long>(internalDelta),
        static_cast<unsigned long>(s.internalLargest),
        static_cast<unsigned long>(s.dmaFree),
        static_cast<long>(dmaDelta),
        static_cast<unsigned long>(s.dmaLargest),
        static_cast<unsigned long>(s.psramFree),
        static_cast<long>(psramDelta),
        static_cast<unsigned long>(s.psramLargest));
    logHeapDiagnostics(label);

    checkpointInternalFree_ = s.internalFree;
    checkpointDmaFree_ = s.dmaFree;
    checkpointPsramFree_ = s.psramFree;
    checkpointReady_ = true;
}

}
