#include "BootDiagnostics.h"

#include <esp_core_dump.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace coronet {

namespace {
BootDiagnostics gBootDiagnostics;
}

BootDiagnostics& bootDiagnostics() {
    return gBootDiagnostics;
}

void BootDiagnostics::begin() {
    snapshot_ = {};
    snapshot_.resetReason = static_cast<int32_t>(esp_reset_reason());

    size_t imageAddress = 0;
    size_t imageSize = 0;
    snapshot_.imageError = esp_core_dump_image_get(&imageAddress, &imageSize);
    snapshot_.imagePresent = snapshot_.imageError == ESP_OK && imageSize > 0;
    snapshot_.imageAddress = static_cast<uint32_t>(imageAddress);
    snapshot_.imageSize = static_cast<uint32_t>(imageSize);
    if (!snapshot_.imagePresent) {
        Serial.printf("[crash] no core dump: %s\n", esp_err_to_name(snapshot_.imageError));
        return;
    }

    snapshot_.imageError = esp_core_dump_image_check();
    snapshot_.imageValid = snapshot_.imageError == ESP_OK;
    if (!snapshot_.imageValid) {
        Serial.printf("[crash] core dump invalid: %s size=%lu\n",
                      esp_err_to_name(snapshot_.imageError),
                      static_cast<unsigned long>(snapshot_.imageSize));
        return;
    }

    auto* summary = static_cast<esp_core_dump_summary_t*>(heap_caps_calloc(
        1, sizeof(esp_core_dump_summary_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!summary) {
        snapshot_.summaryError = ESP_ERR_NO_MEM;
        Serial.println("[crash] core dump summary allocation failed");
        return;
    }

    snapshot_.summaryError = esp_core_dump_get_summary(summary);
    if (snapshot_.summaryError == ESP_OK) {
        snapshot_.summaryValid = true;
        strlcpy(snapshot_.task, summary->exc_task, sizeof(snapshot_.task));
        snapshot_.exceptionPc = summary->exc_pc;
        snapshot_.exceptionCause = summary->ex_info.exc_cause;
        snapshot_.exceptionAddress = summary->ex_info.exc_vaddr;
        const uint32_t maximumDepth =
            sizeof(snapshot_.backtrace) / sizeof(snapshot_.backtrace[0]);
        const uint32_t reportedDepth = summary->exc_bt_info.depth;
        snapshot_.backtraceDepth = static_cast<uint8_t>(
            reportedDepth < maximumDepth ? reportedDepth : maximumDepth);
        snapshot_.backtraceCorrupted = summary->exc_bt_info.corrupted;
        for (uint8_t i = 0; i < snapshot_.backtraceDepth; ++i) {
            snapshot_.backtrace[i] = summary->exc_bt_info.bt[i];
        }
        strlcpy(snapshot_.appElfSha256,
                reinterpret_cast<const char*>(summary->app_elf_sha256),
                sizeof(snapshot_.appElfSha256));
        Serial.printf("[crash] task=%s pc=0x%08lx cause=%lu address=0x%08lx depth=%u elf=%s\n",
                      snapshot_.task,
                      static_cast<unsigned long>(snapshot_.exceptionPc),
                      static_cast<unsigned long>(snapshot_.exceptionCause),
                      static_cast<unsigned long>(snapshot_.exceptionAddress),
                      static_cast<unsigned>(snapshot_.backtraceDepth),
                      snapshot_.appElfSha256);
    } else {
        Serial.printf("[crash] core dump summary failed: %s\n",
                      esp_err_to_name(snapshot_.summaryError));
    }
    heap_caps_free(summary);
}

}
