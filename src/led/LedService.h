#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../core/ProductTypes.h"

namespace coronet {

class LedService {
public:
    void begin();
    void loop();
    bool requestPreview(LedCategory category, uint8_t animation, uint32_t durationMs = 10000);
    void cancelPreview();
    bool copyFrame(RgbwColor* output, size_t count) const;
    void logStatus() const;

private:
    static constexpr uint32_t FrameIntervalMs = 30;
    static constexpr uint32_t BootDurationMs = 35000;
    static constexpr uint32_t SpiClockHz = 3200000;
    static constexpr uint32_t TaskStackBytes = 5120;
    static constexpr UBaseType_t TaskPriority = 8;
    static constexpr BaseType_t TaskCore = 1;

    static void taskEntry(void* context);
    void taskLoop();
    bool allocateBuffers();
    void render(uint32_t now);
    void renderBoot(uint32_t elapsedMs);
    void renderCategory(LedCategory category, uint8_t animation, uint32_t now,
                        uint8_t progress, float chamberTempC, uint32_t filamentRgb);
    void renderIdle(uint8_t animation, uint32_t now);
    void renderPrint(uint8_t animation, uint32_t now, uint8_t progress,
                     float chamberTempC, uint32_t filamentRgb);
    void renderPause(uint8_t animation, uint32_t now, uint8_t progress, uint32_t filamentRgb);
    void renderError(uint8_t animation, uint32_t now);
    void renderFinish(uint8_t animation, uint32_t now, uint32_t filamentRgb);
    void renderOther(uint8_t animation, uint32_t now);
    void applyInsidePolicy();
    void applyOutputPolicies();
    bool smoothAndShow(bool immediate = false);
    void encodeFrame();

    void clearTarget();
    void setPhysical(uint16_t index, const RgbwColor& color);
    void setSection(LedSection section, uint16_t logical, const RgbwColor& color);
    void fillSection(LedSection section, const RgbwColor& color);
    uint16_t sectionCount(LedSection section) const;
    uint16_t sectionPhysicalIndex(LedSection section, uint16_t logical) const;
    RgbwColor sectionColor(LedSection section, const RgbwColor& color) const;
    RgbwColor decorativeHsv(LedCategory category, uint8_t hue, uint8_t saturation,
                            uint8_t value) const;

    SPIClass* spi_ = nullptr;
    TaskHandle_t task_ = nullptr;
    RgbwColor* targetFrame_ = nullptr;
    RgbwColor* currentFrame_ = nullptr;
    uint8_t* txBuffer_ = nullptr;
    mutable portMUX_TYPE frameMux_ = portMUX_INITIALIZER_UNLOCKED;
    bool started_ = false;
    bool bootActive_ = false;
    uint32_t bootStartedMs_ = 0;
    bool previewActive_ = false;
    LedCategory previewCategory_ = LedCategory::Idle;
    uint8_t previewAnimation_ = 0;
    uint32_t previewUntilMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t shows_ = 0;
    uint32_t skippedShows_ = 0;
};

LedService& ledService();

}
