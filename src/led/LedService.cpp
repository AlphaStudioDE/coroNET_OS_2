#include "LedService.h"

#include <esp_heap_caps.h>

#include "../config/HardwareConfig.h"
#include "../boot/BootExperience.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

#if defined(HSPI)
constexpr uint8_t LedSpiHost = HSPI;
#else
constexpr uint8_t LedSpiHost = FSPI;
#endif

constexpr uint8_t NibbleLutHi[16] = {
    0x88, 0x88, 0x88, 0x88, 0x8C, 0x8C, 0x8C, 0x8C,
    0xC8, 0xC8, 0xC8, 0xC8, 0xCC, 0xCC, 0xCC, 0xCC,
};
constexpr uint8_t NibbleLutLo[16] = {
    0x88, 0x8C, 0xC8, 0xCC, 0x88, 0x8C, 0xC8, 0xCC,
    0x88, 0x8C, 0xC8, 0xCC, 0x88, 0x8C, 0xC8, 0xCC,
};
constexpr uint32_t FullLedHandoffStartMs = 31800U;
constexpr uint32_t QuickLedHandoffStartMs = 2700U;

LedService gLedService;

constexpr LedSection VisualOuterSections[3] = {
    LedSection::Left,
    LedSection::Center,
    LedSection::Right,
};

constexpr LedSection VisualAllSections[4] = {
    LedSection::Left,
    LedSection::Center,
    LedSection::Right,
    LedSection::Inside,
};

constexpr uint16_t mappedSectionIndex(LedSection section, uint16_t logical, bool mirror) {
    if (mirror) {
        switch (section) {
            case LedSection::Left: return hw::RightStart + logical;
            case LedSection::Center: return hw::CenterStart + logical;
            case LedSection::Right: return hw::LeftEnd - logical;
            case LedSection::Inside: return hw::InsideEnd - logical;
            default: return 0;
        }
    }
    switch (section) {
        case LedSection::Left: return hw::LeftEnd - logical;
        case LedSection::Center: return hw::CenterEnd - logical;
        case LedSection::Right: return hw::RightStart + logical;
        case LedSection::Inside: return hw::InsideStart + logical;
        default: return 0;
    }
}

static_assert(mappedSectionIndex(LedSection::Right, 0, false) == 0);
static_assert(mappedSectionIndex(LedSection::Right, hw::RightCount - 1U, false) == 10);
static_assert(mappedSectionIndex(LedSection::Center, 0, false) == 30);
static_assert(mappedSectionIndex(LedSection::Center, hw::CenterCount - 1U, false) == 11);
static_assert(mappedSectionIndex(LedSection::Left, 0, false) == 41);
static_assert(mappedSectionIndex(LedSection::Left, hw::LeftCount - 1U, false) == 31);
static_assert(mappedSectionIndex(LedSection::Inside, 0, false) == 42);
static_assert(mappedSectionIndex(LedSection::Inside, hw::InsideCount - 1U, false) == 59);
static_assert(mappedSectionIndex(LedSection::Left, 0, true) == 0);
static_assert(mappedSectionIndex(LedSection::Center, 0, true) == 11);
static_assert(mappedSectionIndex(LedSection::Right, 0, true) == 41);
static_assert(mappedSectionIndex(LedSection::Inside, 0, true) == 59);

uint8_t clampByte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

uint8_t wave8(uint8_t phase) {
    const uint8_t triangle = phase < 128 ? static_cast<uint8_t>(phase * 2U)
                                         : static_cast<uint8_t>((255U - phase) * 2U);
    return static_cast<uint8_t>((static_cast<uint16_t>(triangle) * triangle + 255U) / 255U);
}

uint8_t hash8(uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return static_cast<uint8_t>(value);
}

RgbwColor hsv(uint8_t hue, uint8_t saturation, uint8_t value) {
    if (saturation == 0) return RgbwColor(value, value, value, 0);
    const uint8_t region = hue / 43U;
    const uint8_t remainder = static_cast<uint8_t>((hue - region * 43U) * 6U);
    const uint8_t p = static_cast<uint8_t>((static_cast<uint16_t>(value) * (255U - saturation)) >> 8U);
    const uint8_t q = static_cast<uint8_t>((static_cast<uint16_t>(value) *
                                            (255U - ((static_cast<uint16_t>(saturation) * remainder) >> 8U))) >> 8U);
    const uint8_t t = static_cast<uint8_t>((static_cast<uint16_t>(value) *
                                            (255U - ((static_cast<uint16_t>(saturation) * (255U - remainder)) >> 8U))) >> 8U);
    switch (region) {
        case 0: return RgbwColor(value, t, p);
        case 1: return RgbwColor(q, value, p);
        case 2: return RgbwColor(p, value, t);
        case 3: return RgbwColor(p, q, value);
        case 4: return RgbwColor(t, p, value);
        default: return RgbwColor(value, p, q);
    }
}

RgbwColor fromRgb(uint32_t rgb) {
    return RgbwColor(static_cast<uint8_t>(rgb >> 16U),
                     static_cast<uint8_t>(rgb >> 8U),
                     static_cast<uint8_t>(rgb));
}

RgbwColor complementary(const RgbwColor& color) {
    const uint8_t maximum = max(color.r, max(color.g, color.b));
    const uint8_t minimum = min(color.r, min(color.g, color.b));
    if (maximum < 28 || maximum - minimum < 24) return RgbwColor();
    return RgbwColor(static_cast<uint8_t>(255U - color.r),
                     static_cast<uint8_t>(255U - color.g),
                     static_cast<uint8_t>(255U - color.b));
}

bool rgbHue(const RgbwColor& color, uint8_t& hue) {
    const uint8_t maximum = max(color.r, max(color.g, color.b));
    const uint8_t minimum = min(color.r, min(color.g, color.b));
    const uint8_t delta = maximum - minimum;
    if (maximum < 25U || delta < 20U) {
        hue = 0U;
        return false;
    }

    int16_t value = 0;
    if (maximum == color.r) {
        value = static_cast<int16_t>(43L * (static_cast<int16_t>(color.g) - color.b) / delta);
    } else if (maximum == color.g) {
        value = static_cast<int16_t>(85 + 43L * (static_cast<int16_t>(color.b) - color.r) / delta);
    } else {
        value = static_cast<int16_t>(171 + 43L * (static_cast<int16_t>(color.r) - color.g) / delta);
    }
    if (value < 0) value += 256;
    hue = static_cast<uint8_t>(value);
    return true;
}

uint8_t progressCoverage(uint8_t progress, uint16_t count, uint16_t index) {
    if (!count || index >= count) return 0U;
    progress = min<uint8_t>(progress, 100U);
    const uint32_t edge = static_cast<uint32_t>(progress) * count * 255U / 100U;
    const uint32_t pixelStart = static_cast<uint32_t>(index) * 255U;
    if (edge <= pixelStart) return 0U;
    return static_cast<uint8_t>(min<uint32_t>(255U, edge - pixelStart));
}

RgbwColor blend(const RgbwColor& a, const RgbwColor& b, uint8_t amount) {
    const uint16_t inverse = 255U - amount;
    return RgbwColor(
        static_cast<uint8_t>((static_cast<uint16_t>(a.r) * inverse + static_cast<uint16_t>(b.r) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.g) * inverse + static_cast<uint16_t>(b.g) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.b) * inverse + static_cast<uint16_t>(b.b) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.w) * inverse + static_cast<uint16_t>(b.w) * amount + 127U) / 255U));
}

uint8_t approachChannel(uint8_t current, uint8_t target, uint8_t amount) {
    if (current == target || amount == 255U) return target;

    const uint8_t distance = current > target ? current - target : target - current;
    const uint8_t step = max<uint8_t>(
        1U, static_cast<uint8_t>((static_cast<uint16_t>(distance) * amount + 127U) / 255U));
    if (current < target) {
        return static_cast<uint8_t>(current + min<uint8_t>(distance, step));
    }
    return static_cast<uint8_t>(current - min<uint8_t>(distance, step));
}

RgbwColor approach(const RgbwColor& current, const RgbwColor& target, uint8_t amount) {
    return RgbwColor(approachChannel(current.r, target.r, amount),
                     approachChannel(current.g, target.g, amount),
                     approachChannel(current.b, target.b, amount),
                     approachChannel(current.w, target.w, amount));
}

RgbwColor scaled(const RgbwColor& color, uint8_t scale) {
    return RgbwColor(
        static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.w) * scale + 127U) / 255U));
}

uint8_t temperaturePercent(float temperature, float minimum, float maximum,
                           uint8_t fallback) {
    if (isnan(temperature) || maximum <= minimum) return fallback;
    if (temperature <= minimum) return 0U;
    if (temperature >= maximum) return 100U;
    return static_cast<uint8_t>((temperature - minimum) * 100.0f / (maximum - minimum));
}

RgbwColor temperatureColor(uint8_t percent, uint8_t value = 255U) {
    percent = min<uint8_t>(percent, 100U);
    if (percent <= 33U) {
        return scaled(blend(RgbwColor(0, 35, 255), RgbwColor(0, 235, 210),
                            static_cast<uint8_t>(percent * 255U / 33U)), value);
    }
    if (percent <= 67U) {
        return scaled(blend(RgbwColor(0, 235, 210), RgbwColor(255, 150, 0),
                            static_cast<uint8_t>((percent - 33U) * 255U / 34U)), value);
    }
    return scaled(blend(RgbwColor(255, 150, 0), RgbwColor(255, 0, 0),
                        static_cast<uint8_t>((percent - 67U) * 255U / 33U)), value);
}

LedCategory categoryForState(const SystemState& system) {
    switch (system.printerState) {
        case PrinterState::Printing: return LedCategory::Print;
        case PrinterState::Paused: return LedCategory::Pause;
        case PrinterState::Error: return LedCategory::Error;
        case PrinterState::Complete: return LedCategory::Finish;
        case PrinterState::Unknown:
        case PrinterState::Idle:
        default: return LedCategory::Idle;
    }
}

bool quietSuppressesLeds(const AppSettings& settings, const SystemState& system) {
    if (!system.quietActive) return false;
    if (settings.quietErrorsBypass && system.printerState == PrinterState::Error) return false;
    return settings.quietTarget == QuietTarget::Leds ||
           settings.quietTarget == QuietTarget::SoundAndLeds;
}

}

LedService& ledService() {
    return gLedService;
}

void LedService::begin() {
    state().ledReady = false;
    if (!allocateBuffers()) {
        Serial.println("[led] buffer allocation failed");
        return;
    }

    spi_ = new SPIClass(LedSpiHost);
    if (!spi_) {
        Serial.println("[led] SPI object allocation failed");
        return;
    }
    pinMode(hw::LedDataPin, OUTPUT);
    digitalWrite(hw::LedDataPin, LOW);
    spi_->begin(hw::LedSpiSckDummyPin, -1, hw::LedDataPin, -1);
    spi_->beginTransaction(SPISettings(SpiClockHz, MSBFIRST, SPI_MODE0));

    clearTarget();
    memset(currentFrame_, 0, sizeof(RgbwColor) * hw::LedCount);
    encodeFrame();
    transmitEncodedFrame();

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry, "coronet-led", TaskStackBytes, this, TaskPriority, &task_, TaskCore);
    if (created != pdPASS) {
        Serial.println("[led] task creation failed");
        return;
    }

    bootActive_ = bootExperience().active();
    started_ = true;
    state().ledReady = true;
    Serial.printf("[led] ready SPI=%luHz frame=%lums buffers: psram=%uB internal=%uB\n",
                  static_cast<unsigned long>(SpiClockHz),
                  static_cast<unsigned long>(FrameIntervalMs),
                  static_cast<unsigned>(sizeof(RgbwColor) * hw::LedCount * 2U),
                  static_cast<unsigned>(hw::LedCount * 16U));
}

void LedService::loop() {
    if (previewActive_ && static_cast<int32_t>(millis() - previewUntilMs_) >= 0) {
        previewActive_ = false;
    }
}

bool LedService::requestPreview(LedCategory category, uint8_t animation, uint32_t durationMs) {
    if (!started_ || category >= LedCategory::Count) return false;
    if (durationMs < 1000U) durationMs = 1000U;
    if (durationMs > 30000U) durationMs = 30000U;
    previewCategory_ = category;
    previewAnimation_ = normalizeLedAnimation(category, animation);
    previewStartedMs_ = millis();
    previewDurationMs_ = durationMs;
    previewUntilMs_ = previewStartedMs_ + durationMs;
    previewActive_ = true;
    return true;
}

void LedService::cancelPreview() {
    previewActive_ = false;
}

bool LedService::copyFrame(RgbwColor* output, size_t count) const {
    if (!output || !currentFrame_ || count < hw::LedCount) return false;
    portENTER_CRITICAL(&frameMux_);
    memcpy(output, currentFrame_, sizeof(RgbwColor) * hw::LedCount);
    portEXIT_CRITICAL(&frameMux_);
    return true;
}

void LedService::logStatus() const {
    const UBaseType_t stackHeadroom = task_ ? uxTaskGetStackHighWaterMark(task_) : 0;
    Serial.printf("[led] ready=%u boot=%u preview=%u mirror=%u shows=%lu skipped=%lu frames=%lu dropped=%lu stackHeadroom=%uB\n",
                  started_ ? 1U : 0U, bootActive_ ? 1U : 0U, previewActive_ ? 1U : 0U,
                  settingsService().settings().mirrorLedLayout ? 1U : 0U,
                  static_cast<unsigned long>(shows_), static_cast<unsigned long>(skippedShows_),
                  static_cast<unsigned long>(state().ledFrameCount),
                  static_cast<unsigned long>(state().ledDroppedFrames),
                  static_cast<unsigned>(stackHeadroom));
}

void LedService::taskEntry(void* context) {
    static_cast<LedService*>(context)->taskLoop();
}

void LedService::taskLoop() {
    TickType_t wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(FrameIntervalMs);
    for (;;) {
        const uint32_t now = millis();
        const UBaseType_t desiredPriority = bootExperience().active()
            ? BootTaskPriority : TaskPriority;
        if (desiredPriority != appliedTaskPriority_) {
            vTaskPrioritySet(nullptr, desiredPriority);
            appliedTaskPriority_ = desiredPriority;
        }
        render(now);
        ++state().ledFrameCount;
        lastFrameMs_ = now;
        vTaskDelayUntil(&wake, interval);
    }
}

bool LedService::allocateBuffers() {
    targetFrame_ = static_cast<RgbwColor*>(heap_caps_calloc(
        hw::LedCount, sizeof(RgbwColor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    currentFrame_ = static_cast<RgbwColor*>(heap_caps_calloc(
        hw::LedCount, sizeof(RgbwColor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    txBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
        hw::LedCount * 16U, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return targetFrame_ && currentFrame_ && txBuffer_;
}

void LedService::render(uint32_t now) {
    clearTarget();
    const AppSettings& settings = settingsService().settings();
    const SystemState& system = state();
    frameMirror_ = settings.mirrorLedLayout;

    if (system.printerStateEventSequence != lastPrinterEventSequence_) {
        lastPrinterEventSequence_ = system.printerStateEventSequence;
        const uint8_t selectedPrintAnimation = normalizeLedAnimation(
            LedCategory::Print,
            settings.ledAnimation[static_cast<uint8_t>(LedCategory::Print)]);
        if (settings.ledEnabled && !settings.ledOtherMode &&
            system.printerEventTo == PrinterState::Complete &&
            (system.printerEventFrom == PrinterState::Printing ||
             system.printerEventFrom == PrinterState::Paused) &&
            selectedPrintAnimation == static_cast<uint8_t>(PrintAnimation::Snake)) {
            snakeFinishActive_ = true;
            snakeFinishStartedMs_ = now;
        } else {
            snakeFinishActive_ = false;
        }
    }
    if (snakeFinishActive_ && now - snakeFinishStartedMs_ >= SnakeFinishDurationMs) {
        snakeFinishActive_ = false;
    }

    if (!settings.ledEnabled || quietSuppressesLeds(settings, system)) {
        bootActive_ = false;
        smoothAndShow();
        return;
    }

    bootActive_ = bootExperience().active();
    const uint32_t bootElapsed = bootExperience().timelineMs();
    if (bootActive_) {
        renderBoot(bootExperience().performanceStarted() ? bootElapsed : bootExperience().preludeMs(),
                   bootExperience().full(), bootExperience().performanceStarted());
    } else {
        const bool preview = !snakeFinishActive_ && previewActive_ &&
                             static_cast<int32_t>(previewUntilMs_ - now) > 0;
        const LedCategory category = snakeFinishActive_ ? LedCategory::Print
            : (preview ? previewCategory_
                       : (settings.ledOtherMode ? LedCategory::Other : categoryForState(system)));
        const uint8_t animation = snakeFinishActive_
            ? static_cast<uint8_t>(PrintAnimation::Snake)
            : (preview ? previewAnimation_
                       : settings.ledAnimation[static_cast<uint8_t>(category)]);
        LedAnimationContext context;
        context.nowMs = now;
        context.progress = system.printProgress;
        context.activeTool = system.activeTool;
        context.activeToolTempC = system.activeToolTempC;
        context.bedTempC = system.bedTempC;
        context.chamberTempC = system.chamberTempC;
        context.filamentRgb = system.filamentColorRgb;
        memcpy(context.filamentColorsRgb, system.filamentColorsRgb,
               sizeof(context.filamentColorsRgb));
        context.filamentColorMask = system.filamentColorMask;
        context.printDurationSec = system.printDurationSec;
        context.printEtaSec = system.printEtaSec;
        context.printerOnline = system.printerConnected && system.printerTelemetryValid;
        context.ventFailsafe = system.ventFailsafe;
        context.printerTelemetryAgeMs = system.lastPrinterUpdateMs
            ? now - system.lastPrinterUpdateMs : UINT32_MAX;
        context.preview = preview;
        context.finishing = snakeFinishActive_;
        if (snakeFinishActive_) context.progress = 100U;
        if (preview) {
            const uint32_t elapsed = now - previewStartedMs_;
            context.progress = static_cast<uint8_t>(min<uint32_t>(100U,
                3U + elapsed * 94U / max<uint32_t>(1000U, previewDurationMs_)));
            context.activeTool = 1U;
            context.activeToolTempC = 205.0f + static_cast<float>(wave8(now / 31U)) * 25.0f / 255.0f;
            context.bedTempC = 55.0f + static_cast<float>(wave8(now / 39U + 67U)) * 10.0f / 255.0f;
            context.chamberTempC = 32.0f + static_cast<float>(wave8(now / 47U + 121U)) * 18.0f / 255.0f;
            context.filamentRgb = 0xFF7A00UL;
            context.filamentColorsRgb[0] = 0xFF7A00UL;
            context.filamentColorsRgb[1] = 0x00C8FFUL;
            context.filamentColorsRgb[2] = 0x78E060UL;
            context.filamentColorsRgb[3] = 0xD050FFUL;
            context.filamentColorMask = 0x0FU;
            context.printDurationSec = 4260U;
            context.printEtaSec = 1740U;
            context.printerOnline = true;
            context.ventFailsafe = false;
            context.printerTelemetryAgeMs = 0U;
        }
        renderCategory(category, animation, context);
        applyInsidePolicy();
    }

    applyOutputPolicies();
    smoothAndShow(bootActive_ && bootElapsed < 300U);
}

void LedService::renderBoot(uint32_t elapsedMs, bool full, bool performanceStarted) {
    if (!performanceStarted) {
        const uint8_t breath = static_cast<uint8_t>(10U + wave8(static_cast<uint8_t>(elapsedMs / 20U)) / 5U);
        for (uint16_t i = 0; i < hw::InsideCount; ++i) {
            const uint16_t distance = i > hw::InsideCount / 2U ? i - hw::InsideCount / 2U
                                                               : hw::InsideCount / 2U - i;
            const uint8_t value = distance < 5U ? static_cast<uint8_t>(breath * (6U - distance) / 6U) : 0U;
            setSection(LedSection::Inside, i, hsv(126U, 235U, value));
        }
        return;
    }

    if (!full) {
        const uint8_t reveal = static_cast<uint8_t>(min<uint32_t>(255U, elapsedMs * 255U / 1050U));
        const uint8_t handoff = elapsedMs > QuickLedHandoffStartMs
            ? static_cast<uint8_t>(min<uint32_t>(255U,
                (elapsedMs - QuickLedHandoffStartMs) * 255U /
                (BootExperience::QuickDurationMs - QuickLedHandoffStartMs))) : 0U;
        RgbwColor signature[hw::LedCount] = {};
        for (uint16_t i = 0; i < hw::InsideCount; ++i) {
            const uint16_t distance = i > hw::InsideCount / 2U ? i - hw::InsideCount / 2U
                                                               : hw::InsideCount / 2U - i;
            const uint8_t value = distance * 24U < reveal ? static_cast<uint8_t>(180U - min<uint16_t>(150U, distance * 18U)) : 0U;
            const uint8_t hue = static_cast<uint8_t>(
                static_cast<uint32_t>(i) * 255U / (hw::InsideCount - 1U));
            signature[sectionPhysicalIndex(LedSection::Inside, i)] = hsv(hue, 235U, value);
        }
        for (uint16_t path = 0; path < hw::OuterCount; ++path) {
            const uint16_t distance = path > hw::OuterCount / 2U ? path - hw::OuterCount / 2U
                                                                 : hw::OuterCount / 2U - path;
            if (distance * 12U <= reveal) {
                uint16_t sectionPosition = path;
                uint16_t sectionSize = hw::LeftCount;
                if (path >= hw::LeftCount + hw::CenterCount) {
                    sectionPosition = path - hw::LeftCount - hw::CenterCount;
                    sectionSize = hw::RightCount;
                } else if (path >= hw::LeftCount) {
                    sectionPosition = path - hw::LeftCount;
                    sectionSize = hw::CenterCount;
                }
                const uint8_t hue = static_cast<uint8_t>(
                    static_cast<uint32_t>(sectionPosition) * 255U / (sectionSize - 1U));
                RgbwColor color = hsv(hue, 230U, static_cast<uint8_t>(80U + reveal / 2U));
                if (path < hw::LeftCount) signature[sectionPhysicalIndex(LedSection::Left, path)] = color;
                else if (path < hw::LeftCount + hw::CenterCount) signature[sectionPhysicalIndex(LedSection::Center, path - hw::LeftCount)] = color;
                else signature[sectionPhysicalIndex(LedSection::Right, hw::RightCount - 1U - (path - hw::LeftCount - hw::CenterCount))] = color;
            }
        }
        if (handoff) {
            const SystemState& system = state();
            const AppSettings& settings = settingsService().settings();
            const LedCategory category = settings.ledOtherMode
                ? LedCategory::Other : categoryForState(system);
            LedAnimationContext context;
            context.nowMs = millis();
            context.progress = system.printProgress;
            context.activeTool = system.activeTool;
            context.activeToolTempC = system.activeToolTempC;
            context.bedTempC = system.bedTempC;
            context.chamberTempC = system.chamberTempC;
            context.filamentRgb = system.filamentColorRgb;
            memcpy(context.filamentColorsRgb, system.filamentColorsRgb,
                   sizeof(context.filamentColorsRgb));
            context.filamentColorMask = system.filamentColorMask;
            context.printDurationSec = system.printDurationSec;
            context.printEtaSec = system.printEtaSec;
            context.printerOnline = system.printerConnected && system.printerTelemetryValid;
            context.ventFailsafe = system.ventFailsafe;
            context.printerTelemetryAgeMs = system.lastPrinterUpdateMs
                ? context.nowMs - system.lastPrinterUpdateMs : UINT32_MAX;
            renderCategory(category,
                           settings.ledAnimation[static_cast<uint8_t>(category)], context);
            applyInsidePolicy();
        }
        for (uint16_t i = 0; i < hw::LedCount; ++i) {
            targetFrame_[i] = handoff ? blend(signature[i], targetFrame_[i], handoff) : signature[i];
        }
        return;
    }

    auto ease = [](uint32_t value, uint32_t duration) -> uint8_t {
        if (duration == 0U || value >= duration) return 255U;
        const uint32_t x = value * 255U / duration;
        return static_cast<uint8_t>((x * x * (765U - 2U * x) + 32512U) / 65025U);
    };
    auto envelope = [&](uint32_t start, uint32_t fadeIn, uint32_t end, uint32_t fadeOut) -> uint8_t {
        if (elapsedMs <= start || elapsedMs >= end) return 0U;
        if (fadeIn && elapsedMs < start + fadeIn) return ease(elapsedMs - start, fadeIn);
        if (fadeOut && elapsedMs > end - fadeOut) return static_cast<uint8_t>(255U - ease(elapsedMs - (end - fadeOut), fadeOut));
        return 255U;
    };
    auto saturatingAdd = [](const RgbwColor& base, const RgbwColor& addition) -> RgbwColor {
        return RgbwColor(
            static_cast<uint8_t>(min<uint16_t>(255U, static_cast<uint16_t>(base.r) + addition.r)),
            static_cast<uint8_t>(min<uint16_t>(255U, static_cast<uint16_t>(base.g) + addition.g)),
            static_cast<uint8_t>(min<uint16_t>(255U, static_cast<uint16_t>(base.b) + addition.b)),
            static_cast<uint8_t>(min<uint16_t>(255U, static_cast<uint16_t>(base.w) + addition.w)));
    };
    auto outerPhysical = [&](uint16_t path) -> uint16_t {
        if (path < hw::LeftCount) return sectionPhysicalIndex(LedSection::Left, path);
        path -= hw::LeftCount;
        if (path < hw::CenterCount) return sectionPhysicalIndex(LedSection::Center, path);
        path -= hw::CenterCount;
        return sectionPhysicalIndex(LedSection::Right, hw::RightCount - 1U - path);
    };
    auto addOuter = [&](uint16_t path, const RgbwColor& color, uint8_t power) {
        if (path >= hw::OuterCount || power == 0U) return;
        const uint16_t physical = outerPhysical(path);
        targetFrame_[physical] = saturatingAdd(targetFrame_[physical], scaled(color, power));
    };
    auto addComet = [&](uint16_t head, const RgbwColor& color, uint8_t power,
                        uint8_t tailLength, bool reverse) {
        for (uint8_t tail = 0; tail < tailLength; ++tail) {
            const uint16_t path = reverse
                ? static_cast<uint16_t>((head + tail) % hw::OuterCount)
                : static_cast<uint16_t>((head + hw::OuterCount - tail) % hw::OuterCount);
            addOuter(path, color, static_cast<uint8_t>(
                static_cast<uint16_t>(power) * (tailLength - tail) / tailLength));
        }
    };

    const RgbwColor violet(66U, 0U, 160U);
    const RgbwColor cyan(0U, 205U, 255U);
    const RgbwColor orange(255U, 72U, 0U);
    const RgbwColor white(255U, 255U, 255U);
    const uint8_t rise = ease(elapsedMs, 9000U);
    const uint8_t resonanceEnv = envelope(1200U, 2600U, 16600U, 4200U);
    const uint8_t orbitEnv = envelope(6500U, 4200U, 31800U, 3200U);
    const uint8_t spectrumEnv = envelope(11800U, 2800U, 30500U, 2500U);
    const uint8_t powerEnv = envelope(22000U, 1900U, 31500U, 1800U);

    RgbwColor engineColor;
    if (elapsedMs < 4500U) engineColor = blend(violet, cyan, ease(elapsedMs, 4500U));
    else if (elapsedMs < 9000U) engineColor = blend(cyan, orange, ease(elapsedMs - 4500U, 4500U));
    else if (elapsedMs < 13500U) engineColor = blend(orange, violet, ease(elapsedMs - 9000U, 4500U));
    else if (elapsedMs < 20500U) engineColor = blend(violet, cyan, ease(elapsedMs - 13500U, 7000U));
    else engineColor = blend(cyan, orange, ease(elapsedMs - 20500U, 6500U));

    // A single breathing core remains visible throughout the show. Every later
    // movement grows out of this rhythm instead of replacing it with a new scene.
    const uint32_t breathDivisor = 42U - static_cast<uint32_t>(rise) * 24U / 255U;
    const uint8_t coreBreath = wave8(static_cast<uint8_t>(elapsedMs / max<uint32_t>(18U, breathDivisor)));
    for (uint16_t i = 0; i < hw::InsideCount; ++i) {
        const uint8_t phase = static_cast<uint8_t>(elapsedMs / 24U + i * 17U);
        const uint8_t localWave = wave8(phase);
        uint8_t value = static_cast<uint8_t>(12U + rise / 4U + coreBreath / 4U + localWave / 7U);
        if (powerEnv) value = static_cast<uint8_t>(min<uint16_t>(225U, value + static_cast<uint16_t>(powerEnv) * (35U + localWave / 5U) / 255U));
        const RgbwColor local = blend(engineColor, hsv(static_cast<uint8_t>(elapsedMs / 17U + i * 12U), 245U, 255U),
                                      static_cast<uint8_t>(static_cast<uint16_t>(spectrumEnv) * (35U + localWave / 6U) / 255U));
        setSection(LedSection::Inside, i, scaled(local, value));
    }

    // Low-frequency aura: deliberately dim and nearly monochromatic. It is the
    // connective tissue under the waves, not a full-ring rainbow effect.
    for (uint16_t path = 0; path < hw::OuterCount; ++path) {
        const uint8_t aura = wave8(static_cast<uint8_t>(elapsedMs / 49U + path * 8U));
        const uint8_t value = static_cast<uint8_t>((5U + aura / 18U) * (65U + rise / 2U) / 255U);
        setOuterVisualPathPixel(path, scaled(engineColor, value));
    }

    // Mirrored ignition waves leave the center as one movement, gradually curl
    // around the outer path, and then tighten into the orbit below.
    if (resonanceEnv) {
        const uint16_t center = hw::OuterCount / 2U;
        const uint16_t travel = static_cast<uint16_t>((elapsedMs * (hw::OuterCount + 16UL)) / 5200UL);
        for (uint8_t tail = 0; tail < 11U; ++tail) {
            const uint16_t offset = (travel + hw::OuterCount - tail) % hw::OuterCount;
            const uint16_t left = (center + hw::OuterCount - offset) % hw::OuterCount;
            const uint16_t right = (center + offset) % hw::OuterCount;
            const uint8_t power = static_cast<uint8_t>(static_cast<uint16_t>(resonanceEnv) * (11U - tail) / 11U);
            addOuter(left, engineColor, power);
            addOuter(right, engineColor, power);
        }
    }

    if (orbitEnv) {
        const uint32_t period = 1760U - static_cast<uint32_t>(powerEnv) * 820U / 255U;
        const uint16_t head = static_cast<uint16_t>((elapsedMs * hw::OuterCount) / max<uint32_t>(760U, period));
        const uint8_t cometPower = static_cast<uint8_t>(static_cast<uint16_t>(orbitEnv) * (175U + powerEnv / 4U) / 255U);
        addComet(head % hw::OuterCount, blend(engineColor, cyan, 80U), cometPower, 10U, false);
        addComet((head + hw::OuterCount / 2U) % hw::OuterCount,
                 blend(engineColor, orange, 105U), static_cast<uint8_t>(cometPower * 9U / 10U), 9U, true);
    }

    // The spectrum is revealed as moving energy ribbons. Every color appears,
    // but dark valleys and bright crests preserve direction, force and depth.
    if (spectrumEnv) {
        const uint8_t drift = static_cast<uint8_t>(elapsedMs / 34U);
        for (uint16_t path = 0; path < hw::OuterCount; ++path) {
            const uint8_t position = static_cast<uint8_t>(path * 255U / hw::OuterCount);
            const uint8_t ribbonA = wave8(static_cast<uint8_t>(position * 2U - elapsedMs / 12U));
            const uint8_t ribbonB = wave8(static_cast<uint8_t>(position * 3U + elapsedMs / 18U));
            const uint8_t crest = max(ribbonA, static_cast<uint8_t>(ribbonB * 4U / 5U));
            const uint8_t shaped = static_cast<uint8_t>(static_cast<uint16_t>(crest) * crest / 255U);
            const uint8_t value = static_cast<uint8_t>(static_cast<uint16_t>(spectrumEnv) * (9U + shaped * 105U / 255U) / 255U);
            addOuter(path, hsv(static_cast<uint8_t>(position + drift), 255U, 255U), value);
        }
    }

    // The music briefly inhales around 20.5 s. Scaling the complete layered
    // frame makes all established motion contract together before full power.
    uint8_t masterScale = 255U;
    if (elapsedMs >= 20400U && elapsedMs < 22000U) {
        const uint32_t phase = elapsedMs - 20400U;
        masterScale = phase < 800U
            ? static_cast<uint8_t>(255U - static_cast<uint16_t>(ease(phase, 800U)) * 170U / 255U)
            : static_cast<uint8_t>(85U + static_cast<uint16_t>(ease(phase - 800U, 800U)) * 170U / 255U);
    }
    if (masterScale < 255U) {
        for (uint16_t i = 0; i < hw::LedCount; ++i) targetFrame_[i] = scaled(targetFrame_[i], masterScale);
    }

    // Full power adds a third orbit and restrained beat surges. These reinforce
    // the existing engine motion instead of flashing unrelated pixels.
    if (powerEnv) {
        const uint16_t fastHead = static_cast<uint16_t>((elapsedMs * hw::OuterCount) / 690U);
        addComet((fastHead + hw::OuterCount / 3U) % hw::OuterCount,
                 hsv(static_cast<uint8_t>(elapsedMs / 15U + 90U), 255U, 255U),
                 static_cast<uint8_t>(static_cast<uint16_t>(powerEnv) * 205U / 255U), 8U, false);
        const uint16_t beatPhase = static_cast<uint16_t>((elapsedMs - 22000U) % 840U);
        const uint8_t beat = beatPhase < 210U ? static_cast<uint8_t>(255U - beatPhase * 255U / 210U) : 0U;
        if (beat) {
            const uint16_t center = hw::OuterCount / 2U;
            const uint8_t reach = static_cast<uint8_t>(2U + static_cast<uint16_t>(beat) * 7U / 255U);
            for (uint8_t d = 0; d < reach; ++d) {
                const uint8_t accent = static_cast<uint8_t>(static_cast<uint16_t>(beat) * (reach - d) / reach);
                addOuter((center + d) % hw::OuterCount, engineColor, accent / 2U);
                addOuter((center + hw::OuterCount - d) % hw::OuterCount, engineColor, accent / 2U);
            }
        }
    }

    // White is a short, symmetric finale gate. It resolves the accumulated
    // color energy, then immediately becomes the live-status crossfade.
    const uint8_t whiteGate = envelope(29200U, 1700U, 32500U, 900U);
    if (whiteGate) {
        const uint16_t center = hw::OuterCount / 2U;
        const uint16_t spread = static_cast<uint16_t>(static_cast<uint32_t>(whiteGate) * center / 255U);
        for (uint16_t path = 0; path < hw::OuterCount; ++path) {
            const uint16_t distance = path > center ? path - center : center - path;
            if (distance <= spread) {
                const uint8_t edge = static_cast<uint8_t>((spread + 1U - distance) * 255U / (spread + 1U));
                addOuter(path, white, static_cast<uint8_t>(static_cast<uint16_t>(edge) * 120U / 255U));
            }
        }
    }

    if (elapsedMs >= FullLedHandoffStartMs) {
        const uint8_t handoff = ease(elapsedMs - FullLedHandoffStartMs,
                                     BootExperience::FullDurationMs - FullLedHandoffStartMs);
        RgbwColor signature[hw::LedCount];
        memcpy(signature, targetFrame_, sizeof(signature));
        const SystemState& system = state();
        const AppSettings& settings = settingsService().settings();
        const LedCategory category = settings.ledOtherMode
            ? LedCategory::Other : categoryForState(system);
        LedAnimationContext context;
        context.nowMs = millis();
        context.progress = system.printProgress;
        context.activeTool = system.activeTool;
        context.activeToolTempC = system.activeToolTempC;
        context.bedTempC = system.bedTempC;
        context.chamberTempC = system.chamberTempC;
        context.filamentRgb = system.filamentColorRgb;
        memcpy(context.filamentColorsRgb, system.filamentColorsRgb,
               sizeof(context.filamentColorsRgb));
        context.filamentColorMask = system.filamentColorMask;
        context.printDurationSec = system.printDurationSec;
        context.printEtaSec = system.printEtaSec;
        context.printerOnline = system.printerConnected && system.printerTelemetryValid;
        context.ventFailsafe = system.ventFailsafe;
        context.printerTelemetryAgeMs = system.lastPrinterUpdateMs
            ? context.nowMs - system.lastPrinterUpdateMs : UINT32_MAX;
        renderCategory(category,
                       settings.ledAnimation[static_cast<uint8_t>(category)], context);
        applyInsidePolicy();
        for (uint16_t i = 0; i < hw::LedCount; ++i) targetFrame_[i] = blend(signature[i], targetFrame_[i], handoff);
    }
}

void LedService::renderCategory(LedCategory category, uint8_t animation,
                                const LedAnimationContext& context) {
    animation = normalizeLedAnimation(category, animation);
    switch (category) {
        case LedCategory::Print: renderPrint(animation, context); break;
        case LedCategory::Pause: renderPause(animation, context); break;
        case LedCategory::Error: renderError(animation, context.nowMs); break;
        case LedCategory::Finish: renderFinish(animation, context.nowMs, context.filamentRgb); break;
        case LedCategory::Other: renderOther(animation, context.nowMs); break;
        case LedCategory::Idle:
        default: renderIdle(animation, context.nowMs); break;
    }
}

void LedService::renderIdle(uint8_t animation, uint32_t now) {
    switch (animation % 4U) {
        case 1: {
            const uint16_t head = static_cast<uint16_t>((now / 90U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > head ? path - head : head - path;
                if (distance > 5U) continue;
                const RgbwColor color = decorativeHsv(LedCategory::Idle,
                    static_cast<uint8_t>(now / 24U + path * 5U), 230,
                    static_cast<uint8_t>((6U - distance) * 35U));
                setOuterVisualPathPixel(path, color);
            }
            break;
        }
        case 2: {
            const float temp = isnan(state().chamberTempC) ? 25.0f : state().chamberTempC;
            const uint8_t hot = clampByte(static_cast<int>((temp - 20.0f) * 255.0f / 40.0f));
            const RgbwColor thermal = blend(RgbwColor(0, 70, 255), RgbwColor(255, 20, 0), hot);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t pulse = static_cast<uint8_t>(50U + wave8(static_cast<uint8_t>(now / 24U + i * 12U)) / 2U);
                    setSection(section, i, scaled(thermal, pulse));
                }
            }
            break;
        }
        case 3:
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t spark = hash8(i * 97U + sectionIndex * 701U + now / 180U);
                    if (spark > 232U) setSection(section, i, decorativeHsv(LedCategory::Idle, spark + now / 30U, 210, spark));
                }
            }
            break;
        case 0:
        default:
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t phase = static_cast<uint8_t>(now / 28U + i * 10U + sectionIndex * 31U);
                    const uint8_t hue = static_cast<uint8_t>(135U + wave8(static_cast<uint8_t>(phase / 2U)) / 3U);
                    setSection(section, i, decorativeHsv(LedCategory::Idle, hue, 220,
                                                         static_cast<uint8_t>(24U + wave8(phase) / 3U)));
                }
            }
            break;
    }
}

void LedService::renderPrint(uint8_t animation, const LedAnimationContext& context) {
    const uint32_t now = context.nowMs;
    const uint8_t progress = min<uint8_t>(context.progress, 100U);
    const RgbwColor rawFilament = fromRgb(context.filamentRgb);
    const uint8_t rawMaximum = max(rawFilament.r, max(rawFilament.g, rawFilament.b));
    const RgbwColor filament = rawMaximum < 12U
        ? decorativeHsv(LedCategory::Print, static_cast<uint8_t>(now / 20U), 255U, 230U)
        : rawFilament;
    const uint16_t lit = static_cast<uint16_t>((progress * hw::CenterCount + 99U) / 100U);
    uint8_t filamentHue = 18U;
    const bool filamentHasHue = rgbHue(filament, filamentHue);
    RgbwColor filamentPalette[4];
    for (uint8_t slot = 0; slot < 4U; ++slot) {
        if (context.filamentColorMask & (1U << slot)) {
            filamentPalette[slot] = fromRgb(context.filamentColorsRgb[slot]);
            const uint8_t maximum = max(filamentPalette[slot].r,
                                        max(filamentPalette[slot].g, filamentPalette[slot].b));
            if (maximum >= 12U) continue;
        }
        const uint8_t fallbackHue = filamentHasHue
            ? static_cast<uint8_t>(filamentHue + 53U + slot * 59U)
            : static_cast<uint8_t>(20U + slot * 61U);
        filamentPalette[slot] = decorativeHsv(LedCategory::Print, fallbackHue, 245U, 255U);
    }
    auto fillFilamentSides = [&](uint8_t value = 255U) {
        fillSection(LedSection::Left, scaled(filament, value));
        fillSection(LedSection::Right, scaled(filament, value));
    };

    const bool animationChanged = lastPrintAnimation_ != animation;
    if (animationChanged) lastPrintAnimation_ = animation;

    switch (static_cast<PrintAnimation>(animation)) {
        case PrintAnimation::FinishPressure: {
            const uint8_t pressure = progress <= 80U ? 0U
                : static_cast<uint8_t>((progress - 80U) * 255U / 20U);
            const uint8_t sidePulse = static_cast<uint8_t>(64U +
                static_cast<uint16_t>(wave8(now / max<uint16_t>(8U, 34U - pressure / 12U))) *
                    (45U + pressure / 2U) / 255U);
            fillSection(LedSection::Left, scaled(filament, sidePulse));
            fillSection(LedSection::Right, scaled(filament, sidePulse));
            RgbwColor pressureColor = complementary(filament);
            if (pressureColor.r == 0U && pressureColor.g == 0U && pressureColor.b == 0U) {
                pressureColor = decorativeHsv(LedCategory::Print, 28U, 245U, 255U);
            }
            const uint16_t half = hw::CenterCount / 2U;
            const uint32_t reach = static_cast<uint32_t>(pressure) * half * 255U / 255U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t progressBase = progressCoverage(progress, hw::CenterCount, i);
                RgbwColor color = scaled(filament,
                    static_cast<uint8_t>(14U + progressBase * 48U / 255U));
                const uint16_t distanceFromEdge = min<uint16_t>(i, hw::CenterCount - 1U - i);
                const uint32_t pixelStart = static_cast<uint32_t>(distanceFromEdge) * 255U;
                const uint8_t compression = reach <= pixelStart ? 0U
                    : reach >= pixelStart + 255U ? 255U
                    : static_cast<uint8_t>(reach - pixelStart);
                if (compression) color = blend(color, pressureColor, compression);
                setSection(LedSection::Center, i, color);
            }
            break;
        }

        case PrintAnimation::DualTempMeter: {
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor chamber = temperatureColor(chamberPercent);
            const RgbwColor tool = temperatureColor(toolPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t leftCoverage = progressCoverage(chamberPercent, hw::LeftCount, i);
                const uint8_t rightCoverage = progressCoverage(toolPercent, hw::RightCount, i);
                setSection(LedSection::Left, i, scaled(chamber,
                    leftCoverage ? static_cast<uint8_t>(55U + leftCoverage * 175U / 255U) : 8U));
                setSection(LedSection::Right, i, scaled(tool,
                    rightCoverage ? static_cast<uint8_t>(55U + rightCoverage * 175U / 255U) : 8U));
            }
            const uint16_t half = hw::CenterCount / 2U;
            for (uint16_t i = 0; i < half; ++i) {
                const uint8_t chamberCoverage = progressCoverage(chamberPercent, half, i);
                const uint8_t toolCoverage = progressCoverage(toolPercent, half, i);
                const uint8_t shimmerA = static_cast<uint8_t>(80U + wave8(
                    static_cast<uint8_t>(now / 29U + i * 18U)) / 3U);
                const uint8_t shimmerB = static_cast<uint8_t>(80U + wave8(
                    static_cast<uint8_t>(now / 23U + i * 18U + 96U)) / 3U);
                setSection(LedSection::Center, half - 1U - i,
                           scaled(chamber, chamberCoverage ?
                               static_cast<uint8_t>(chamberCoverage * shimmerA / 255U) : 7U));
                setSection(LedSection::Center, half + i,
                           scaled(tool, toolCoverage ?
                               static_cast<uint8_t>(toolCoverage * shimmerB / 255U) : 7U));
            }
            break;
        }

        case PrintAnimation::LayerPulse: {
            fillSection(LedSection::Left, scaled(filament, 30U));
            fillSection(LedSection::Right, scaled(filament, 30U));
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U));
            const uint16_t pulseRadius = lit
                ? static_cast<uint16_t>((now / 72U) % (lit + 5U)) : 0U;
            RgbwColor pulseColor = complementary(filament);
            if (pulseColor.r == 0U && pulseColor.g == 0U && pulseColor.b == 0U) {
                pulseColor = decorativeHsv(LedCategory::Print, 145U, 235U, 255U);
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                RgbwColor color = scaled(filament,
                    static_cast<uint8_t>(35U + coverage * 70U / 255U));
                const uint16_t distance = marker > i ? marker - i : i - marker;
                const uint16_t ringDistance = distance > pulseRadius
                    ? distance - pulseRadius : pulseRadius - distance;
                if (i <= marker && ringDistance <= 1U) {
                    color = scaled(pulseColor, ringDistance ? 120U : 245U);
                }
                setSection(LedSection::Center, i, color);
            }
            if ((now / 72U) % max<uint16_t>(1U, lit + 5U) < 2U) {
                fillSection(LedSection::Left, scaled(filament, 105U));
                fillSection(LedSection::Right, scaled(filament, 105U));
            }
            break;
        }

        case PrintAnimation::ToolpathEcho: {
            fillSection(LedSection::Left, scaled(filament, 12U));
            fillSection(LedSection::Right, scaled(filament, 12U));
            const uint16_t workingSpan = max<uint16_t>(2U, lit);
            const uint16_t travelSpan = workingSpan - 1U;
            const uint16_t cycle = max<uint16_t>(1U, travelSpan * 2U);
            const uint16_t phase = static_cast<uint16_t>((now / 54U) % cycle);
            const uint16_t head = phase <= travelSpan ? phase : cycle - phase;
            const bool forward = phase <= travelSpan;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filament, static_cast<uint8_t>(16U + coverage * 42U / 255U)));
                const uint16_t distance = i > head ? i - head : head - i;
                if (distance <= 4U && ((forward && i <= head) || (!forward && i >= head))) {
                    setSection(LedSection::Center, i,
                               scaled(filament, static_cast<uint8_t>(255U - distance * 46U)));
                }
            }
            constexpr uint8_t EchoDelays[4] = {3U, 7U, 12U, 18U};
            for (uint8_t echo = 0; echo < 4U; ++echo) {
                const uint16_t delayedPhase = static_cast<uint16_t>(
                    (phase + cycle - EchoDelays[echo] % cycle) % cycle);
                const uint16_t delayedHead = delayedPhase <= travelSpan
                    ? delayedPhase : cycle - delayedPhase;
                const uint16_t sidePosition = static_cast<uint16_t>(
                    static_cast<uint32_t>(delayedHead) * (hw::LeftCount - 1U) /
                    max<uint16_t>(1U, travelSpan));
                const uint8_t value = static_cast<uint8_t>(170U - echo * 32U);
                setSection(LedSection::Left, sidePosition, scaled(filament, value));
                setSection(LedSection::Right, hw::RightCount - 1U - sidePosition,
                           scaled(filament, value));
            }
            break;
        }

        case PrintAnimation::ThermalRibbon: {
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor bed = temperatureColor(bedPercent);
            const RgbwColor tool = temperatureColor(toolPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t foldA = wave8(static_cast<uint8_t>(now / 25U + i * 28U));
                const uint8_t foldB = wave8(static_cast<uint8_t>(now / 19U - i * 28U));
                setSection(LedSection::Left, i,
                           scaled(bed, static_cast<uint8_t>(42U + foldA * 145U / 255U)));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(tool, static_cast<uint8_t>(42U + foldB * 145U / 255U)));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (hw::CenterCount - 1U));
                const uint8_t fold = wave8(static_cast<uint8_t>(now / 21U + i * 24U));
                const uint8_t value = coverage
                    ? static_cast<uint8_t>(55U + fold * coverage * 155UL / 65025UL) : 8U;
                setSection(LedSection::Center, i, scaled(blend(bed, tool, amount), value));
            }
            break;
        }

        case PrintAnimation::InfillGrid: {
            const uint8_t phaseA = static_cast<uint8_t>(now / 94U);
            const uint8_t phaseB = static_cast<uint8_t>(now / 121U);
            RgbwColor crossColor = complementary(filament);
            if (crossColor.r == 0U && crossColor.g == 0U && crossColor.b == 0U) {
                crossColor = decorativeHsv(LedCategory::Print, 145U, 245U, 255U);
            }
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const bool diagonalA = ((path + phaseA) % 8U) == 0U;
                const bool diagonalB = ((path * 3U + phaseB) % 11U) == 0U;
                uint8_t value = 14U;
                RgbwColor color = filament;
                if (diagonalA || diagonalB) value = 150U;
                if (diagonalA && diagonalB) {
                    value = 255U;
                    color = crossColor;
                }
                if (path >= hw::LeftCount && path < hw::LeftCount + hw::CenterCount) {
                    const uint16_t centerIndex = path - hw::LeftCount;
                    const uint8_t coverage = progressCoverage(progress, hw::CenterCount, centerIndex);
                    value = static_cast<uint8_t>(static_cast<uint16_t>(value) * coverage / 255U);
                }
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }

        case PrintAnimation::FilamentBeads: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                setOuterVisualPathPixel(path, scaled(filament, 8U));
            }
            const uint16_t step = static_cast<uint16_t>(now / 82U);
            for (uint8_t bead = 0; bead < 8U; ++bead) {
                const uint16_t path = static_cast<uint16_t>((step + bead * 6U) % hw::OuterCount);
                const uint8_t paletteIndex = bead & 3U;
                setOuterVisualPathPixel(path, scaled(filamentPalette[paletteIndex], 235U));
                const uint16_t neighbor = static_cast<uint16_t>((path + 1U) % hw::OuterCount);
                setOuterVisualPathPixel(neighbor, scaled(filamentPalette[paletteIndex], 52U));
            }
            const uint16_t fixedBeads = static_cast<uint16_t>(progress * 5U / 100U);
            for (uint16_t bead = 0; bead < fixedBeads; ++bead) {
                const uint16_t position = min<uint16_t>(hw::CenterCount - 1U,
                    static_cast<uint16_t>(bead * 4U + 1U));
                setSection(LedSection::Center, position,
                           scaled(filamentPalette[bead & 3U], 255U));
            }
            break;
        }

        case PrintAnimation::TimeFlow: {
            const uint64_t totalSeconds = static_cast<uint64_t>(context.printDurationSec) +
                                          context.printEtaSec;
            const uint8_t elapsedPercent = totalSeconds
                ? static_cast<uint8_t>(min<uint64_t>(100U,
                    static_cast<uint64_t>(context.printDurationSec) * 100U / totalSeconds))
                : progress;
            const uint8_t remainingPercent = static_cast<uint8_t>(100U - elapsedPercent);
            RgbwColor remainingColor = complementary(filament);
            if (remainingColor.r == 0U && remainingColor.g == 0U && remainingColor.b == 0U) {
                remainingColor = decorativeHsv(LedCategory::Print, 145U, 240U, 255U);
            }
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t elapsedCoverage = progressCoverage(elapsedPercent, hw::LeftCount, i);
                const uint8_t remainingCoverage = progressCoverage(remainingPercent, hw::RightCount, i);
                setSection(LedSection::Left, i,
                           scaled(filament, elapsedCoverage ? elapsedCoverage : 8U));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(remainingColor, remainingCoverage ? remainingCoverage : 8U));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                setSection(LedSection::Center, i,
                           coverage ? scaled(filament, static_cast<uint8_t>(45U + coverage * 110U / 255U))
                                    : scaled(remainingColor, 12U));
            }
            const uint16_t period = static_cast<uint16_t>(45U + remainingPercent);
            const uint16_t droplet = static_cast<uint16_t>((now / period) % hw::OuterCount);
            setOuterVisualPathPixel(hw::OuterCount - 1U - droplet,
                                   blend(filament, remainingColor, 128U));
            break;
        }

        case PrintAnimation::StepperTicks: {
            const uint16_t tick = static_cast<uint16_t>((now / 74U) % hw::CenterCount);
            const uint8_t phase = static_cast<uint8_t>((now / 74U) & 3U);
            RgbwColor tickColor = complementary(filament);
            if (tickColor.r == 0U && tickColor.g == 0U && tickColor.b == 0U) {
                tickColor = decorativeHsv(LedCategory::Print, 145U, 245U, 255U);
            }
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const bool phaseLeft = (i & 3U) == phase;
                const bool phaseRight = (i & 3U) == ((phase + 2U) & 3U);
                setSection(LedSection::Left, i,
                           scaled(phaseLeft ? tickColor : filament, phaseLeft ? 230U : 14U));
                setSection(LedSection::Right, i,
                           scaled(phaseRight ? tickColor : filament, phaseRight ? 230U : 14U));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const bool ruler = (i % 5U) == 0U;
                uint8_t value = coverage
                    ? static_cast<uint8_t>((ruler ? 92U : 35U) + coverage / 5U) : 6U;
                RgbwColor color = filament;
                if (i == tick) {
                    value = 255U;
                    color = tickColor;
                }
                setSection(LedSection::Center, i, scaled(color, value));
            }
            break;
        }

        case PrintAnimation::CalmBuild: {
            const uint8_t breath = static_cast<uint8_t>(65U + wave8(now / 86U) / 8U);
            fillSection(LedSection::Left, scaled(filament, breath));
            fillSection(LedSection::Right, scaled(filament, breath));
            const uint8_t terraces = static_cast<uint8_t>(3U + progress / 17U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const uint8_t terrace = static_cast<uint8_t>(i * terraces / hw::CenterCount);
                const uint8_t drift = wave8(static_cast<uint8_t>(now / 83U + terrace * 31U));
                const uint8_t value = static_cast<uint8_t>(50U + terrace * 10U + drift / 12U);
                setSection(LedSection::Center, i,
                           scaled(filament, static_cast<uint8_t>(value * coverage / 255U)));
            }
            break;
        }

        case PrintAnimation::QualityGuard: {
            const bool stale = context.printerTelemetryAgeMs > 20000U;
            const bool invalidTool = isnan(context.activeToolTempC) ||
                context.activeToolTempC < 100.0f || context.activeToolTempC > 320.0f;
            const bool invalidChamber = !isnan(context.chamberTempC) &&
                (context.chamberTempC < 0.0f || context.chamberTempC > 75.0f);
            const bool warning = !context.printerOnline || context.ventFailsafe || stale ||
                                 invalidTool || invalidChamber;
            const RgbwColor guard = warning ? RgbwColor(255U, 55U, 0U)
                                            : RgbwColor(0U, 215U, 150U);
            const uint8_t railValue = warning
                ? static_cast<uint8_t>(55U + wave8(now / 13U) * 180U / 255U) : 72U;
            fillSection(LedSection::Left, scaled(guard, railValue));
            fillSection(LedSection::Right, scaled(guard, railValue));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                setSection(LedSection::Center, i, coverage
                    ? blend(scaled(filament, 76U), scaled(guard, 160U), 92U)
                    : scaled(guard, 8U));
            }
            const uint16_t scan = static_cast<uint16_t>((now / (warning ? 48U : 145U)) % hw::CenterCount);
            setSection(LedSection::Center, scan,
                       warning ? RgbwColor(255U, 155U, 0U) : RgbwColor(120U, 255U, 215U));
            break;
        }

        case PrintAnimation::NozzleHeat: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor heat = temperatureColor(toolPercent);
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U));
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t coverage = progressCoverage(toolPercent, hw::LeftCount, i);
                setSection(LedSection::Left, i,
                           scaled(heat, coverage ? static_cast<uint8_t>(55U + coverage * 175U / 255U) : 8U));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(filament, static_cast<uint8_t>(30U + coverage * 65U / 255U)));
            }
            const uint16_t radius = static_cast<uint16_t>(1U + toolPercent / 24U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                RgbwColor color = coverage ? scaled(filament,
                    static_cast<uint8_t>(38U + coverage * 72U / 255U)) : RgbwColor();
                const uint16_t distance = i > marker ? i - marker : marker - i;
                if (distance <= radius) {
                    const uint8_t intensity = static_cast<uint8_t>(
                        255U - static_cast<uint32_t>(distance) * 205U / max<uint16_t>(1U, radius));
                    color = blend(color, heat, intensity);
                }
                setSection(LedSection::Center, i, color);
            }
            break;
        }

        case PrintAnimation::LayerFill: {
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t leftWave = wave8(static_cast<uint8_t>(now / 38U + i * 17U));
                const uint8_t rightWave = wave8(static_cast<uint8_t>(now / 43U - i * 17U + 90U));
                setSection(LedSection::Left, i,
                           scaled(filament, static_cast<uint8_t>(68U + leftWave / 5U)));
                setSection(LedSection::Right, i,
                           scaled(filament, static_cast<uint8_t>(68U + rightWave / 5U)));
            }
            const uint32_t exactFill = static_cast<uint32_t>(progress) * hw::CenterCount * 255U / 100U;
            const uint16_t activeLayer = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(exactFill / 255U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t layerTexture = (i & 1U) ? 96U : 122U;
                uint8_t value = coverage
                    ? static_cast<uint8_t>(static_cast<uint16_t>(layerTexture) * coverage / 255U) : 10U;
                RgbwColor color = filament;
                const uint16_t distance = i > activeLayer ? i - activeLayer : activeLayer - i;
                if (distance <= 2U && progress < 100U) {
                    const uint8_t fresh = static_cast<uint8_t>(230U - distance * 58U);
                    value = max<uint8_t>(value, fresh);
                    color = blend(filament, RgbwColor(255U, 78U, 0U), 72U);
                }
                setSection(LedSection::Center, i, scaled(color, value));
            }
            break;
        }

        case PrintAnimation::ThermalBalance: {
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const RgbwColor bed = temperatureColor(bedPercent);
            const RgbwColor chamber = temperatureColor(chamberPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t bedCoverage = progressCoverage(bedPercent, hw::LeftCount, i);
                const uint8_t chamberCoverage = progressCoverage(chamberPercent, hw::RightCount, i);
                const uint8_t leftValue = bedCoverage
                    ? static_cast<uint8_t>(58U + static_cast<uint16_t>(bedCoverage) * 145U / 255U) : 12U;
                const uint8_t rightValue = chamberCoverage
                    ? static_cast<uint8_t>(58U + static_cast<uint16_t>(chamberCoverage) * 145U / 255U) : 12U;
                setSection(LedSection::Left, i, scaled(bed, leftValue));
                setSection(LedSection::Right, i, scaled(chamber, rightValue));
            }
            const uint16_t thermalTotal = static_cast<uint16_t>(bedPercent) + chamberPercent;
            const uint16_t balance = thermalTotal
                ? static_cast<uint16_t>(static_cast<uint32_t>(bedPercent) *
                    (hw::CenterCount - 1U) / thermalTotal)
                : hw::CenterCount / 2U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (hw::CenterCount - 1U));
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                uint8_t value = static_cast<uint8_t>(38U + static_cast<uint16_t>(coverage) * 82U / 255U);
                const uint16_t distance = i > balance ? i - balance : balance - i;
                if (distance <= 1U) {
                    const uint8_t pulse = static_cast<uint8_t>(165U + wave8(now / 31U) * 90U / 255U);
                    value = max<uint8_t>(value, distance ? pulse / 2U : pulse);
                }
                setSection(LedSection::Center, i, scaled(blend(bed, chamber, amount), value));
            }
            break;
        }

        case PrintAnimation::MaterialCore: {
            const uint8_t breath = static_cast<uint8_t>(36U + wave8(now / 42U) / 5U);
            fillSection(LedSection::Left, scaled(filament, breath));
            fillSection(LedSection::Right, scaled(filament, breath));
            RgbwColor coreColor = complementary(filament);
            if (coreColor.r == 0U && coreColor.g == 0U && coreColor.b == 0U) {
                coreColor = decorativeHsv(LedCategory::Print, 138U, 245U, 255U);
            }
            const uint16_t half = hw::CenterCount / 2U;
            const uint16_t travelSpan = max<uint16_t>(1U, half - 1U);
            const uint16_t cycle = travelSpan * 2U;
            const uint16_t phase = static_cast<uint16_t>((now / 78U) % cycle);
            const uint16_t travel = phase <= travelSpan ? phase : cycle - phase;
            const uint16_t leftCore = travel;
            const uint16_t rightCore = hw::CenterCount - 1U - travel;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                setSection(LedSection::Center, i,
                           scaled(filament, static_cast<uint8_t>(16U + coverage * 55U / 255U)));
                const uint16_t leftDistance = i > leftCore ? i - leftCore : leftCore - i;
                const uint16_t rightDistance = i > rightCore ? i - rightCore : rightCore - i;
                const uint16_t distance = min<uint16_t>(leftDistance, rightDistance);
                if (distance <= 3U) {
                    const uint8_t value = static_cast<uint8_t>(255U - distance * 64U);
                    setSection(LedSection::Center, i,
                               blend(scaled(filament, value), scaled(coreColor, value), 120U));
                }
            }
            break;
        }

        case PrintAnimation::HeatSoak: {
            float chamberC = isnan(context.chamberTempC) ? 30.0f : context.chamberTempC;
            chamberC = max<float>(0.0f, min<float>(80.0f, chamberC));
            const uint8_t heatPercent = temperaturePercent(chamberC, 20.0f, 60.0f, 25U);
            const RgbwColor heat = temperatureColor(heatPercent);
            const uint16_t halfPath = (hw::OuterCount + 1U) / 2U;
            const uint32_t front = static_cast<uint32_t>(heatPercent) * halfPath * 255U / 100U;
            const uint8_t breathing = static_cast<uint8_t>(165U + wave8(now / 48U) * 70U / 255U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distanceFromEnd = min<uint16_t>(path, hw::OuterCount - 1U - path);
                const uint32_t pixelStart = static_cast<uint32_t>(distanceFromEnd) * 255U;
                const uint8_t coverage = front <= pixelStart ? 0U
                    : front >= pixelStart + 255U ? 255U
                    : static_cast<uint8_t>(front - pixelStart);
                const uint8_t value = coverage
                    ? static_cast<uint8_t>(25U + static_cast<uint16_t>(coverage) * breathing / 255U)
                    : 8U;
                setOuterVisualPathPixel(path, scaled(heat, value));
            }
            break;
        }

        case PrintAnimation::StabilityMonitor: {
            if (animationChanged || stabilityWasPreview_ != context.preview) {
                stabilityWasPreview_ = context.preview;
                stabilitySampleMs_ = now;
                stabilityLastToolC_ = context.activeToolTempC;
                stabilityLastChamberC_ = context.chamberTempC;
                stabilityJitter_ = 0U;
            }
            if (context.preview) {
                stabilityJitter_ = static_cast<uint8_t>(12U + wave8(now / 19U) * 54U / 255U);
            } else if (now - stabilitySampleMs_ >= 1000U) {
                float jitter = 0.0f;
                if (!isnan(context.activeToolTempC) && !isnan(stabilityLastToolC_)) {
                    jitter += fabsf(context.activeToolTempC - stabilityLastToolC_) * 18.0f;
                }
                if (!isnan(context.chamberTempC) && !isnan(stabilityLastChamberC_)) {
                    jitter += fabsf(context.chamberTempC - stabilityLastChamberC_) * 35.0f;
                }
                stabilityJitter_ = static_cast<uint8_t>(min<float>(100.0f, jitter));
                stabilityLastToolC_ = context.activeToolTempC;
                stabilityLastChamberC_ = context.chamberTempC;
                stabilitySampleMs_ = now;
            }
            const RgbwColor stable(0U, 190U, 150U);
            const RgbwColor warning(255U, 92U, 0U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t baseline = static_cast<uint8_t>(26U + coverage * 48U / 255U);
                setSection(LedSection::Center, i, scaled(stable, baseline));
                const uint8_t sample = hash8(i * 79U + now / 125U * 37U);
                if (sample > static_cast<uint8_t>(250U - stabilityJitter_ / 2U)) {
                    setSection(LedSection::Center, i, scaled(warning,
                        static_cast<uint8_t>(130U + sample / 2U)));
                }
            }
            const uint8_t calm = static_cast<uint8_t>(48U + wave8(now / 46U) / 5U);
            const uint8_t alert = static_cast<uint8_t>(80U + stabilityJitter_ * 7U / 4U);
            fillSection(LedSection::Left,
                        scaled(stabilityJitter_ > 35U ? warning : stable,
                               stabilityJitter_ > 35U ? alert : calm));
            fillSection(LedSection::Right,
                        scaled(stabilityJitter_ > 35U ? warning : stable,
                               stabilityJitter_ > 35U ? alert : calm));
            break;
        }

        case PrintAnimation::LayerEngine: {
            RgbwColor phaseColor = complementary(filament);
            if (phaseColor.r == 0U && phaseColor.g == 0U && phaseColor.b == 0U) {
                phaseColor = decorativeHsv(LedCategory::Print, 145U, 235U, 255U);
            }
            const uint8_t phase = static_cast<uint8_t>(now / 68U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t leftCoil = static_cast<uint8_t>((i + phase) % 6U);
                const uint8_t rightCoil = static_cast<uint8_t>((hw::RightCount - 1U - i + phase) % 6U);
                setSection(LedSection::Left, i,
                           scaled(leftCoil < 2U ? phaseColor : filament, leftCoil < 2U ? 220U : 24U));
                setSection(LedSection::Right, i,
                           scaled(rightCoil < 2U ? phaseColor : filament, rightCoil < 2U ? 220U : 24U));
            }
            const uint16_t activeLayer = lit
                ? static_cast<uint16_t>((now / 92U) % lit) : 0U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const bool seam = ((i + phase / 3U) % 4U) == 0U;
                uint8_t value = static_cast<uint8_t>(32U + coverage * (seam ? 88U : 48U) / 255U);
                const uint16_t distance = i > activeLayer ? i - activeLayer : activeLayer - i;
                if (distance <= 2U) value = max<uint8_t>(value,
                    static_cast<uint8_t>(238U - distance * 66U));
                setSection(LedSection::Center, i,
                           scaled(distance == 0U ? phaseColor : filament, value));
            }
            break;
        }

        case PrintAnimation::TimeTunnel: {
            const uint32_t totalSeconds = context.printDurationSec + context.printEtaSec;
            const uint8_t remainingPercent = totalSeconds
                ? static_cast<uint8_t>(min<uint32_t>(100U,
                    context.printEtaSec * 100U / totalSeconds))
                : static_cast<uint8_t>(100U - progress);
            const uint16_t stepMs = static_cast<uint16_t>(38U + remainingPercent);
            const uint8_t phase = static_cast<uint8_t>(now / stepMs);
            RgbwColor horizon = complementary(filament);
            if (horizon.r == 0U && horizon.g == 0U && horizon.b == 0U) {
                horizon = decorativeHsv(LedCategory::Print, 24U, 240U, 255U);
            }
            const uint16_t center = (hw::OuterCount - 1U) / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t band = static_cast<uint8_t>((distance * 3U + phase) % 12U);
                const uint8_t depth = static_cast<uint8_t>(255U -
                    min<uint16_t>(210U, distance * 210U / max<uint16_t>(1U, center)));
                const uint8_t value = band < 3U
                    ? static_cast<uint8_t>(70U + depth * (3U - band) / 3U)
                    : static_cast<uint8_t>(10U + depth / 10U);
                setOuterVisualPathPixel(path, scaled(band == 0U ? horizon : filament, value));
            }
            break;
        }

        case PrintAnimation::ChamberAura: {
            float chamberC = isnan(context.chamberTempC) ? 35.0f : context.chamberTempC;
            chamberC = max<float>(0.0f, min<float>(80.0f, chamberC));
            const uint8_t chamberPercent = temperaturePercent(chamberC, 20.0f, 60.0f, 35U);
            const RgbwColor aura = temperatureColor(chamberPercent);
            uint16_t divisor = 44U;
            if (chamberC < 20.0f) divisor = max<uint16_t>(9U,
                static_cast<uint16_t>(44.0f - (20.0f - chamberC) * 1.5f));
            else if (chamberC > 60.0f) divisor = max<uint16_t>(9U,
                static_cast<uint16_t>(44.0f - (chamberC - 60.0f) * 1.5f));
            const uint8_t breathe = static_cast<uint8_t>(52U + wave8(now / divisor) * 128U / 255U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t halo = static_cast<uint8_t>(breathe +
                    wave8(static_cast<uint8_t>(now / 31U + i * 13U)) / 5U);
                setSection(LedSection::Left, i, scaled(aura, halo));
                setSection(LedSection::Right, hw::RightCount - 1U - i, scaled(aura, halo));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const RgbwColor silhouette = blend(aura, filament, 155U);
                setSection(LedSection::Center, i,
                           scaled(silhouette, static_cast<uint8_t>(18U + coverage * 112U / 255U)));
            }
            break;
        }

        case PrintAnimation::FilamentFlow: {
            constexpr uint16_t Sources[4] = {1U, 9U, 32U, 40U};
            const uint8_t active = context.activeTool & 3U;
            const uint16_t destination = static_cast<uint16_t>(hw::LeftCount +
                min<uint16_t>(hw::CenterCount - 1U,
                    static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U)));
            for (uint8_t tool = 0; tool < 4U; ++tool) {
                setOuterVisualPathPixel(Sources[tool], scaled(filamentPalette[tool],
                    tool == active ? 230U : 58U));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filamentPalette[active], static_cast<uint8_t>(20U + coverage * 62U / 255U)));
            }
            const uint16_t source = Sources[active];
            const uint8_t travel = static_cast<uint8_t>((now / 18U) & 0xFFU);
            const int32_t span = static_cast<int32_t>(destination) - source;
            const int32_t headRaw = static_cast<int32_t>(source) + span * travel / 255;
            const uint16_t head = static_cast<uint16_t>(max<int32_t>(0,
                min<int32_t>(hw::OuterCount - 1U, headRaw)));
            const int8_t direction = span >= 0 ? 1 : -1;
            for (uint8_t tail = 0; tail < 5U; ++tail) {
                const int32_t position = static_cast<int32_t>(head) - direction * tail;
                if (position < 0 || position >= hw::OuterCount) continue;
                setOuterVisualPathPixel(static_cast<uint16_t>(position),
                    scaled(filamentPalette[active], static_cast<uint8_t>(255U - tail * 45U)));
            }
            break;
        }

        case PrintAnimation::ProcessStack: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const RgbwColor toolHeat = temperatureColor(toolPercent);
            const RgbwColor chamberHeat = temperatureColor(chamberPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t toolCoverage = progressCoverage(toolPercent, hw::LeftCount, i);
                const uint8_t chamberCoverage = progressCoverage(chamberPercent, hw::RightCount, i);
                setSection(LedSection::Left, i, toolCoverage
                    ? blend(scaled(filament, 105U), scaled(toolHeat, 220U), toolCoverage)
                    : scaled(filament, 12U));
                setSection(LedSection::Right, i, scaled(chamberHeat,
                    chamberCoverage ? static_cast<uint8_t>(55U + chamberCoverage * 165U / 255U) : 12U));
            }
            RgbwColor progressColor = complementary(filament);
            if (progressColor.r == 0U && progressColor.g == 0U && progressColor.b == 0U) {
                progressColor = decorativeHsv(LedCategory::Print, 132U, 245U, 255U);
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                setSection(LedSection::Center, i,
                           coverage ? scaled(progressColor, coverage) : RgbwColor(2U, 2U, 2U));
            }
            const uint16_t handoff = static_cast<uint16_t>((now / 105U) % hw::OuterCount);
            setOuterVisualPathPixel(handoff, RgbwColor(230U, 230U, 230U));
            break;
        }

        case PrintAnimation::HealthBeacon: {
            const bool stale = context.printerTelemetryAgeMs > 20000U;
            const bool healthy = context.printerOnline && !context.ventFailsafe && !stale;
            const RgbwColor status = healthy ? RgbwColor(0U, 220U, 135U)
                                            : RgbwColor(255U, 45U, 0U);
            const uint32_t cycle = now % (healthy ? 2400U : 760U);
            const bool firstBeat = cycle < (healthy ? 150U : 110U);
            const bool secondBeat = cycle >= (healthy ? 270U : 200U) &&
                                    cycle < (healthy ? 410U : 310U);
            const uint8_t value = firstBeat || secondBeat ? 255U : healthy ? 28U : 16U;
            fillSection(LedSection::Left, scaled(status, value));
            fillSection(LedSection::Right, scaled(status, value));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t baseline = static_cast<uint8_t>((healthy ? 24U : 10U) + coverage / 4U);
                setSection(LedSection::Center, i, scaled(status, baseline));
            }
            const uint16_t sweep = static_cast<uint16_t>((now / (healthy ? 135U : 62U)) % hw::CenterCount);
            setSection(LedSection::Center, sweep,
                       healthy ? RgbwColor(120U, 255U, 205U) : RgbwColor(255U, 120U, 0U));
            break;
        }

        case PrintAnimation::ExtruderSpark: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor heat = temperatureColor(toolPercent);
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U));
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t packet = static_cast<uint8_t>((i * 3U + now / 42U) % 9U);
                const uint8_t value = packet == 0U ? 235U : packet == 1U ? 105U : 24U;
                setSection(LedSection::Left, i, scaled(filament, value));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(filament, value));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filament, static_cast<uint8_t>(35U + coverage * 105U / 255U)));
                const uint16_t distance = i > marker ? i - marker : marker - i;
                if (distance <= 1U) {
                    setSection(LedSection::Center, i,
                               scaled(heat, distance == 0U ? 255U : 130U));
                } else {
                    const uint8_t spark = hash8(i * 83U + now / 48U * 37U);
                    const uint8_t threshold = static_cast<uint8_t>(250U - toolPercent / 10U);
                    if (spark > threshold && distance <= 5U) {
                        setSection(LedSection::Center, i,
                                   scaled(heat, static_cast<uint8_t>(120U + spark / 2U)));
                    }
                }
            }
            break;
        }

        case PrintAnimation::LayerScan: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                setOuterVisualPathPixel(path, scaled(filament, 20U));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filament, static_cast<uint8_t>(32U + coverage * 80U / 255U)));
            }
            const uint16_t span = hw::OuterCount - 1U;
            const uint16_t cycle = span * 2U;
            const uint16_t phase = static_cast<uint16_t>((now / 46U) % cycle);
            const uint16_t scan = phase <= span ? phase : cycle - phase;
            RgbwColor scanColor = complementary(filament);
            if (scanColor.r == 0U && scanColor.g == 0U && scanColor.b == 0U) {
                scanColor = decorativeHsv(LedCategory::Print, 132U, 220U, 255U);
            }
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > scan ? path - scan : scan - path;
                if (distance <= 2U) {
                    setOuterVisualPathPixel(path, scaled(scanColor,
                        static_cast<uint8_t>(255U - distance * 82U)));
                }
            }
            break;
        }

        case PrintAnimation::HeatRipple: {
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor bed = temperatureColor(bedPercent);
            const RgbwColor chamber = temperatureColor(chamberPercent);
            const RgbwColor tool = temperatureColor(toolPercent);
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U));
            const uint16_t origin = hw::LeftCount + marker;
            const uint8_t ripplePhase = static_cast<uint8_t>((now / 58U) % 11U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                RgbwColor base;
                if (path < hw::LeftCount) base = scaled(bed, 28U);
                else if (path < hw::LeftCount + hw::CenterCount) {
                    const uint16_t centerIndex = path - hw::LeftCount;
                    base = scaled(filament, progressCoverage(progress, hw::CenterCount, centerIndex) / 4U);
                } else base = scaled(chamber, 28U);
                const uint16_t distance = path > origin ? path - origin : origin - path;
                const uint8_t band = static_cast<uint8_t>((distance + 11U - ripplePhase) % 11U);
                const uint8_t ripple = band <= 2U
                    ? static_cast<uint8_t>(210U - band * 72U) : 0U;
                setOuterVisualPathPixel(path, ripple ? blend(base, tool, ripple) : base);
            }
            break;
        }

        case PrintAnimation::FilamentComets: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                setOuterVisualPathPixel(path, scaled(filament, 10U));
            }
            for (uint8_t comet = 0; comet < 4U; ++comet) {
                const uint16_t head = static_cast<uint16_t>((now / (42U + comet * 7U) +
                    comet * (hw::OuterCount / 4U)) % hw::OuterCount);
                for (uint8_t tail = 0; tail < 5U; ++tail) {
                    const uint16_t path = static_cast<uint16_t>(
                        (head + hw::OuterCount - tail) % hw::OuterCount);
                    const uint8_t value = static_cast<uint8_t>(255U - tail * 48U);
                    setOuterVisualPathPixel(path, scaled(filamentPalette[comet], value));
                }
            }
            break;
        }

        case PrintAnimation::ProgressTheater: {
            RgbwColor marquee = complementary(filament);
            if (marquee.r == 0U && marquee.g == 0U && marquee.b == 0U) {
                marquee = decorativeHsv(LedCategory::Print, 26U, 235U, 255U);
            }
            const uint16_t curtainOpen = static_cast<uint16_t>(
                static_cast<uint32_t>(progress) * hw::LeftCount / 100U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const bool open = i < curtainOpen;
                const uint8_t fold = wave8(static_cast<uint8_t>(now / 38U + i * 28U));
                const uint8_t value = open ? 20U
                    : static_cast<uint8_t>(95U + fold * 125U / 255U);
                const RgbwColor curtain = decorativeHsv(LedCategory::Print, 248U, 245U, value);
                setSection(LedSection::Left, i, curtain);
                setSection(LedSection::Right, hw::RightCount - 1U - i, curtain);
            }
            const uint16_t chase = static_cast<uint16_t>(now / 105U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const bool bulb = ((i + chase) % 4U) == 0U;
                const uint8_t value = bulb ? 255U : 92U;
                setSection(LedSection::Center, i, scaled(bulb ? marquee : filament,
                    static_cast<uint8_t>(static_cast<uint16_t>(coverage) * value / 255U)));
            }
            break;
        }

        case PrintAnimation::NozzleTrace: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const RgbwColor heat = temperatureColor(toolPercent);
            fillSection(LedSection::Left, scaled(heat, 34U));
            fillSection(LedSection::Right, scaled(heat, 34U));
            const uint16_t scanCount = max<uint16_t>(1U, lit);
            const uint16_t span = scanCount > 1U ? scanCount - 1U : 0U;
            const uint16_t cycle = max<uint16_t>(1U, span * 2U);
            const uint16_t phase = static_cast<uint16_t>((now / 64U) % cycle);
            const uint16_t nozzle = span == 0U ? 0U : (phase <= span ? phase : cycle - phase);
            const bool movingForward = span == 0U || phase <= span;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filament, static_cast<uint8_t>(28U + coverage * 72U / 255U)));
                const bool behind = movingForward ? i <= nozzle : i >= nozzle;
                const uint16_t distance = i > nozzle ? i - nozzle : nozzle - i;
                if (i < scanCount && behind && distance <= 5U) {
                    setSection(LedSection::Center, i, scaled(filament,
                        static_cast<uint8_t>(210U - distance * 28U)));
                }
            }
            setSection(LedSection::Center, min<uint16_t>(nozzle, hw::CenterCount - 1U), heat);
            break;
        }

        case PrintAnimation::BuildPlate: {
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const RgbwColor bed = temperatureColor(bedPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t coverage = progressCoverage(bedPercent, hw::LeftCount, i);
                const uint8_t value = coverage ? static_cast<uint8_t>(45U + coverage * 155U / 255U) : 12U;
                setSection(LedSection::Left, i, scaled(bed, value));
                setSection(LedSection::Right, i, scaled(bed, value));
            }
            const uint16_t pairCount = (hw::CenterCount + 1U) / 2U;
            const uint32_t footprint = static_cast<uint32_t>(progress) * pairCount * 255U / 100U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t pair = i < hw::CenterCount / 2U
                    ? hw::CenterCount / 2U - 1U - i : i - hw::CenterCount / 2U;
                const uint32_t start = static_cast<uint32_t>(pair) * 255U;
                const uint8_t coverage = footprint <= start ? 0U
                    : static_cast<uint8_t>(min<uint32_t>(255U, footprint - start));
                const uint8_t grid = (i % 2U) ? 30U : 48U;
                setSection(LedSection::Center, i,
                    coverage ? blend(scaled(bed, grid), filament, coverage)
                             : scaled(bed, grid));
            }
            break;
        }

        case PrintAnimation::MicroSteps: {
            RgbwColor phaseB = complementary(filament);
            if (phaseB.r == 0U && phaseB.g == 0U && phaseB.b == 0U) {
                phaseB = decorativeHsv(LedCategory::Print, 134U, 240U, 255U);
            }
            const uint8_t phase = static_cast<uint8_t>((now / 80U) % 4U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t coil = static_cast<uint8_t>((i + phase) % 4U);
                const uint8_t value = coil == 0U ? 255U : coil == 1U ? 105U : 18U;
                setSection(LedSection::Left, i, scaled(coil < 2U ? filament : phaseB, value));
                setSection(LedSection::Right, i, scaled(coil < 2U ? phaseB : filament, value));
            }
            const uint32_t edge = static_cast<uint32_t>(progress) * hw::CenterCount * 255U / 100U;
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(edge / 255U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i, scaled(filament,
                    static_cast<uint8_t>(32U + coverage * 88U / 255U)));
            }
            const int16_t offset = static_cast<int16_t>(phase) - 2;
            const int16_t microPosition = static_cast<int16_t>(marker) + (offset > 0 ? 1 : 0);
            if (microPosition >= 0 && microPosition < static_cast<int16_t>(hw::CenterCount)) {
                setSection(LedSection::Center, static_cast<uint16_t>(microPosition),
                           phase & 1U ? phaseB : filament);
            }
            break;
        }

        case PrintAnimation::FlowWave: {
            const uint8_t speed = static_cast<uint8_t>(16U + progress / 8U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t center = hw::OuterCount / 2U;
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t wave = wave8(static_cast<uint8_t>(now / speed - distance * 24U));
                uint8_t value = static_cast<uint8_t>(22U + wave * 190U / 255U);
                RgbwColor color = filament;
                if (path >= hw::LeftCount && path < hw::LeftCount + hw::CenterCount) {
                    const uint16_t centerIndex = path - hw::LeftCount;
                    const uint8_t coverage = progressCoverage(progress, hw::CenterCount, centerIndex);
                    value = static_cast<uint8_t>(value * coverage / 255U);
                    if (!coverage) color = RgbwColor();
                }
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }

        case PrintAnimation::ToolheadOrbit: {
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i,
                    scaled(filament, static_cast<uint8_t>(28U + coverage * 75U / 255U)));
            }
            constexpr uint16_t Anchors[4] = {2U, 10U, 31U, 39U};
            for (uint8_t tool = 0; tool < 4U; ++tool) {
                setOuterVisualPathPixel(Anchors[tool],
                    scaled(filamentPalette[tool], tool == (context.activeTool & 3U) ? 210U : 42U));
            }
            const uint8_t active = context.activeTool & 3U;
            const uint16_t anchor = Anchors[active];
            const uint8_t localPhase = static_cast<uint8_t>((now / 62U) % 16U);
            const int8_t localOffset = localPhase < 8U
                ? static_cast<int8_t>(localPhase) - 4
                : static_cast<int8_t>(11 - localPhase);
            const int16_t rawOrbit = static_cast<int16_t>(anchor) + localOffset;
            const uint16_t orbit = static_cast<uint16_t>(
                (rawOrbit + static_cast<int16_t>(hw::OuterCount)) % hw::OuterCount);
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            setOuterVisualPathPixel(orbit, temperatureColor(toolPercent));
            const uint16_t previous = static_cast<uint16_t>(
                (orbit + hw::OuterCount - (localOffset >= 0 ? 1U : hw::OuterCount - 1U)) % hw::OuterCount);
            setOuterVisualPathPixel(previous, scaled(filamentPalette[active], 110U));
            break;
        }

        case PrintAnimation::Wipe: {
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t value = static_cast<uint8_t>(165U + wave8(
                    static_cast<uint8_t>(now / 34U + i * 16U)) * 90U / 255U);
                setSection(LedSection::Left, i, scaled(filament, value));
                setSection(LedSection::Right, i, scaled(filament, value));
            }

            RgbwColor curtain = complementary(filament);
            const bool rainbowFallback = curtain.r == 0U && curtain.g == 0U && curtain.b == 0U;
            const uint16_t pairCount = (hw::CenterCount + 1U) / 2U;
            const uint32_t extinguished = static_cast<uint32_t>(progress) * pairCount * 255U / 100U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t pair = i < hw::CenterCount / 2U
                    ? hw::CenterCount / 2U - 1U - i : i - hw::CenterCount / 2U;
                const uint32_t pairStart = static_cast<uint32_t>(pair) * 255U;
                const uint8_t remaining = extinguished <= pairStart ? 255U
                    : extinguished >= pairStart + 255U ? 0U
                    : static_cast<uint8_t>(pairStart + 255U - extinguished);
                const RgbwColor color = rainbowFallback
                    ? decorativeHsv(LedCategory::Print,
                        static_cast<uint8_t>(i * 17U + now / 28U), 245U, remaining)
                    : scaled(curtain, remaining);
                setSection(LedSection::Center, i, color);
            }
            break;
        }

        case PrintAnimation::Shimmer: {
            const uint32_t tick = now / 72U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t grain = hash8(path * 73U + tick * 47U);
                const uint8_t slow = wave8(static_cast<uint8_t>(now / 31U + path * 9U));
                uint8_t value = static_cast<uint8_t>(78U + static_cast<uint16_t>(slow) * 62U / 255U);
                if (grain > 218U) value = static_cast<uint8_t>(170U + (grain - 218U) * 85U / 37U);
                setOuterVisualPathPixel(path, scaled(filament, value));
            }
            break;
        }

        case PrintAnimation::Bicolor: {
            fillFilamentSides(205U);
            uint8_t filamentHue = 0U;
            const bool chromatic = rgbHue(filament, filamentHue);
            const RgbwColor first = chromatic ? filament
                : decorativeHsv(LedCategory::Print, 132U, 245U, 255U);
            RgbwColor second = chromatic ? complementary(filament) : RgbwColor();
            if (second.r == 0U && second.g == 0U && second.b == 0U) {
                second = decorativeHsv(LedCategory::Print, 20U, 245U, 255U);
            }
            const uint16_t shift = static_cast<uint16_t>(now / 115U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool alternate = ((i + shift) / 2U) % 2U;
                const uint8_t edge = wave8(static_cast<uint8_t>(now / 28U + i * 12U));
                setSection(LedSection::Center, i,
                           scaled(alternate ? second : first,
                                  static_cast<uint8_t>(150U + edge * 105U / 255U)));
            }
            break;
        }

        case PrintAnimation::Thermometer: {
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            auto renderMeter = [&](LedSection section, uint8_t percent, const RgbwColor& color) {
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t coverage = progressCoverage(percent, count, i);
                    const uint8_t value = coverage
                        ? static_cast<uint8_t>(70U + static_cast<uint16_t>(coverage) * 185U / 255U)
                        : 18U;
                    setSection(section, i, scaled(color, value));
                }
            };
            renderMeter(LedSection::Left, bedPercent, temperatureColor(bedPercent));
            renderMeter(LedSection::Right, chamberPercent, temperatureColor(chamberPercent));
            const RgbwColor toolColor = temperatureColor(toolPercent);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t value = static_cast<uint8_t>(62U + static_cast<uint16_t>(coverage) * 193U / 255U);
                setSection(LedSection::Center, i, scaled(toolColor, value));
            }
            break;
        }

        case PrintAnimation::Snake: {
            const bool previewChanged = snakeWasPreview_ != context.preview ||
                (context.preview && snakePreviewStartedMs_ != previewStartedMs_);
            if (animationChanged || previewChanged) {
                snakeWasPreview_ = context.preview;
                snakePreviewStartedMs_ = previewStartedMs_;
                snakeLastStepMs_ = now;
                snakeHead_ = 0U;
                snakeFood_ = 8U;
                snakeLength_ = 2U;
                snakeGrowthBucket_ = 0U;
            }

            if (context.finishing) {
                const uint32_t elapsed = now - snakeFinishStartedMs_;
                const uint16_t origin = snakeHead_ % hw::OuterCount;
                const uint16_t radius = elapsed < 120U ? 0U
                    : min<uint16_t>(hw::OuterCount / 2U,
                        static_cast<uint16_t>((elapsed - 120U) * (hw::OuterCount / 2U + 3U) /
                                              (SnakeFinishDurationMs - 120U)));
                const uint8_t fade = elapsed >= SnakeFinishDurationMs ? 0U
                    : static_cast<uint8_t>(255U - elapsed * 190U / SnakeFinishDurationMs);
                for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                    const uint16_t direct = path > origin ? path - origin : origin - path;
                    const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                    const uint16_t ringDistance = distance > radius ? distance - radius : radius - distance;
                    const uint8_t spark = hash8(path * 97U + elapsed / 55U * 31U);
                    uint8_t value = ringDistance <= 2U
                        ? static_cast<uint8_t>(max<int16_t>(0, static_cast<int16_t>(fade) - ringDistance * 55))
                        : 0U;
                    if (spark > 238U && distance <= radius + 5U) value = max<uint8_t>(value, spark);
                    if (value) {
                        setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Print,
                            static_cast<uint8_t>(path * 19U + elapsed / 7U), 245U, value));
                    }
                }
                break;
            }

            const uint16_t stepMs = max<uint16_t>(55U, static_cast<uint16_t>(125U - progress / 2U));
            if (now - snakeLastStepMs_ >= stepMs) {
                const uint16_t steps = min<uint16_t>(4U, (now - snakeLastStepMs_) / stepMs);
                snakeLastStepMs_ += steps * stepMs;
                for (uint16_t step = 0; step < steps; ++step) {
                    snakeHead_ = static_cast<uint16_t>((snakeHead_ + 1U) % hw::OuterCount);
                    if (snakeHead_ == snakeFood_) {
                        const uint8_t targetLength = min<uint8_t>(24U, static_cast<uint8_t>(2U + progress / 5U));
                        if (snakeLength_ < targetLength) ++snakeLength_;
                        snakeFood_ = static_cast<uint16_t>((snakeFood_ + 7U +
                            hash8(now + snakeGrowthBucket_++ * 43U) % 17U) % hw::OuterCount);
                    }
                }
            }
            for (uint8_t body = 0; body < snakeLength_; ++body) {
                const uint16_t path = static_cast<uint16_t>(
                    (snakeHead_ + hw::OuterCount - body) % hw::OuterCount);
                const uint8_t value = snakeLength_ <= 1U ? 255U
                    : static_cast<uint8_t>(255U - static_cast<uint16_t>(body) * 175U /
                                                        (snakeLength_ - 1U));
                setOuterVisualPathPixel(path, scaled(filament, value));
            }
            setOuterVisualPathPixel(snakeFood_, decorativeHsv(
                LedCategory::Print, 20U, 255U,
                static_cast<uint8_t>(175U + wave8(now / 13U) * 80U / 255U)));
            break;
        }

        case PrintAnimation::RainbowProgress: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t coverage = progressCoverage(progress, hw::OuterCount, path);
                if (!coverage) continue;
                const uint8_t hue = static_cast<uint8_t>(path * 256U / hw::OuterCount + now / 70U);
                setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Print, hue, 255U,
                    static_cast<uint8_t>(static_cast<uint16_t>(coverage) * 225U / 255U)));
            }
            break;
        }

        case PrintAnimation::Heartbeat: {
            const uint16_t cycle = static_cast<uint16_t>(now % 1320U);
            auto beat = [&](uint16_t center, uint16_t width) {
                const uint16_t distance = cycle > center ? cycle - center : center - cycle;
                if (distance >= width) return static_cast<uint8_t>(0U);
                return wave8(static_cast<uint8_t>((width - distance) * 128U / width));
            };
            const uint8_t pulse = max<uint8_t>(beat(180U, 150U), beat(475U, 115U));
            const uint8_t strength = static_cast<uint8_t>(65U + progress * 190U / 100U);
            const uint8_t value = static_cast<uint8_t>(25U + static_cast<uint16_t>(pulse) * strength / 255U);
            fillSection(LedSection::Left, scaled(filament, value));
            fillSection(LedSection::Center, scaled(filament, value));
            fillSection(LedSection::Right, scaled(filament, value));
            const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
                static_cast<uint16_t>(progress * (hw::CenterCount - 1U) / 100U));
            if (pulse > 80U) setSection(LedSection::Center, marker, filament);
            break;
        }

        case PrintAnimation::DnaHelix: {
            uint8_t filamentHue = 0U;
            const bool chromatic = rgbHue(filament, filamentHue);
            const RgbwColor strandA = chromatic ? filament
                : decorativeHsv(LedCategory::Print, 138U, 250U, 255U);
            RgbwColor strandB = chromatic ? complementary(filament) : RgbwColor();
            if (strandB.r == 0U && strandB.g == 0U && strandB.b == 0U) {
                strandB = decorativeHsv(LedCategory::Print, 232U, 235U, 255U);
            }
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t phase = static_cast<uint8_t>(now / 22U + path * 31U);
                const uint8_t helixA = wave8(phase);
                const uint8_t helixB = wave8(static_cast<uint8_t>(phase + 128U));
                const bool useA = helixA >= helixB;
                const uint8_t value = static_cast<uint8_t>(48U + max<uint8_t>(helixA, helixB) * 207U / 255U);
                setOuterVisualPathPixel(path, scaled(useA ? strandA : strandB, value));
            }
            break;
        }

        case PrintAnimation::PixelRain: {
            constexpr LedSection Sides[2] = {LedSection::Left, LedSection::Right};
            const uint32_t tick = now / 85U;
            for (uint8_t sideIndex = 0; sideIndex < 2U; ++sideIndex) {
                const LedSection section = Sides[sideIndex];
                const uint16_t count = sectionCount(section);
                for (uint8_t drop = 0; drop < 4U; ++drop) {
                    const uint16_t cycle = count + 7U;
                    const uint16_t head = static_cast<uint16_t>((tick * (drop + 1U) +
                        hash8(drop * 71U + sideIndex * 137U)) % cycle);
                    for (uint8_t tail = 0; tail < 4U; ++tail) {
                        if (head < tail) continue;
                        const uint16_t position = head - tail;
                        if (position >= count) continue;
                        const uint8_t value = static_cast<uint8_t>(235U - tail * 55U);
                        setSection(section, position, scaled(filament, value));
                    }
                }
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const uint8_t shimmer = wave8(static_cast<uint8_t>(now / 24U + i * 23U));
                const uint8_t value = static_cast<uint8_t>(72U + shimmer * 95U / 255U);
                setSection(LedSection::Center, i, scaled(filament,
                    static_cast<uint8_t>(static_cast<uint16_t>(coverage) * value / 255U)));
            }
            if (lit) setSection(LedSection::Center, lit - 1U, filament);
            break;
        }

        case PrintAnimation::Orbit: {
            const uint16_t period = max<uint16_t>(38U, static_cast<uint16_t>(92U - progress / 2U));
            const uint16_t first = static_cast<uint16_t>((now / period) % hw::OuterCount);
            const uint16_t second = static_cast<uint16_t>((first + hw::OuterCount / 2U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t d1raw = path > first ? path - first : first - path;
                const uint16_t d2raw = path > second ? path - second : second - path;
                const uint16_t d1 = min<uint16_t>(d1raw, hw::OuterCount - d1raw);
                const uint16_t d2 = min<uint16_t>(d2raw, hw::OuterCount - d2raw);
                const uint16_t distance = min<uint16_t>(d1, d2);
                const uint8_t value = distance > 3U ? 18U
                    : static_cast<uint8_t>(255U - distance * 63U);
                const RgbwColor color = distance == 0U
                    ? decorativeHsv(LedCategory::Print,
                        static_cast<uint8_t>(path * 9U + now / 18U), 245U, value)
                    : scaled(filament, value);
                setOuterVisualPathPixel(path, color);
            }
            break;
        }

        case PrintAnimation::Laser: {
            fillFilamentSides();
            uint8_t filamentHue = 0U;
            const bool chromatic = rgbHue(filament, filamentHue);
            const bool redFilament = chromatic && (filamentHue <= 18U || filamentHue >= 238U);
            const RgbwColor laser = redFilament ? RgbwColor(0, 255, 40) : RgbwColor(255, 0, 0);
            const uint16_t span = max<uint16_t>(1U, lit);
            const uint16_t phase = static_cast<uint16_t>((now / 34U) % (span * 2U));
            const uint16_t head = phase < span ? phase : static_cast<uint16_t>(span * 2U - 1U - phase);
            const uint16_t tip = lit ? lit - 1U : 0U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                uint8_t value = static_cast<uint8_t>(progressCoverage(progress, hw::CenterCount, i) * 18U / 255U);
                const uint16_t headDistance = i > head ? i - head : head - i;
                if (lit && i < lit && headDistance <= 3U) {
                    value = max<uint8_t>(value, headDistance == 0U ? 255U
                        : static_cast<uint8_t>(150U - headDistance * 38U));
                }
                const uint16_t tipDistance = i > tip ? i - tip : tip - i;
                if (lit && tipDistance <= 1U) value = max<uint8_t>(value, tipDistance ? 95U : 255U);
                if (value) setSection(LedSection::Center, i, scaled(laser, value));
            }
            break;
        }

        case PrintAnimation::Wave: {
            const uint8_t sidePulse = static_cast<uint8_t>(60U + wave8(now / 20U) / 2U);
            fillFilamentSides(sidePulse);
            const RgbwColor opposite = complementary(filament);
            const bool rainbowFallback = opposite.r == 0U && opposite.g == 0U && opposite.b == 0U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const uint8_t waveA = wave8(static_cast<uint8_t>(now / 18U + i * 26U));
                const uint8_t waveB = wave8(static_cast<uint8_t>(now / 29U + i * 13U + 85U));
                const uint8_t crest = static_cast<uint8_t>((static_cast<uint16_t>(waveA) * 3U + waveB) / 4U);
                const uint8_t value = static_cast<uint8_t>(24U + static_cast<uint16_t>(crest) * 231U / 255U);
                const RgbwColor color = rainbowFallback
                    ? decorativeHsv(LedCategory::Print, static_cast<uint8_t>(now / 20U + i * 10U), 255U, value)
                    : scaled(opposite, value);
                setSection(LedSection::Center, i, scaled(color, coverage));
            }
            break;
        }

        case PrintAnimation::Thermal: {
            float temperature = isnan(context.chamberTempC) ? 40.0f : context.chamberTempC;
            if (temperature < 0.0f) temperature = 0.0f;
            if (temperature > 80.0f) temperature = 80.0f;
            const uint8_t redAmount = temperature <= 20.0f ? 0U : temperature >= 60.0f ? 255U
                : static_cast<uint8_t>((temperature - 20.0f) * 255.0f / 40.0f);
            uint8_t thermalValue = static_cast<uint8_t>(155.0f + fabsf(temperature - 40.0f) * 65.0f / 40.0f);
            if (temperature < 20.0f || temperature > 60.0f) {
                const float edge = temperature < 20.0f ? 20.0f - temperature : temperature - 60.0f;
                const uint16_t period = max<uint16_t>(460U, static_cast<uint16_t>(1700.0f - edge * 62.0f));
                const uint8_t breath = wave8(static_cast<uint8_t>(now / max<uint16_t>(2U, period / 256U)));
                thermalValue = static_cast<uint8_t>(static_cast<uint16_t>(thermalValue) *
                    (145U + static_cast<uint16_t>(breath) * 110U / 255U) / 255U);
            }
            auto thermalPixel = [&](LedSection section, uint16_t i, uint16_t count) {
                const uint8_t position = static_cast<uint8_t>(i * 255U / (count - 1U));
                const int16_t threshold = 255 - redAmount;
                int16_t mix = (static_cast<int16_t>(position) - (threshold - 58)) * 255 / 116;
                mix = max<int16_t>(0, min<int16_t>(255, mix));
                const uint8_t red = static_cast<uint8_t>(mix);
                const uint8_t blue = static_cast<uint8_t>(255U - red);
                const uint8_t green = static_cast<uint8_t>(
                    static_cast<uint16_t>(min<uint8_t>(red, blue)) * 45U / 128U);
                setSection(section, i, scaled(RgbwColor(red, green, blue), thermalValue));
            };
            for (uint16_t i = 0; i < hw::LeftCount; ++i) thermalPixel(LedSection::Left, i, hw::LeftCount);
            for (uint16_t i = 0; i < hw::RightCount; ++i) thermalPixel(LedSection::Right, i, hw::RightCount);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                setSection(LedSection::Center, i,
                           scaled(filament, progressCoverage(progress, hw::CenterCount, i)));
            }
            break;
        }

        case PrintAnimation::Stripes: {
            fillFilamentSides();
            uint8_t baseHue = 18U;
            const bool filamentHasHue = rgbHue(filament, baseHue);
            RgbwColor colors[4] = {
                filamentHasHue ? filament : decorativeHsv(LedCategory::Print, 18U, 240U, 255U),
                decorativeHsv(LedCategory::Print, static_cast<uint8_t>(baseHue + 53U), 235U, 255U),
                decorativeHsv(LedCategory::Print, static_cast<uint8_t>(baseHue + 117U), 235U, 255U),
                decorativeHsv(LedCategory::Print, static_cast<uint8_t>(baseHue + 181U), 235U, 255U),
            };
            const uint32_t shift = now / 95U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const uint8_t colorIndex = static_cast<uint8_t>((i + shift) % 4U);
                const uint8_t shimmer = wave8(static_cast<uint8_t>(now / 13U + i * 31U + colorIndex * 47U));
                const uint8_t value = static_cast<uint8_t>(90U + static_cast<uint16_t>(shimmer) * 165U / 255U);
                setSection(LedSection::Center, i, scaled(colors[colorIndex],
                    static_cast<uint8_t>(static_cast<uint16_t>(coverage) * value / 255U)));
            }
            break;
        }

        case PrintAnimation::ProgressPulse: {
            const uint16_t period = max<uint16_t>(200U, static_cast<uint16_t>(800U - progress * 6U));
            const uint8_t pulse = static_cast<uint8_t>(35U + static_cast<uint16_t>(wave8(
                static_cast<uint8_t>(now * 256U / period))) * 180U / 255U);
            fillSection(LedSection::Left, scaled(filament, pulse));
            fillSection(LedSection::Center, scaled(filament, pulse));
            fillSection(LedSection::Right, scaled(filament, pulse));
            const uint16_t marker = static_cast<uint16_t>((progress * (hw::CenterCount - 1U) + 50U) / 100U);
            for (int8_t offset = -1; offset <= 1; ++offset) {
                const int16_t position = static_cast<int16_t>(marker) + offset;
                if (position >= 0 && position < static_cast<int16_t>(hw::CenterCount)) {
                    setSection(LedSection::Center, static_cast<uint16_t>(position), filament);
                }
            }
            break;
        }

        case PrintAnimation::Comet: {
            const uint16_t head = static_cast<uint16_t>((now / 30U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t trail = static_cast<uint16_t>((head + hw::OuterCount - path) % hw::OuterCount);
                if (trail > 9U) continue;
                setOuterVisualPathPixel(path, scaled(filament, static_cast<uint8_t>(255U - trail * 25U)));
            }
            break;
        }

        case PrintAnimation::ActiveSection: {
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                setSection(LedSection::Center, i,
                           scaled(filament, progressCoverage(progress, hw::CenterCount, i)));
            }
            const uint8_t density = static_cast<uint8_t>(18U + static_cast<uint16_t>(progress) * 150U / 100U);
            const uint16_t tempo = max<uint16_t>(45U, static_cast<uint16_t>(190U - progress * 130U / 100U));
            const uint32_t tick = now / tempo;
            constexpr LedSection Sides[2] = {LedSection::Left, LedSection::Right};
            for (LedSection section : Sides) {
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t seed = hash8(i * 97U + static_cast<uint8_t>(section) * 701U + tick * 41U);
                    if (seed >= density) continue;
                    const uint8_t hue = hash8(i * 53U + tick * 23U + 7U);
                    const uint8_t value = static_cast<uint8_t>(80U + hash8(i * 31U + tick * 13U) * 150U / 255U);
                    setSection(section, i, decorativeHsv(LedCategory::Print, hue, 255U, value));
                }
            }
            break;
        }

        case PrintAnimation::Running: {
            fillFilamentSides();
            const uint32_t shift = now / 60U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool on = ((i + shift) % 5U) < 3U;
                setSection(LedSection::Center, i, scaled(filament, on ? 255U : 42U));
            }
            break;
        }

        case PrintAnimation::Breathe: {
            const uint8_t value = static_cast<uint8_t>(60U + wave8(now / 32U) * 195U / 255U);
            fillSection(LedSection::Left, scaled(filament, value));
            fillSection(LedSection::Center, scaled(filament, value));
            fillSection(LedSection::Right, scaled(filament, value));
            break;
        }

        case PrintAnimation::ProgressBar:
        case PrintAnimation::Count:
        default: {
            fillFilamentSides();
            const RgbwColor opposite = complementary(filament);
            const bool rainbowFallback = opposite.r == 0U && opposite.g == 0U && opposite.b == 0U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (!coverage) continue;
                const RgbwColor color = rainbowFallback
                    ? decorativeHsv(LedCategory::Print, static_cast<uint8_t>(i * 13U + now / 20U), 255U, 230U)
                    : opposite;
                setSection(LedSection::Center, i, scaled(color, coverage));
            }
            break;
        }
    }
}

void LedService::renderPause(uint8_t animation, const LedAnimationContext& context) {
    const uint32_t now = context.nowMs;
    const uint8_t progress = min<uint8_t>(context.progress, 100U);
    const RgbwColor rawFilament = fromRgb(context.filamentRgb);
    const uint8_t filamentMaximum = max(rawFilament.r, max(rawFilament.g, rawFilament.b));
    const RgbwColor filament = filamentMaximum < 12U
        ? decorativeHsv(LedCategory::Pause, static_cast<uint8_t>(now / 24U), 245U, 235U)
        : rawFilament;
    const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U,
        static_cast<uint16_t>(progress) * (hw::CenterCount - 1U) / 100U);
    const uint16_t lit = static_cast<uint16_t>((progress * hw::CenterCount + 99U) / 100U);
    const RgbwColor amber = decorativeHsv(LedCategory::Pause, 24U, 255U, 255U);
    const RgbwColor cool = decorativeHsv(LedCategory::Pause, 145U, 235U, 255U);

    auto frozenProgress = [&](const RgbwColor& color, uint8_t filledValue,
                              uint8_t emptyValue, uint8_t markerValue) {
        for (uint16_t i = 0; i < hw::CenterCount; ++i) {
            const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
            uint8_t value = coverage
                ? static_cast<uint8_t>(static_cast<uint16_t>(coverage) * filledValue / 255U)
                : emptyValue;
            const uint16_t distance = i > marker ? i - marker : marker - i;
            if (distance == 0U) value = markerValue;
            else if (distance == 1U) value = max<uint8_t>(value, markerValue / 3U);
            setSection(LedSection::Center, i, scaled(color, value));
        }
    };

    switch (static_cast<PauseAnimation>(animation)) {
        case PauseAnimation::Amber: {
            const uint8_t breath = static_cast<uint8_t>(52U + wave8(now / 34U) * 118U / 255U);
            fillSection(LedSection::Left, scaled(amber, breath));
            fillSection(LedSection::Right, scaled(amber, breath));
            frozenProgress(amber, static_cast<uint8_t>(34U + breath / 3U), 5U,
                           static_cast<uint8_t>(145U + breath / 3U));
            break;
        }
        case PauseAnimation::Hazard: {
            const uint32_t cycle = now % 1800U;
            const bool firstFlash = cycle < 120U;
            const bool secondFlash = cycle >= 230U && cycle < 350U;
            const bool leftActive = cycle < 900U;
            const uint8_t activeValue = (firstFlash || secondFlash) ? 235U : 24U;
            fillSection(LedSection::Left, scaled(amber, leftActive ? activeValue : 12U));
            fillSection(LedSection::Right, scaled(amber, leftActive ? 12U : activeValue));
            frozenProgress(amber, 42U, 4U, (firstFlash || secondFlash) ? 210U : 100U);
            break;
        }
        case PauseAnimation::Freeze: {
            const uint8_t frostBreath = static_cast<uint8_t>(42U + wave8(now / 62U) / 5U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t crystal = hash8(i * 89U + sectionIndex * 977U + now / 620U);
                    uint8_t value = frostBreath;
                    if (crystal > 238U) value = static_cast<uint8_t>(170U + (crystal - 238U) * 5U);
                    if (section == LedSection::Center && i >= lit) value /= 5U;
                    setSection(section, i, scaled(cool, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(RgbwColor(220U, 245U, 255U), 205U));
            break;
        }
        case PauseAnimation::Radar: {
            const uint16_t head = static_cast<uint16_t>((now / 78U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = head >= path ? head - path : head + hw::OuterCount - path;
                uint8_t value = 6U;
                if (distance < 9U) {
                    value = static_cast<uint8_t>(220U - distance * 23U);
                } else if ((path + now / 420U) % 11U == 0U) {
                    value = 24U;
                }
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 220U));
            break;
        }
        case PauseAnimation::Heartbeat: {
            const uint32_t beat = now % 1900U;
            uint8_t strength = 18U;
            if (beat < 95U) strength = static_cast<uint8_t>(90U + beat * 165U / 95U);
            else if (beat < 190U) strength = static_cast<uint8_t>(255U - (beat - 95U) * 225U / 95U);
            else if (beat >= 280U && beat < 370U) strength = static_cast<uint8_t>(70U + (beat - 280U) * 160U / 90U);
            else if (beat >= 370U && beat < 500U) strength = static_cast<uint8_t>(230U - (beat - 370U) * 212U / 130U);
            fillSection(LedSection::Left, scaled(amber, strength));
            fillSection(LedSection::Right, scaled(amber, strength));
            frozenProgress(filament, 42U, 3U, max<uint8_t>(100U, strength));
            break;
        }
        case PauseAnimation::ProgressBar: {
            const uint8_t boundaryPulse = static_cast<uint8_t>(145U + wave8(now / 25U) / 3U);
            fillSection(LedSection::Left, scaled(filament, 72U));
            fillSection(LedSection::Right, scaled(filament, 72U));
            frozenProgress(amber, 118U, 4U, boundaryPulse);
            break;
        }
        case PauseAnimation::Crossfade: {
            const uint8_t amount = wave8(now / 47U);
            const RgbwColor mixed = blend(amber, cool, amount);
            fillSection(LedSection::Left, scaled(mixed, 100U));
            fillSection(LedSection::Right, scaled(blend(cool, amber, amount), 100U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t localAmount = static_cast<uint8_t>(amount + i * 255U / hw::CenterCount);
                const RgbwColor color = blend(amber, cool, wave8(localAmount));
                const uint16_t distance = i > marker ? i - marker : marker - i;
                setSection(LedSection::Center, i,
                           scaled(color, distance == 0U ? 205U : (i < lit ? 58U : 8U)));
            }
            break;
        }
        case PauseAnimation::Phase: {
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint8_t phase = static_cast<uint8_t>(now / 31U + sectionIndex * 85U);
                const uint8_t value = static_cast<uint8_t>(20U + wave8(phase) * 155U / 255U);
                fillSection(section, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 230U));
            break;
        }
        case PauseAnimation::YellowWhite: {
            const uint8_t amount = wave8(now / 43U);
            const RgbwColor warmWhite(235U, 225U, 190U);
            const RgbwColor color = blend(amber, warmWhite, amount);
            const uint8_t value = static_cast<uint8_t>(75U + wave8(now / 61U) / 3U);
            fillSection(LedSection::Left, scaled(color, value));
            fillSection(LedSection::Right, scaled(color, value));
            frozenProgress(color, 90U, 5U, 205U);
            break;
        }
        case PauseAnimation::WatchfulEyes: {
            const uint32_t cycle = now % 5600U;
            int16_t glance = 0;
            if (cycle >= 1100U && cycle < 1900U) glance = static_cast<int16_t>((cycle - 1100U) * 4U / 800U);
            else if (cycle >= 1900U && cycle < 2700U) glance = static_cast<int16_t>(4 - (cycle - 1900U) * 4U / 800U);
            else if (cycle >= 3300U && cycle < 4100U) glance = -static_cast<int16_t>((cycle - 3300U) * 4U / 800U);
            else if (cycle >= 4100U && cycle < 4900U) glance = static_cast<int16_t>(-4 + (cycle - 4100U) * 4U / 800U);
            const uint16_t eye = static_cast<uint16_t>(max<int16_t>(1, min<int16_t>(hw::LeftCount - 2U,
                static_cast<int16_t>(hw::LeftCount / 2U) + glance)));
            fillSection(LedSection::Left, scaled(amber, 10U));
            fillSection(LedSection::Right, scaled(amber, 10U));
            for (int8_t offset = -1; offset <= 1; ++offset) {
                const uint8_t value = offset == 0 ? 225U : 58U;
                setSection(LedSection::Left, static_cast<uint16_t>(eye + offset), scaled(amber, value));
                setSection(LedSection::Right, static_cast<uint16_t>(eye + offset), scaled(amber, value));
            }
            frozenProgress(filament, 24U, 2U, 85U);
            break;
        }
        case PauseAnimation::Count:
        default:
            break;
    }
}

void LedService::renderError(uint8_t animation, uint32_t now) {
    const bool on = animation % 3U == 1U ? ((now / 120U) % 4U < 2U)
                                         : wave8(static_cast<uint8_t>(now / 8U)) > 70U;
    const uint8_t value = on ? 255U : 12U;
    const RgbwColor warning = decorativeHsv(LedCategory::Error, 0, 255, value);
    fillSection(LedSection::Left, warning);
    fillSection(LedSection::Center, warning);
    fillSection(LedSection::Right, warning);
}

void LedService::renderFinish(uint8_t animation, uint32_t now, uint32_t filamentRgb) {
    const RgbwColor filament = fromRgb(filamentRgb);
    for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
        const LedSection section = VisualOuterSections[sectionIndex];
        const uint16_t count = sectionCount(section);
        for (uint16_t i = 0; i < count; ++i) {
            const uint8_t hue = static_cast<uint8_t>(now / 10U + i * 19U + sectionIndex * 61U);
            const uint8_t sparkle = hash8(i * 37U + sectionIndex * 991U + now / 70U);
            RgbwColor color = animation % 3U == 0 ? decorativeHsv(LedCategory::Finish, hue, 240, 150)
                                                   : scaled(filament, 100);
            if (sparkle > 222U) color = decorativeHsv(LedCategory::Finish, sparkle + hue, 210, 255);
            setSection(section, i, color);
        }
    }
}

void LedService::renderOther(uint8_t animation, uint32_t now) {
    for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
        const LedSection section = VisualOuterSections[sectionIndex];
        const uint16_t count = sectionCount(section);
        for (uint16_t i = 0; i < count; ++i) {
            RgbwColor color;
            switch (animation % 4U) {
                case 1:
                    color = decorativeHsv(LedCategory::Other,
                        static_cast<uint8_t>(145U + wave8(static_cast<uint8_t>(now / 24U + i * 11U)) / 4U),
                        210, static_cast<uint8_t>(35U + wave8(static_cast<uint8_t>(now / 17U + i * 15U)) / 2U));
                    break;
                case 2: {
                    const uint8_t flicker = static_cast<uint8_t>(120U + hash8(i * 67U + now / 45U) / 2U);
                    color = RgbwColor(flicker, static_cast<uint8_t>(flicker / 5U), 0);
                    break;
                }
                case 3:
                    color = decorativeHsv(LedCategory::Other,
                        static_cast<uint8_t>(132U + i * 2U), 235,
                        static_cast<uint8_t>(30U + wave8(static_cast<uint8_t>(now / 20U - i * 13U)) / 2U));
                    break;
                case 0:
                default:
                    color = decorativeHsv(LedCategory::Other,
                        static_cast<uint8_t>(now / 14U + i * 256U / count + sectionIndex * 29U), 255, 190);
                    break;
            }
            setSection(section, i, color);
        }
    }
}

void LedService::applyInsidePolicy() {
    const AppSettings& settings = settingsService().settings();
    if (settings.insideColorStyle == InsideColorStyle::White) {
        fillSection(LedSection::Inside, RgbwColor(0, 0, 0, 255));
        return;
    }

    for (uint16_t i = 0; i < hw::InsideCount; ++i) {
        const uint16_t outerCenter = static_cast<uint16_t>(i * (hw::OuterCount - 1U) / (hw::InsideCount - 1U));
        uint32_t r = 0, g = 0, b = 0, w = 0, total = 0;
        static constexpr int8_t Offsets[5] = {-2, -1, 0, 1, 2};
        static constexpr uint8_t Weights[5] = {1, 2, 4, 2, 1};
        for (uint8_t sample = 0; sample < 5U; ++sample) {
            int16_t path = static_cast<int16_t>(outerCenter) + Offsets[sample];
            if (path < 0) path = 0;
            if (path >= static_cast<int16_t>(hw::OuterCount)) path = hw::OuterCount - 1U;
            RgbwColor color;
            if (path < hw::RightCount) color = targetFrame_[sectionPhysicalIndex(LedSection::Right, path)];
            else if (path < hw::RightCount + hw::CenterCount) {
                const uint16_t centerFromRight = path - hw::RightCount;
                color = targetFrame_[sectionPhysicalIndex(LedSection::Center,
                    hw::CenterCount - 1U - centerFromRight)];
            } else {
                const uint16_t leftFromBottom = path - hw::RightCount - hw::CenterCount;
                color = targetFrame_[sectionPhysicalIndex(LedSection::Left,
                    hw::LeftCount - 1U - leftFromBottom)];
            }
            const uint8_t weight = Weights[sample];
            r += static_cast<uint32_t>(color.r) * weight;
            g += static_cast<uint32_t>(color.g) * weight;
            b += static_cast<uint32_t>(color.b) * weight;
            w += static_cast<uint32_t>(color.w) * weight;
            total += weight;
        }
        setSection(LedSection::Inside, hw::InsideCount - 1U - i,
                   RgbwColor(static_cast<uint8_t>(r / total), static_cast<uint8_t>(g / total),
                             static_cast<uint8_t>(b / total), static_cast<uint8_t>((w / total) * 3U / 5U)));
    }
}

void LedService::applyOutputPolicies() {
    const AppSettings& settings = settingsService().settings();
    if (bootActive_) {
        uint16_t brightnessSum = 0;
        for (uint8_t index = 0; index < enumCount(LedSection{}); ++index) {
            brightnessSum += settings.ledBrightness[index];
        }
        const uint8_t average = static_cast<uint8_t>((brightnessSum + 2U) / enumCount(LedSection{}));
        const uint32_t elapsed = bootExperience().timelineMs();
        const uint8_t handoff = bootExperience().full()
            ? (elapsed > FullLedHandoffStartMs
                ? static_cast<uint8_t>(min<uint32_t>(255U,
                    (elapsed - FullLedHandoffStartMs) * 255U /
                    (BootExperience::FullDurationMs - FullLedHandoffStartMs))) : 0U)
            : (elapsed > QuickLedHandoffStartMs
                ? static_cast<uint8_t>(min<uint32_t>(255U,
                    (elapsed - QuickLedHandoffStartMs) * 255U /
                    (BootExperience::QuickDurationMs - QuickLedHandoffStartMs))) : 0U);
        for (uint8_t sectionIndex = 0; sectionIndex < enumCount(LedSection{}); ++sectionIndex) {
            const uint8_t sectionPercent = settings.ledBrightness[sectionIndex];
            const uint8_t percent = static_cast<uint8_t>(
                (static_cast<uint16_t>(average) * (255U - handoff) +
                 static_cast<uint16_t>(sectionPercent) * handoff + 127U) / 255U);
            const uint8_t scaleValue = static_cast<uint8_t>(static_cast<uint16_t>(percent) * 255U / 100U);
            const LedSection section = static_cast<LedSection>(sectionIndex);
            const uint16_t count = sectionCount(section);
            for (uint16_t i = 0; i < count; ++i) {
                const uint16_t physical = sectionPhysicalIndex(section, i);
                targetFrame_[physical] = scaled(targetFrame_[physical], scaleValue);
            }
        }
        return;
    }

    const bool dimmAllowed = state().printerState != PrinterState::Error;
    const uint32_t activityBase = state().lastTouchMs ? state().lastTouchMs : state().bootMs;
    const bool inactive = millis() - activityBase >= 5UL * 60UL * 1000UL;
    for (uint8_t sectionIndex = 0; sectionIndex < enumCount(LedSection{}); ++sectionIndex) {
        const bool dimmed = dimmAllowed && inactive && settings.ledDimmEnabled[sectionIndex];
        const uint8_t percent = dimmed ? settings.ledDimmPercent[sectionIndex]
                                       : settings.ledBrightness[sectionIndex];
        const uint32_t scaleValue = dimmed
            ? static_cast<uint32_t>(percent) * percent * 255U / 10000U
            : static_cast<uint32_t>(percent) * 255U / 100U;
        const LedSection section = static_cast<LedSection>(sectionIndex);
        const uint16_t count = sectionCount(section);
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t physical = sectionPhysicalIndex(section, i);
            targetFrame_[physical] = scaled(targetFrame_[physical], static_cast<uint8_t>(scaleValue));
        }
    }
}

bool LedService::smoothAndShow(bool immediate) {
    bool dirty = false;
    const uint8_t blendAmount = immediate ? 255U : 96U;
    portENTER_CRITICAL(&frameMux_);
    for (uint16_t i = 0; i < hw::LedCount; ++i) {
        const RgbwColor next = approach(currentFrame_[i], targetFrame_[i], blendAmount);
        if (memcmp(&next, &currentFrame_[i], sizeof(next)) != 0) {
            currentFrame_[i] = next;
            dirty = true;
        }
    }
    portEXIT_CRITICAL(&frameMux_);
    if (!dirty) {
        ++skippedShows_;
        return false;
    }
    encodeFrame();
    transmitEncodedFrame();
    ++shows_;
    return true;
}

void LedService::transmitEncodedFrame() {
    // Arduino's polling SPI writer emits this 960-byte frame as 15 separate
    // 64-byte hardware transactions. A long Core 1 interrupt between chunks
    // exceeds the SK6812 reset interval and latches a partial frame. Keep the
    // complete 2.4 ms waveform uninterrupted; Core 0 remains available for
    // audio, networking and other time-sensitive work.
    portENTER_CRITICAL(&outputMux_);
    spi_->writeBytes(txBuffer_, hw::LedCount * 16U);
    portEXIT_CRITICAL(&outputMux_);
    delayMicroseconds(100);
}

void LedService::encodeFrame() {
    size_t output = 0;
    auto append = [&](uint8_t value) {
        const uint8_t high = value >> 4U;
        const uint8_t low = value & 0x0FU;
        txBuffer_[output++] = NibbleLutHi[high];
        txBuffer_[output++] = NibbleLutLo[high];
        txBuffer_[output++] = NibbleLutHi[low];
        txBuffer_[output++] = NibbleLutLo[low];
    };
    for (uint16_t outputIndex = 0; outputIndex < hw::LedCount; ++outputIndex) {
        const RgbwColor& color = currentFrame_[outputIndex];
        append(color.g);
        append(color.r);
        append(color.b);
        append(color.w);
    }
}

void LedService::clearTarget() {
    memset(targetFrame_, 0, sizeof(RgbwColor) * hw::LedCount);
}

void LedService::setPhysical(uint16_t index, const RgbwColor& color) {
    if (index < hw::LedCount) targetFrame_[index] = color;
}

void LedService::setSection(LedSection section, uint16_t logical, const RgbwColor& color) {
    if (logical >= sectionCount(section)) return;
    setPhysical(sectionPhysicalIndex(section, logical), color);
}

void LedService::setOuterVisualPathPixel(uint16_t path, const RgbwColor& color) {
    if (path >= hw::OuterCount) return;
    if (path < hw::LeftCount) {
        setSection(LedSection::Left, path, color);
        return;
    }
    path -= hw::LeftCount;
    if (path < hw::CenterCount) {
        setSection(LedSection::Center, path, color);
        return;
    }
    path -= hw::CenterCount;
    setSection(LedSection::Right, hw::RightCount - 1U - path, color);
}

void LedService::fillSection(LedSection section, const RgbwColor& color) {
    const uint16_t count = sectionCount(section);
    for (uint16_t i = 0; i < count; ++i) setSection(section, i, color);
}

uint16_t LedService::sectionCount(LedSection section) const {
    switch (section) {
        case LedSection::Right: return hw::RightCount;
        case LedSection::Center: return hw::CenterCount;
        case LedSection::Left: return hw::LeftCount;
        case LedSection::Inside: return hw::InsideCount;
        case LedSection::Count:
        default: return 0;
    }
}

uint16_t LedService::sectionPhysicalIndex(LedSection section, uint16_t logical) const {
    const uint16_t count = sectionCount(section);
    if (!count) return 0;
    if (logical >= count) logical = count - 1U;
    return mappedSectionIndex(section, logical, frameMirror_);
}

RgbwColor LedService::decorativeHsv(LedCategory category, uint8_t hue,
                                    uint8_t saturation, uint8_t value) const {
    const int16_t degrees = settingsService().settings().ledColorRemixDegrees[static_cast<uint8_t>(category)];
    const int16_t shift = static_cast<int16_t>(degrees * 256L / 360L);
    return hsv(static_cast<uint8_t>(hue + shift), saturation, value);
}

}
