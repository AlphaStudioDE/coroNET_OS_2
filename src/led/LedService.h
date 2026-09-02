#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../core/ProductTypes.h"
#include "LedAnimations.h"

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
    static constexpr uint32_t SpiClockHz = 3200000;
    static constexpr uint32_t TaskStackBytes = 3584;
    static constexpr UBaseType_t TaskPriority = 8;
    static constexpr UBaseType_t BootTaskPriority = 19;
    static constexpr BaseType_t TaskCore = 1;

    static void taskEntry(void* context);
    void taskLoop();
    bool allocateBuffers();
    void render(uint32_t now);
    void renderBoot(uint32_t elapsedMs, bool full, bool performanceStarted);
    void renderCategory(LedCategory category, uint8_t animation,
                        const LedAnimationContext& context);
    void renderIdle(uint8_t animation, const LedAnimationContext& context);
    void renderPrint(uint8_t animation, const LedAnimationContext& context);
    void renderPause(uint8_t animation, const LedAnimationContext& context);
    void renderError(uint8_t animation, const LedAnimationContext& context);
    void renderFinish(uint8_t animation, const LedAnimationContext& context);
    void renderOther(uint8_t animation, const LedAnimationContext& context);
    void applyInsidePolicy();
    void applyOutputPolicies();
    bool smoothAndShow(bool immediate = false);
    void encodeFrame();
    void transmitEncodedFrame();

    void clearTarget();
    void setPhysical(uint16_t index, const RgbwColor& color);
    void setSection(LedSection section, uint16_t logical, const RgbwColor& color);
    void setOuterVisualPathPixel(uint16_t path, const RgbwColor& color);
    void fillSection(LedSection section, const RgbwColor& color);
    uint16_t sectionCount(LedSection section) const;
    uint16_t sectionPhysicalIndex(LedSection section, uint16_t logical) const;
    RgbwColor decorativeHsv(LedCategory category, uint8_t hue, uint8_t saturation,
                            uint8_t value) const;

    SPIClass* spi_ = nullptr;
    TaskHandle_t task_ = nullptr;
    RgbwColor* targetFrame_ = nullptr;
    RgbwColor* currentFrame_ = nullptr;
    uint8_t* txBuffer_ = nullptr;
    mutable portMUX_TYPE frameMux_ = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE outputMux_ = portMUX_INITIALIZER_UNLOCKED;
    bool started_ = false;
    bool bootActive_ = false;
    bool previewActive_ = false;
    bool frameMirror_ = false;
    LedCategory previewCategory_ = LedCategory::Idle;
    uint8_t previewAnimation_ = 0;
    uint32_t previewStartedMs_ = 0;
    uint32_t previewDurationMs_ = 10000;
    uint32_t previewUntilMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t shows_ = 0;
    uint32_t skippedShows_ = 0;
    UBaseType_t appliedTaskPriority_ = TaskPriority;
    uint32_t lastPrinterEventSequence_ = 0;
    bool snakeFinishActive_ = false;
    uint32_t snakeFinishStartedMs_ = 0;
    uint8_t lastPrintAnimation_ = 0xFF;
    bool snakeWasPreview_ = false;
    uint32_t snakePreviewStartedMs_ = 0;
    uint32_t snakeLastStepMs_ = 0;
    uint16_t snakeHead_ = 0;
    uint16_t snakeFood_ = 0;
    uint8_t snakeLength_ = 2;
    uint8_t snakeGrowthBucket_ = 0;
    uint32_t stabilitySampleMs_ = 0;
    float stabilityLastToolC_ = NAN;
    float stabilityLastChamberC_ = NAN;
    uint8_t stabilityJitter_ = 0;
    bool stabilityWasPreview_ = false;
};

LedService& ledService();

}
