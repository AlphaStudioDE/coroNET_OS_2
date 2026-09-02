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
        case LedCategory::Error: renderError(animation, context); break;
        case LedCategory::Finish: renderFinish(animation, context); break;
        case LedCategory::Other: renderOther(animation, context.nowMs); break;
        case LedCategory::Idle:
        default: renderIdle(animation, context.nowMs); break;
    }
}

void LedService::renderIdle(uint8_t animation, uint32_t now) {
    switch (static_cast<IdleAnimation>(animation)) {
        case IdleAnimation::Rainbow: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t hue = static_cast<uint8_t>(now / 34U +
                    path * 256U / hw::OuterCount);
                const uint8_t value = static_cast<uint8_t>(105U +
                    wave8(static_cast<uint8_t>(now / 61U + path * 7U)) / 4U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 240U, value));
            }
            break;
        }
        case IdleAnimation::Fireplace: {
            const uint32_t tick = now / 58U;
            constexpr LedSection sides[2] = {LedSection::Left, LedSection::Right};
            for (uint8_t sideIndex = 0; sideIndex < 2U; ++sideIndex) {
                const LedSection section = sides[sideIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t height = static_cast<uint8_t>(
                        i * 255U / max<uint16_t>(1U, count - 1U));
                    const uint8_t flameA = wave8(static_cast<uint8_t>(tick * 5U +
                        i * 29U + sideIndex * 83U));
                    const uint8_t flameB = hash8(tick * 37U + i * 97U + sideIndex * 503U);
                    const uint8_t heat = static_cast<uint8_t>(max<int>(12,
                        230 - height * 145 / 255 + flameA / 4 + flameB / 7));
                    const uint8_t hue = static_cast<uint8_t>(2U + heat * 34U / 255U);
                    setSection(section, i,
                        decorativeHsv(LedCategory::Idle, hue, 255U, heat));
                }
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t ember = wave8(static_cast<uint8_t>(now / 37U + i * 19U));
                const uint8_t value = static_cast<uint8_t>(35U + ember * 75U / 255U);
                setSection(LedSection::Center, i,
                    decorativeHsv(LedCategory::Idle,
                        static_cast<uint8_t>(12U + ember / 18U), 255U, value));
            }
            break;
        }
        case IdleAnimation::Ocean: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t swell = wave8(static_cast<uint8_t>(now / 42U - path * 11U));
                const uint8_t ripple = wave8(static_cast<uint8_t>(now / 19U + path * 23U));
                const uint8_t mix = static_cast<uint8_t>(
                    (static_cast<uint16_t>(swell) * 3U + ripple) / 4U);
                const uint8_t hue = static_cast<uint8_t>(132U + mix * 27U / 255U);
                const uint8_t value = static_cast<uint8_t>(42U + mix * 130U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 205U, value));
            }
            break;
        }
        case IdleAnimation::StarPulse: {
            const uint8_t pulse = wave8(static_cast<uint8_t>(now / 54U));
            const uint8_t hue = static_cast<uint8_t>((now / 3200U) * 29U);
            const uint16_t center = (hw::OuterCount - 1U) / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t shape = static_cast<uint8_t>(max<int>(85,
                    255 - static_cast<int>(distance) * 7));
                const uint8_t value = static_cast<uint8_t>((45U + pulse * 115U / 255U) *
                    static_cast<uint16_t>(shape) / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 205U, value));
            }
            break;
        }
        case IdleAnimation::Meteor: {
            const uint16_t route = hw::OuterCount + 12U;
            const uint16_t rawHead = static_cast<uint16_t>((now / 46U) % route);
            if (rawHead < hw::OuterCount) {
                const uint8_t hue = static_cast<uint8_t>((now / (route * 46U)) * 43U);
                for (uint8_t tail = 0; tail < 11U; ++tail) {
                    if (rawHead < tail) continue;
                    const uint16_t path = rawHead - tail;
                    const uint8_t value = static_cast<uint8_t>(245U - tail * 21U);
                    setOuterVisualPathPixel(path,
                        decorativeHsv(LedCategory::Idle, hue, 235U, value));
                }
            }
            break;
        }
        case IdleAnimation::Twinkle: {
            const uint32_t frame = now / 110U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t slot = static_cast<uint8_t>((frame + path * 5U) & 31U);
                const uint8_t seed = hash8((frame - slot) * 131U + path * 89U);
                if (seed < 214U || slot > 12U) continue;
                const uint8_t value = static_cast<uint8_t>(220U - slot * 15U);
                const uint8_t hue = static_cast<uint8_t>(20U + seed / 9U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 55U, value));
            }
            break;
        }
        case IdleAnimation::Larson: {
            const uint16_t span = hw::OuterCount - 1U;
            const uint16_t phase = static_cast<uint16_t>((now / 27U) % (span * 2U));
            const uint16_t head = phase <= span ? phase : span * 2U - phase;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > head ? path - head : head - path;
                if (distance > 8U) continue;
                const uint8_t value = static_cast<uint8_t>(245U - distance * 27U);
                setOuterVisualPathPixel(path, RgbwColor(value, 0U, 0U));
            }
            break;
        }
        case IdleAnimation::Lava: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t flowA = wave8(static_cast<uint8_t>(now / 67U + path * 17U));
                const uint8_t flowB = wave8(static_cast<uint8_t>(now / 103U - path * 11U + 49U));
                const uint8_t molten = static_cast<uint8_t>(
                    (static_cast<uint16_t>(flowA) + flowB) / 2U);
                const uint8_t hue = static_cast<uint8_t>(248U + molten * 42U / 255U);
                const uint8_t value = static_cast<uint8_t>(35U + molten * 155U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 255U, value));
            }
            break;
        }
        case IdleAnimation::Gradient: {
            const uint8_t drift = static_cast<uint8_t>(now / 82U);
            const uint8_t hueA = static_cast<uint8_t>(drift / 3U);
            const uint8_t hueB = static_cast<uint8_t>(hueA + 92U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t amount = static_cast<uint8_t>(
                    path * 255U / max<uint16_t>(1U, hw::OuterCount - 1U));
                const RgbwColor first = decorativeHsv(LedCategory::Idle, hueA, 235U, 145U);
                const RgbwColor second = decorativeHsv(LedCategory::Idle, hueB, 235U, 145U);
                setOuterVisualPathPixel(path, blend(first, second, amount));
            }
            break;
        }
        case IdleAnimation::Plasma: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t fieldA = wave8(static_cast<uint8_t>(now / 24U + path * 15U));
                const uint8_t fieldB = wave8(static_cast<uint8_t>(now / 15U - path * 9U + 61U));
                const uint8_t interference = static_cast<uint8_t>(
                    (static_cast<uint16_t>(fieldA) + fieldB) / 2U);
                const uint8_t hue = static_cast<uint8_t>(interference + now / 74U);
                const uint8_t value = static_cast<uint8_t>(62U +
                    max<uint8_t>(fieldA, fieldB) * 120U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 245U, value));
            }
            break;
        }
        case IdleAnimation::SectionBreathe: {
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint8_t breath = wave8(static_cast<uint8_t>(
                    now / 58U + sectionIndex * 85U));
                const uint8_t value = static_cast<uint8_t>(38U + breath * 125U / 255U);
                const uint8_t hue = static_cast<uint8_t>(24U + sectionIndex * 13U);
                fillSection(section,
                    decorativeHsv(LedCategory::Idle, hue, 225U, value));
            }
            break;
        }
        case IdleAnimation::Snow: {
            const RgbwColor winter = decorativeHsv(LedCategory::Idle, 158U, 75U, 15U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                fillSection(section, winter);
                for (uint8_t flake = 0; flake < 4U; ++flake) {
                    const uint16_t route = count + 5U;
                    const uint16_t position = static_cast<uint16_t>((now / (145U + flake * 23U) +
                        hash8(sectionIndex * 97U + flake * 61U)) % route);
                    if (position >= count) continue;
                    const uint8_t shimmer = static_cast<uint8_t>(175U +
                        hash8(flake * 43U + sectionIndex * 211U) / 3U);
                    setSection(section, position, RgbwColor(shimmer, shimmer, 255U));
                }
            }
            break;
        }
        case IdleAnimation::ColorWipe: {
            const uint32_t cycleLength = 2U * hw::OuterCount * 48U + 720U;
            const uint32_t cycle = now % cycleLength;
            const uint32_t travelLength = hw::OuterCount * 48U;
            const uint8_t hue = static_cast<uint8_t>((now / cycleLength) * 47U);
            uint16_t coverage = 0U;
            bool reverse = false;
            if (cycle < travelLength) {
                coverage = static_cast<uint16_t>(cycle * hw::OuterCount * 255U / travelLength);
            } else if (cycle < travelLength + 720U) {
                coverage = hw::OuterCount * 255U;
            } else {
                reverse = true;
                coverage = static_cast<uint16_t>((cycle - travelLength - 720U) *
                    hw::OuterCount * 255U / travelLength);
            }
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint32_t start = static_cast<uint32_t>(path) * 255U;
                uint8_t amount = coverage <= start ? 0U
                    : static_cast<uint8_t>(min<uint32_t>(255U, coverage - start));
                if (reverse) amount = static_cast<uint8_t>(255U - amount);
                if (amount) setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 245U, amount));
            }
            break;
        }
        case IdleAnimation::Moonlight: {
            const uint16_t moon = static_cast<uint16_t>((now / 135U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > moon ? path - moon : moon - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint8_t halo = distance < 9U
                    ? static_cast<uint8_t>(150U - distance * 11U) : 44U;
                const uint8_t ripple = wave8(static_cast<uint8_t>(now / 79U + path * 5U));
                const uint8_t value = static_cast<uint8_t>(halo + ripple / 10U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, 164U, 72U, value));
            }
            break;
        }
        case IdleAnimation::Tetris: {
            const uint32_t tick = now / 155U;
            for (uint8_t block = 0; block < 7U; ++block) {
                const uint16_t route = hw::OuterCount + 11U;
                const uint16_t head = static_cast<uint16_t>((tick * (1U + block % 2U) +
                    hash8(block * 91U)) % route);
                if (head >= hw::OuterCount) continue;
                const uint8_t length = static_cast<uint8_t>(2U + block % 3U);
                const uint8_t hue = static_cast<uint8_t>(block * 39U +
                    (tick / route) * 17U);
                for (uint8_t part = 0; part < length; ++part) {
                    if (head < part) continue;
                    setOuterVisualPathPixel(head - part,
                        decorativeHsv(LedCategory::Idle, hue, 245U,
                            part == 0U ? 205U : 145U));
                }
            }
            break;
        }
        case IdleAnimation::Running: {
            const uint16_t shift = static_cast<uint16_t>(now / 72U);
            const uint8_t hue = static_cast<uint8_t>(now / 42U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t lane = static_cast<uint8_t>((path + shift) % 6U);
                const uint8_t value = lane < 2U ? 195U : lane == 2U ? 75U : 12U;
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 240U, value));
            }
            break;
        }
        case IdleAnimation::Bubbles: {
            fillSection(LedSection::Center,
                decorativeHsv(LedCategory::Idle, 150U, 180U, 14U));
            constexpr LedSection sides[2] = {LedSection::Left, LedSection::Right};
            for (uint8_t sideIndex = 0; sideIndex < 2U; ++sideIndex) {
                const LedSection section = sides[sideIndex];
                const uint16_t count = sectionCount(section);
                for (uint8_t bubble = 0; bubble < 5U; ++bubble) {
                    const uint16_t route = count + 4U;
                    const uint16_t position = static_cast<uint16_t>((now / (112U + bubble * 17U) +
                        bubble * 3U + sideIndex * 2U) % route);
                    if (position < count) {
                        const uint8_t hue = static_cast<uint8_t>(143U + bubble * 15U);
                        setSection(section, position,
                            decorativeHsv(LedCategory::Idle, hue, 180U, 175U));
                    } else if (position == count) {
                        const uint16_t pop = sideIndex == 0U ? bubble * 4U % hw::CenterCount
                            : hw::CenterCount - 1U - bubble * 4U % hw::CenterCount;
                        setSection(LedSection::Center, pop, RgbwColor(180U, 225U, 255U));
                    }
                }
            }
            break;
        }
        case IdleAnimation::Drift: {
            const uint8_t base = static_cast<uint8_t>(now / 105U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t ribbon = wave8(static_cast<uint8_t>(now / 91U + path * 8U));
                const uint8_t hue = static_cast<uint8_t>(base + path * 4U + ribbon / 9U);
                const uint8_t value = static_cast<uint8_t>(55U + ribbon * 95U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 195U, value));
            }
            break;
        }
        case IdleAnimation::Candle: {
            const uint32_t tick = now / 62U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t slow = wave8(static_cast<uint8_t>(now / 83U + path * 7U));
                const uint8_t noise = hash8(tick * 73U + path * 101U);
                const uint8_t value = static_cast<uint8_t>(75U + slow / 5U + noise / 4U);
                const uint8_t hue = static_cast<uint8_t>(17U + noise / 24U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 250U, value));
            }
            break;
        }
        case IdleAnimation::Starfield: {
            const uint8_t drift = static_cast<uint8_t>(now / 170U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t identity = hash8(path * 127U + 29U);
                const uint8_t twinkle = wave8(static_cast<uint8_t>(
                    now / (56U + identity % 59U) + identity + drift));
                const uint8_t floor = static_cast<uint8_t>(identity / 12U);
                const uint8_t value = static_cast<uint8_t>(floor + twinkle *
                    (65U + identity / 2U) / 255U);
                const uint8_t hue = static_cast<uint8_t>(150U + identity / 8U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Idle, hue, 48U, value));
            }
            break;
        }
        case IdleAnimation::Count:
        default:
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
    auto sectionMeter = [&](LedSection section, uint8_t percent, const RgbwColor& color,
                            bool reverse = false) {
        const uint16_t count = sectionCount(section);
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t meterIndex = reverse ? count - 1U - i : i;
            const uint8_t coverage = progressCoverage(percent, count, meterIndex);
            setSection(section, i, scaled(color, coverage ? coverage : 5U));
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
        case PauseAnimation::AmberStrobe: {
            const uint32_t cycle = now % 2400U;
            const bool flash = cycle < 65U || (cycle >= 120U && cycle < 185U) ||
                               (cycle >= 240U && cycle < 305U);
            const uint16_t origin = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(min<uint32_t>(hw::OuterCount,
                cycle * hw::OuterCount / 620U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > origin ? path - origin : origin - path;
                uint8_t value = 4U;
                if (flash && distance <= radius && radius - distance < 5U) {
                    value = static_cast<uint8_t>(235U - (radius - distance) * 42U);
                }
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(amber, flash ? 255U : 54U));
            break;
        }
        case PauseAnimation::Zigzag: {
            const uint8_t step = static_cast<uint8_t>((now / 210U) % 6U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const bool high = ((i + sectionIndex * 2U + step) % 6U) < 2U;
                    const bool reverse = (sectionIndex & 1U) != 0U;
                    const uint16_t logical = reverse ? count - 1U - i : i;
                    setSection(section, logical, scaled(amber, high ? 175U : 12U));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 225U));
            break;
        }
        case PauseAnimation::Neon: {
            const uint32_t cycle = now % 5200U;
            const uint8_t warmValue = cycle < 130U || (cycle >= 210U && cycle < 280U)
                ? 230U : static_cast<uint8_t>(74U + wave8(now / 55U) / 8U);
            const RgbwColor magenta = decorativeHsv(LedCategory::Pause, 224U, 235U, 255U);
            const RgbwColor cyan = decorativeHsv(LedCategory::Pause, 132U, 235U, 255U);
            fillSection(LedSection::Left, scaled(magenta, warmValue));
            fillSection(LedSection::Right, scaled(cyan, warmValue));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool tube = ((i + 1U) % 4U) < 2U;
                const RgbwColor color = tube ? magenta : cyan;
                const uint8_t value = i < lit ? warmValue : 10U;
                setSection(LedSection::Center, i, scaled(color, value));
            }
            setSection(LedSection::Center, marker, scaled(amber, 245U));
            break;
        }
        case PauseAnimation::Hourglass: {
            const uint32_t cycle = now % 6000U;
            const uint8_t phase = static_cast<uint8_t>(cycle * 255U / 6000U);
            const uint16_t grains = static_cast<uint16_t>((255U - phase) * hw::LeftCount / 255U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t leftValue = i >= hw::LeftCount - grains ? 150U : 7U;
                const uint8_t rightValue = i < hw::RightCount - grains ? 150U : 7U;
                setSection(LedSection::Left, i, scaled(amber, leftValue));
                setSection(LedSection::Right, i, scaled(amber, rightValue));
            }
            frozenProgress(filament, 44U, 2U, 145U);
            const uint16_t falling = static_cast<uint16_t>((cycle / 95U) % hw::CenterCount);
            setSection(LedSection::Center, falling, scaled(amber, 210U));
            break;
        }
        case PauseAnimation::AmberWave: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t first = wave8(static_cast<uint8_t>(now / 29U - path * 17U));
                const uint8_t second = wave8(static_cast<uint8_t>(now / 47U + path * 9U));
                const uint8_t value = static_cast<uint8_t>(8U +
                    static_cast<uint16_t>(first) * first / 640U + second / 9U);
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 180U));
            break;
        }
        case PauseAnimation::Bounce: {
            const uint32_t cycle = now % 3200U;
            const uint32_t half = cycle < 1600U ? cycle : 3200U - cycle;
            const uint16_t head = static_cast<uint16_t>(half * (hw::OuterCount - 1U) / 1600U);
            for (int8_t offset = -4; offset <= 4; ++offset) {
                const int16_t path = static_cast<int16_t>(head) + offset;
                if (path < 0 || path >= static_cast<int16_t>(hw::OuterCount)) continue;
                const uint8_t value = static_cast<uint8_t>(230U - abs(offset) * 47U);
                setOuterVisualPathPixel(static_cast<uint16_t>(path), scaled(cool, value));
            }
            frozenProgress(amber, 30U, 2U, 150U);
            break;
        }
        case PauseAnimation::SlowComet: {
            const uint16_t head = static_cast<uint16_t>((now / 135U) % hw::OuterCount);
            for (uint8_t tail = 0; tail < 14U; ++tail) {
                const uint16_t path = static_cast<uint16_t>((head + hw::OuterCount - tail) % hw::OuterCount);
                const uint8_t value = static_cast<uint8_t>(220U - tail * 15U);
                setOuterVisualPathPixel(path, scaled(filament, value));
            }
            frozenProgress(amber, 24U, 2U, 125U);
            break;
        }
        case PauseAnimation::Spinner: {
            const uint16_t head = static_cast<uint16_t>((now / 52U) % hw::OuterCount);
            const uint16_t opposite = static_cast<uint16_t>((head + hw::OuterCount / 2U) % hw::OuterCount);
            for (uint8_t tail = 0; tail < 7U; ++tail) {
                const uint8_t value = static_cast<uint8_t>(220U - tail * 31U);
                setOuterVisualPathPixel((head + hw::OuterCount - tail) % hw::OuterCount,
                                        scaled(amber, value));
                setOuterVisualPathPixel((opposite + tail) % hw::OuterCount,
                                        scaled(cool, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 150U));
            break;
        }
        case PauseAnimation::MorseWait: {
            static constexpr uint8_t Pattern[] = {
                1U, 3U, 3U, 0U, 1U, 3U, 0U, 1U, 1U, 0U, 3U,
            };
            const uint16_t tick = static_cast<uint16_t>((now / 170U) % 38U);
            uint16_t cursor = 0U;
            int8_t active = -1;
            for (uint8_t symbol = 0; symbol < sizeof(Pattern); ++symbol) {
                const uint8_t units = Pattern[symbol] ? Pattern[symbol] : 2U;
                if (Pattern[symbol] && tick >= cursor && tick < cursor + units) active = symbol;
                cursor += units + (Pattern[symbol] ? 1U : 0U);
            }
            fillSection(LedSection::Left, scaled(amber, active >= 0 ? 105U : 12U));
            fillSection(LedSection::Right, scaled(amber, active >= 0 ? 105U : 12U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t symbol = static_cast<uint8_t>(i * sizeof(Pattern) / hw::CenterCount);
                uint8_t value = 4U;
                if (Pattern[symbol] == 0U) value = 0U;
                else if (active == symbol) value = 225U;
                else value = Pattern[symbol] == 3U ? 55U : 28U;
                setSection(LedSection::Center, i, scaled(amber, value));
            }
            break;
        }
        case PauseAnimation::BlueBreathe: {
            const uint8_t breath = wave8(now / 54U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t edgeDistance = min<uint16_t>(i, count - 1U - i);
                    const uint8_t local = wave8(static_cast<uint8_t>(now / 54U + edgeDistance * 15U));
                    const uint8_t value = static_cast<uint8_t>(12U +
                        static_cast<uint16_t>(breath + local) * 72U / 510U);
                    setSection(section, i, scaled(cool, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 145U));
            break;
        }
        case PauseAnimation::SoftHold: {
            const uint8_t breath = static_cast<uint8_t>(36U + wave8(now / 73U) / 7U);
            fillSection(LedSection::Left, scaled(filament, breath));
            fillSection(LedSection::Right, scaled(filament, breath));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > marker ? i - marker : marker - i;
                const uint8_t value = distance < 5U
                    ? static_cast<uint8_t>(95U - distance * 17U + wave8(now / 67U + distance * 23U) / 10U)
                    : (i < lit ? 20U : 3U);
                setSection(LedSection::Center, i, scaled(amber, value));
            }
            break;
        }
        case PauseAnimation::AmberTheater: {
            const uint8_t step = static_cast<uint8_t>((now / 145U) % 6U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t inward = i <= (count - 1U) / 2U ? i : count - 1U - i;
                    const bool lamp = ((inward + step) % 6U) < 2U;
                    setSection(section, i, scaled(amber, lamp ? 190U : 8U));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 230U));
            break;
        }
        case PauseAnimation::BreathingDots: {
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    uint8_t value = 2U;
                    if ((i + sectionIndex) % 4U == 0U) {
                        value = static_cast<uint8_t>(20U + wave8(now / 49U + i * 29U + sectionIndex * 61U) / 2U);
                    }
                    setSection(section, i, scaled(amber, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 145U));
            break;
        }
        case PauseAnimation::WaitingRipple: {
            const uint16_t origin = static_cast<uint16_t>(hw::LeftCount + marker);
            const uint16_t radius = static_cast<uint16_t>((now / 92U) % (hw::OuterCount / 2U + 7U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > origin ? path - origin : origin - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                const uint8_t value = delta < 4U ? static_cast<uint8_t>(185U - delta * 43U) : 4U;
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 210U));
            break;
        }
        case PauseAnimation::ParkingLights: {
            const uint32_t cycle = now % 3000U;
            const uint8_t signal = cycle < 110U || (cycle >= 230U && cycle < 340U) ? 225U : 48U;
            fillSection(LedSection::Left, scaled(amber, 8U));
            fillSection(LedSection::Right, scaled(amber, 8U));
            setSection(LedSection::Left, 0U, scaled(amber, signal));
            setSection(LedSection::Left, hw::LeftCount - 1U, scaled(amber, signal));
            setSection(LedSection::Right, 0U, scaled(amber, signal));
            setSection(LedSection::Right, hw::RightCount - 1U, scaled(amber, signal));
            frozenProgress(filament, 35U, 2U, 115U);
            break;
        }
        case PauseAnimation::DimSparks: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t seed = hash8(path * 127U + now / 125U);
                uint8_t value = 3U;
                if (seed > 244U) value = static_cast<uint8_t>(45U + (seed - 244U) * 10U);
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 115U));
            break;
        }
        case PauseAnimation::SlowScan: {
            const uint32_t cycle = now % 6200U;
            const uint32_t half = cycle < 3100U ? cycle : 6200U - cycle;
            const uint16_t scan = static_cast<uint16_t>(half * (hw::CenterCount - 1U) / 3100U);
            fillSection(LedSection::Left, scaled(filament, 32U));
            fillSection(LedSection::Right, scaled(filament, 32U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > scan ? i - scan : scan - i;
                uint8_t value = i < lit ? 26U : 3U;
                if (distance < 5U) value = static_cast<uint8_t>(195U - distance * 38U);
                setSection(LedSection::Center, i, scaled(cool, value));
            }
            setSection(LedSection::Center, marker, scaled(amber, 150U));
            break;
        }
        case PauseAnimation::FrozenGold: {
            const RgbwColor gold = decorativeHsv(LedCategory::Pause, 31U, 220U, 255U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t facet = hash8(i * 73U + sectionIndex * 113U);
                    const uint8_t value = static_cast<uint8_t>(18U + facet / 5U);
                    setSection(section, i, scaled(gold, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(RgbwColor(255U, 238U, 170U), 205U));
            break;
        }
        case PauseAnimation::ClockTick: {
            const uint16_t second = static_cast<uint16_t>((now / 1000U) % hw::CenterCount);
            const uint8_t tickPulse = static_cast<uint8_t>(255U - min<uint32_t>(220U, (now % 1000U) * 220U / 1000U));
            fillSection(LedSection::Left, scaled(amber, 10U));
            fillSection(LedSection::Right, scaled(amber, 10U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool quarter = i % 5U == 0U;
                const uint8_t value = i == second ? tickPulse : (quarter ? 36U : 4U);
                setSection(LedSection::Center, i, scaled(amber, value));
            }
            setSection(LedSection::Left, progress / 10U, scaled(filament, 120U));
            setSection(LedSection::Right, progress / 10U, scaled(filament, 120U));
            break;
        }
        case PauseAnimation::CalmOrbit: {
            const uint16_t first = static_cast<uint16_t>((now / 105U) % hw::OuterCount);
            const uint16_t second = static_cast<uint16_t>((first + hw::OuterCount / 2U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t directA = path > first ? path - first : first - path;
                const uint16_t directB = path > second ? path - second : second - path;
                const uint16_t distanceA = min<uint16_t>(directA, hw::OuterCount - directA);
                const uint16_t distanceB = min<uint16_t>(directB, hw::OuterCount - directB);
                const uint8_t valueA = distanceA < 4U ? static_cast<uint8_t>(135U - distanceA * 31U) : 0U;
                const uint8_t valueB = distanceB < 4U ? static_cast<uint8_t>(120U - distanceB * 27U) : 0U;
                const RgbwColor color = blend(scaled(amber, valueA), scaled(cool, valueB), 128U);
                setOuterVisualPathPixel(path, color);
            }
            setSection(LedSection::Center, marker, scaled(filament, 105U));
            break;
        }
        case PauseAnimation::HoldingPattern: {
            frozenProgress(amber, 72U, 3U, 175U);
            const uint16_t shuttle = static_cast<uint16_t>((now / 155U) % (hw::LeftCount * 2U - 2U));
            const uint16_t sidePos = shuttle < hw::LeftCount ? shuttle : hw::LeftCount * 2U - 2U - shuttle;
            fillSection(LedSection::Left, scaled(filament, 12U));
            fillSection(LedSection::Right, scaled(filament, 12U));
            for (int8_t offset = -2; offset <= 2; ++offset) {
                const int16_t position = static_cast<int16_t>(sidePos) + offset;
                if (position < 0 || position >= static_cast<int16_t>(hw::LeftCount)) continue;
                const uint8_t value = static_cast<uint8_t>(170U - abs(offset) * 58U);
                setSection(LedSection::Left, static_cast<uint16_t>(position), scaled(amber, value));
                setSection(LedSection::Right, hw::RightCount - 1U - static_cast<uint16_t>(position), scaled(amber, value));
            }
            break;
        }
        case PauseAnimation::BreathingAmber: {
            const uint8_t breath = static_cast<uint8_t>(26U + wave8(now / 48U) * 144U / 255U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t local = wave8(static_cast<uint8_t>(now / 48U + i * 6U));
                    const uint8_t value = static_cast<uint8_t>(breath * (190U + local / 4U) / 255U);
                    setSection(section, i, scaled(amber, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, max<uint8_t>(115U, breath)));
            break;
        }
        case PauseAnimation::ResumeGate: {
            const uint8_t opening = wave8(now / 42U);
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t span = static_cast<uint16_t>(opening * (hw::OuterCount / 2U) / 255U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint16_t edgeDistance = distance > span ? distance - span : span - distance;
                const uint8_t value = edgeDistance < 5U
                    ? static_cast<uint8_t>(205U - edgeDistance * 38U)
                    : (distance < span ? 20U : 3U);
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(filament, 200U));
            break;
        }
        case PauseAnimation::TempKeepalive: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 45U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            sectionMeter(LedSection::Left, toolPercent, temperatureColor(toolPercent));
            sectionMeter(LedSection::Right, chamberPercent, temperatureColor(chamberPercent), true);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t bedCoverage = progressCoverage(bedPercent, hw::CenterCount, i);
                const uint8_t printCoverage = progressCoverage(progress, hw::CenterCount, i);
                RgbwColor color = scaled(temperatureColor(bedPercent), bedCoverage ? 70U : 4U);
                if (printCoverage) color = blend(color, filament, static_cast<uint8_t>(printCoverage * 3U / 5U));
                setSection(LedSection::Center, i, color);
            }
            setSection(LedSection::Center, marker, scaled(amber, 225U));
            break;
        }
        case PauseAnimation::SoftAttention: {
            const uint32_t cycle = now % 5200U;
            uint8_t attention = 20U;
            if (cycle < 650U) attention = static_cast<uint8_t>(20U + wave8(static_cast<uint8_t>(cycle * 255U / 650U)) * 155U / 255U);
            fillSection(LedSection::Left, scaled(amber, attention));
            fillSection(LedSection::Right, scaled(amber, attention));
            frozenProgress(filament, 34U, 2U, static_cast<uint8_t>(80U + attention / 2U));
            break;
        }
        case PauseAnimation::OperatorWait: {
            const uint32_t cycle = now % 2600U;
            const bool prompt = cycle < 650U || (cycle >= 900U && cycle < 1550U);
            fillSection(LedSection::Left, scaled(amber, prompt ? 84U : 16U));
            fillSection(LedSection::Right, scaled(amber, prompt ? 84U : 16U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool bracket = i < 3U || i >= hw::CenterCount - 3U;
                const bool progressPixel = i < lit;
                const uint8_t value = bracket ? (prompt ? 205U : 48U) : (progressPixel ? 42U : 3U);
                setSection(LedSection::Center, i, scaled(bracket ? amber : filament, value));
            }
            break;
        }
        case PauseAnimation::FrozenLayer: {
            fillSection(LedSection::Left, scaled(cool, 12U));
            fillSection(LedSection::Right, scaled(cool, 12U));
            for (uint16_t i = 0; i < hw::LeftCount; i += 3U) {
                const uint8_t shimmer = static_cast<uint8_t>(35U + wave8(now / 82U + i * 21U) / 5U);
                setSection(LedSection::Left, i, scaled(cool, shimmer));
                setSection(LedSection::Right, hw::RightCount - 1U - i, scaled(cool, shimmer));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const uint8_t facet = hash8(i * 91U);
                const uint8_t value = coverage
                    ? static_cast<uint8_t>(48U + facet / 8U)
                    : 3U;
                setSection(LedSection::Center, i, scaled(cool, value));
            }
            setSection(LedSection::Center, marker, scaled(RgbwColor(225U, 248U, 255U), 215U));
            break;
        }
        case PauseAnimation::FilamentHold: {
            frozenProgress(filament, 70U, 2U, 205U);
            const uint16_t head = static_cast<uint16_t>((now / 125U) % hw::OuterCount);
            for (uint8_t tail = 0; tail < 8U; ++tail) {
                const uint16_t path = (head + hw::OuterCount - tail) % hw::OuterCount;
                setOuterVisualPathPixel(path, scaled(filament, static_cast<uint8_t>(210U - tail * 24U)));
            }
            setSection(LedSection::Center, marker, scaled(amber, 205U));
            break;
        }
        case PauseAnimation::DoNotTouch: {
            const uint32_t cycle = now % 2100U;
            const bool warning = cycle < 115U || (cycle >= 235U && cycle < 350U);
            const RgbwColor warningColor = decorativeHsv(LedCategory::Pause, 5U, 255U, 255U);
            fillSection(LedSection::Left, scaled(warningColor, warning ? 240U : 24U));
            fillSection(LedSection::Right, scaled(warningColor, warning ? 240U : 24U));
            frozenProgress(amber, 50U, 3U, warning ? 230U : 105U);
            break;
        }
        case PauseAnimation::HeatHoldSplit: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 68U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 35U);
            const RgbwColor toolColor = temperatureColor(toolPercent);
            const RgbwColor chamberColor = temperatureColor(chamberPercent);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (hw::LeftCount - 1U));
                setSection(LedSection::Left, i, scaled(blend(chamberColor, toolColor, amount), 105U));
                setSection(LedSection::Right, i, scaled(blend(toolColor, chamberColor, amount), 105U));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (hw::CenterCount - 1U));
                const RgbwColor heat = blend(chamberColor, toolColor, amount);
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                setSection(LedSection::Center, i, scaled(heat, coverage ? 130U : 24U));
            }
            setSection(LedSection::Center, marker, scaled(filament, 220U));
            break;
        }
        case PauseAnimation::CalmDown: {
            const uint8_t level = static_cast<uint8_t>(33U + wave8(now / 28U) * 112U / 255U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t spatial = distance * 4U < level ? static_cast<uint8_t>(level - distance * 4U) : 5U;
                setOuterVisualPathPixel(path, scaled(amber, spatial));
            }
            setSection(LedSection::Center, marker, scaled(filament, static_cast<uint8_t>(70U + level / 2U)));
            break;
        }
        case PauseAnimation::StillWater: {
            const RgbwColor water = decorativeHsv(LedCategory::Pause, 139U, 210U, 255U);
            const uint16_t origin = static_cast<uint16_t>(hw::LeftCount + marker);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > origin ? path - origin : origin - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint8_t rippleA = wave8(static_cast<uint8_t>(distance * 19U - now / 61U));
                const uint8_t rippleB = wave8(static_cast<uint8_t>(distance * 11U - now / 97U));
                const uint8_t value = static_cast<uint8_t>(9U + rippleA / 8U + rippleB / 12U);
                setOuterVisualPathPixel(path, scaled(water, value));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                if (coverage) setSection(LedSection::Center, i, scaled(water, static_cast<uint8_t>(45U + coverage / 5U)));
            }
            setSection(LedSection::Center, marker, scaled(RgbwColor(185U, 240U, 255U), 180U));
            break;
        }
        case PauseAnimation::SoftLantern: {
            const RgbwColor lantern = decorativeHsv(LedCategory::Pause, 21U, 205U, 255U);
            const uint8_t flame = static_cast<uint8_t>(105U + wave8(now / 59U) / 5U + hash8(now / 180U) / 12U);
            const uint16_t center = hw::CenterCount / 2U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > center ? i - center : center - i;
                const uint8_t value = flame > distance * 13U ? static_cast<uint8_t>(flame - distance * 13U) : 4U;
                setSection(LedSection::Center, i, scaled(lantern, value));
            }
            fillSection(LedSection::Left, scaled(lantern, 20U));
            fillSection(LedSection::Right, scaled(lantern, 20U));
            setSection(LedSection::Center, marker, scaled(filament, 150U));
            break;
        }
        case PauseAnimation::HoldOrb: {
            const uint8_t pulse = static_cast<uint8_t>(145U + wave8(now / 52U) / 4U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > marker ? i - marker : marker - i;
                const uint8_t value = distance < 7U
                    ? static_cast<uint8_t>(pulse > distance * 23U ? pulse - distance * 23U : 0U)
                    : 2U;
                setSection(LedSection::Center, i, scaled(filament, value));
            }
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint16_t edgeDistance = hw::LeftCount - 1U - i;
                const uint8_t value = edgeDistance < 4U ? static_cast<uint8_t>(55U - edgeDistance * 13U) : 3U;
                setSection(LedSection::Left, i, scaled(amber, value));
                setSection(LedSection::Right, hw::RightCount - 1U - i, scaled(amber, value));
            }
            break;
        }
        case PauseAnimation::SuspendedLayer: {
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(progress, hw::CenterCount, i);
                const bool layerLine = i % 3U == 0U;
                const uint8_t value = coverage ? (layerLine ? 105U : 34U) : 3U;
                setSection(LedSection::Center, i, scaled(filament, value));
            }
            const uint8_t drift = static_cast<uint8_t>((now / 330U) % 4U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const bool suspended = (i + drift) % 4U == 0U;
                const uint8_t value = suspended ? 82U : 7U;
                setSection(LedSection::Left, i, scaled(amber, value));
                setSection(LedSection::Right, i, scaled(amber, value));
            }
            setSection(LedSection::Center, marker, scaled(amber, 205U));
            break;
        }
        case PauseAnimation::GentleReminder: {
            const uint8_t second = static_cast<uint8_t>((now / 1000U) % 10U);
            fillSection(LedSection::Left, scaled(amber, 16U));
            fillSection(LedSection::Right, scaled(amber, 16U));
            frozenProgress(filament, 28U, 2U, 90U);
            for (uint8_t dot = 0; dot < 10U; ++dot) {
                const uint16_t position = static_cast<uint16_t>(dot * (hw::CenterCount - 1U) / 9U);
                const uint8_t value = dot <= second ? 130U : 15U;
                setSection(LedSection::Center, position, scaled(amber, value));
            }
            if ((now % 10000U) < 320U) {
                fillSection(LedSection::Left, scaled(amber, 105U));
                fillSection(LedSection::Right, scaled(amber, 105U));
            }
            break;
        }
        case PauseAnimation::BreathGate: {
            const uint8_t breath = wave8(now / 58U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                const uint16_t gate = static_cast<uint16_t>(breath * (count / 2U) / 255U);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t edge = min<uint16_t>(i, count - 1U - i);
                    const uint16_t distance = edge > gate ? edge - gate : gate - edge;
                    const uint8_t value = distance < 3U ? static_cast<uint8_t>(155U - distance * 46U) : 5U;
                    setSection(section, i, scaled(amber, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 180U));
            break;
        }
        case PauseAnimation::WaitingRoom: {
            const uint8_t occupied = static_cast<uint8_t>((now / 900U) % 8U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const bool seat = i % 3U == 1U;
                    const uint8_t seatNumber = static_cast<uint8_t>((i / 3U + sectionIndex * 3U) % 8U);
                    const uint8_t value = !seat ? 2U : (seatNumber == occupied ? 145U : 28U);
                    setSection(section, i, scaled(amber, value));
                }
            }
            setSection(LedSection::Center, marker, scaled(filament, 115U));
            break;
        }
        case PauseAnimation::ToolPark: {
            frozenProgress(amber, 30U, 2U, 110U);
            const bool parkLeft = context.activeTool < 2U;
            const LedSection parkSection = parkLeft ? LedSection::Left : LedSection::Right;
            const LedSection quietSection = parkLeft ? LedSection::Right : LedSection::Left;
            const uint16_t count = sectionCount(parkSection);
            const uint16_t anchor = (context.activeTool & 1U) ? count - 2U : 1U;
            fillSection(quietSection, scaled(filament, 8U));
            fillSection(parkSection, scaled(filament, 14U));
            const uint8_t blink = static_cast<uint8_t>(125U + wave8(now / 38U) / 2U);
            for (int8_t offset = -1; offset <= 1; ++offset) {
                const int16_t position = static_cast<int16_t>(anchor) + offset;
                if (position < 0 || position >= static_cast<int16_t>(count)) continue;
                setSection(parkSection, static_cast<uint16_t>(position),
                           scaled(filament, offset == 0 ? blink : blink / 3U));
            }
            break;
        }
        case PauseAnimation::ResumeRamp: {
            const uint32_t cycle = now % 4600U;
            const uint8_t ramp = cycle < 3000U
                ? static_cast<uint8_t>(cycle * 255U / 3000U)
                : cycle < 3900U ? 255U
                : static_cast<uint8_t>((4600U - cycle) * 255U / 700U);
            const RgbwColor ready = decorativeHsv(LedCategory::Pause, 92U, 240U, 255U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t threshold = static_cast<uint8_t>(path * 255U / (hw::OuterCount - 1U));
                uint8_t value = 4U;
                if (ramp >= threshold) value = static_cast<uint8_t>(60U + (ramp - threshold) / 2U);
                setOuterVisualPathPixel(path, scaled(ready, value));
            }
            if (cycle >= 3000U && cycle < 3900U) {
                const uint8_t settle = static_cast<uint8_t>(70U + wave8(now / 46U) / 4U);
                fillSection(LedSection::Left, scaled(ready, settle));
                fillSection(LedSection::Right, scaled(ready, settle));
            }
            setSection(LedSection::Center, marker, scaled(filament, 210U));
            break;
        }
        case PauseAnimation::Count:
        default:
            break;
    }
}

void LedService::renderError(uint8_t animation, const LedAnimationContext& context) {
    const uint32_t now = context.nowMs;
    const RgbwColor red = decorativeHsv(LedCategory::Error, 0U, 255U, 255U);
    const RgbwColor amber = decorativeHsv(LedCategory::Error, 22U, 255U, 255U);
    const RgbwColor blue = decorativeHsv(LedCategory::Error, 166U, 255U, 255U);
    const RgbwColor green = decorativeHsv(LedCategory::Error, 92U, 235U, 255U);
    const bool networkFault = !context.printerOnline || context.printerTelemetryAgeMs > 15000U;
    const bool thermalFault = context.ventFailsafe ||
        (!isnan(context.chamberTempC) && context.chamberTempC > 70.0f) ||
        (!isnan(context.activeToolTempC) && context.activeToolTempC > 295.0f);

    switch (static_cast<ErrorAnimation>(animation)) {
        case ErrorAnimation::Blink: {
            const bool sides = ((now / 330U) & 1U) == 0U;
            fillSection(LedSection::Left, scaled(blue, sides ? 220U : 5U));
            fillSection(LedSection::Right, scaled(blue, sides ? 220U : 5U));
            fillSection(LedSection::Center, scaled(red, sides ? 5U : 235U));
            break;
        }
        case ErrorAnimation::Sos: {
            static constexpr uint16_t Durations[] = {
                150U, 150U, 150U, 150U, 150U, 300U,
                450U, 150U, 450U, 150U, 450U, 300U,
                150U, 150U, 150U, 150U, 150U, 850U,
            };
            uint32_t phase = now % 4800U;
            uint8_t slot = 0U;
            while (slot < sizeof(Durations) / sizeof(Durations[0]) && phase >= Durations[slot]) {
                phase -= Durations[slot++];
            }
            const bool on = slot < sizeof(Durations) / sizeof(Durations[0]) && (slot & 1U) == 0U;
            const uint8_t value = on ? 235U : 3U;
            fillSection(LedSection::Left, scaled(red, value));
            fillSection(LedSection::Center, scaled(red, value));
            fillSection(LedSection::Right, scaled(red, value));
            break;
        }
        case ErrorAnimation::Alarm: {
            const uint32_t cycle = now % 1450U;
            const bool sideFlash = cycle < 80U || (cycle >= 150U && cycle < 230U) ||
                                   (cycle >= 300U && cycle < 380U);
            fillSection(LedSection::Left, scaled(amber, sideFlash ? 245U : 10U));
            fillSection(LedSection::Right, scaled(amber, sideFlash ? 245U : 10U));
            const uint8_t core = static_cast<uint8_t>(70U + wave8(now / 12U) * 165U / 255U);
            fillSection(LedSection::Center, scaled(red, core));
            break;
        }
        case ErrorAnimation::Critical: {
            const uint32_t cycle = now % 1250U;
            const uint8_t value = cycle < 1040U ? 225U : cycle < 1110U ? 0U : 255U;
            fillSection(LedSection::Left, scaled(red, value));
            fillSection(LedSection::Center, scaled(red, value));
            fillSection(LedSection::Right, scaled(red, value));
            break;
        }
        case ErrorAnimation::Police: {
            const uint32_t cycle = now % 960U;
            const bool leftActive = cycle < 480U;
            const uint32_t local = cycle % 480U;
            const bool flash = local < 85U || (local >= 150U && local < 235U);
            fillSection(LedSection::Left, scaled(blue, leftActive && flash ? 245U : 6U));
            fillSection(LedSection::Right, scaled(red, !leftActive && flash ? 245U : 6U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool leftHalf = i < hw::CenterCount / 2U;
                const RgbwColor color = leftHalf ? blue : red;
                const bool active = leftHalf == leftActive;
                setSection(LedSection::Center, i, scaled(color, active && flash ? 145U : 4U));
            }
            break;
        }
        case ErrorAnimation::RedBreathe: {
            const uint8_t value = static_cast<uint8_t>(28U + wave8(now / 31U) * 180U / 255U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t local = wave8(static_cast<uint8_t>(now / 31U + i * 4U));
                    setSection(section, i, scaled(red,
                        static_cast<uint8_t>(value * (205U + local / 5U) / 255U)));
                }
            }
            break;
        }
        case ErrorAnimation::Heartbeat: {
            const uint32_t cycle = now % 1580U;
            uint8_t value = 12U;
            if (cycle < 80U) value = static_cast<uint8_t>(50U + cycle * 205U / 80U);
            else if (cycle < 155U) value = static_cast<uint8_t>(255U - (cycle - 80U) * 238U / 75U);
            else if (cycle >= 230U && cycle < 305U) value = static_cast<uint8_t>(45U + (cycle - 230U) * 190U / 75U);
            else if (cycle >= 305U && cycle < 410U) value = static_cast<uint8_t>(235U - (cycle - 305U) * 220U / 105U);
            fillSection(LedSection::Left, scaled(red, value));
            fillSection(LedSection::Center, scaled(red, value));
            fillSection(LedSection::Right, scaled(red, value));
            break;
        }
        case ErrorAnimation::Strobe: {
            const uint32_t cycle = now % 1700U;
            const bool flash = cycle < 55U || (cycle >= 105U && cycle < 160U) ||
                               (cycle >= 210U && cycle < 265U) || (cycle >= 315U && cycle < 370U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const bool alternating = ((path / 3U) & 1U) == ((cycle / 55U) & 1U);
                setOuterVisualPathPixel(path, scaled(red, flash && alternating ? 250U : 2U));
            }
            break;
        }
        case ErrorAnimation::RedWave: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t first = wave8(static_cast<uint8_t>(now / 18U - path * 14U));
                const uint8_t second = wave8(static_cast<uint8_t>(now / 27U + path * 9U));
                const uint8_t value = static_cast<uint8_t>(20U + first / 2U + second / 5U);
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::Xenon: {
            const uint32_t cycle = now % 2300U;
            const bool first = cycle < 28U;
            const bool second = cycle >= 95U && cycle < 125U;
            const uint8_t value = first || second ? 255U : 2U;
            const RgbwColor xenon = first ? RgbwColor(245U, 252U, 255U)
                                         : RgbwColor(185U, 220U, 255U);
            fillSection(LedSection::Left, scaled(xenon, value));
            fillSection(LedSection::Center, scaled(xenon, value));
            fillSection(LedSection::Right, scaled(xenon, value));
            break;
        }
        case ErrorAnimation::Siren: {
            const uint16_t head = static_cast<uint16_t>((now / 30U) % hw::OuterCount);
            for (uint8_t tail = 0; tail < 12U; ++tail) {
                const uint16_t path = (head + hw::OuterCount - tail) % hw::OuterCount;
                const uint8_t value = static_cast<uint8_t>(245U - tail * 19U);
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            setOuterVisualPathPixel(head, RgbwColor(245U, 245U, 245U));
            break;
        }
        case ErrorAnimation::Thunder: {
            const uint32_t bucket = now / 1250U;
            const uint32_t local = now % 1250U;
            const uint8_t strikeSection = hash8(bucket * 97U) % 3U;
            const bool strike = local < 45U || (local >= 105U && local < 145U) ||
                                ((hash8(bucket * 313U) > 150U) && local >= 210U && local < 250U);
            if (strike) {
                const LedSection section = VisualOuterSections[strikeSection];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const bool branch = ((i + hash8(bucket + i * 31U)) % 3U) != 0U;
                    if (branch) setSection(section, i, RgbwColor(205U, 225U, 255U));
                }
            } else {
                fillSection(LedSection::Left, scaled(red, 4U));
                fillSection(LedSection::Center, scaled(red, 2U));
                fillSection(LedSection::Right, scaled(red, 4U));
            }
            break;
        }
        case ErrorAnimation::Countdown: {
            const uint32_t elapsed = now % 8000U;
            const uint32_t period = max<uint32_t>(120U, 820U - elapsed * 700U / 8000U);
            const uint32_t pulsePhase = elapsed % period;
            const uint8_t pulse = pulsePhase < period / 3U
                ? static_cast<uint8_t>(255U - pulsePhase * 210U / max<uint32_t>(1U, period / 3U))
                : 20U;
            const uint16_t contraction = static_cast<uint16_t>(elapsed * (hw::OuterCount / 2U) / 8000U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t value = distance <= hw::OuterCount / 2U - contraction ? pulse : 3U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::Glitch: {
            const uint32_t frame = now / 72U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t noise = hash8(frame * 977U + path * 131U);
                uint8_t value = noise > 202U ? noise : 3U;
                RgbwColor color = red;
                if (noise > 246U) color = amber;
                if ((frame + path) % 23U == 0U) color = RgbwColor(235U, 235U, 235U);
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case ErrorAnimation::AlarmChase: {
            const uint8_t offset = static_cast<uint8_t>(now / 42U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t band = static_cast<uint8_t>((path + offset) % 10U);
                const bool whiteBand = band < 2U;
                const bool redBand = band < 6U;
                const RgbwColor color = whiteBand ? RgbwColor(235U, 235U, 235U) : red;
                setOuterVisualPathPixel(path, scaled(color, redBand ? 220U : 5U));
            }
            break;
        }
        case ErrorAnimation::DangerStripe: {
            const uint8_t offset = static_cast<uint8_t>(now / 95U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t stripe = static_cast<uint8_t>((path + offset) % 12U);
                const bool warningBand = stripe < 5U;
                const RgbwColor color = warningBand ? amber : red;
                const uint8_t value = warningBand ? 205U : (stripe < 9U ? 55U : 4U);
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case ErrorAnimation::PulseAlert: {
            const uint32_t cycle = now % 1050U;
            const uint8_t envelope = cycle < 90U
                ? static_cast<uint8_t>(cycle * 255U / 90U)
                : static_cast<uint8_t>(255U - (cycle - 90U) * 238U / 960U);
            const uint16_t radius = static_cast<uint16_t>(cycle * (hw::OuterCount / 2U + 5U) / 1050U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                const uint8_t value = delta < 5U
                    ? static_cast<uint8_t>(static_cast<uint16_t>(envelope) * (5U - delta) / 5U)
                    : static_cast<uint8_t>(envelope / 14U);
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::Redout: {
            const uint8_t collapse = wave8(now / 44U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t edge = static_cast<uint8_t>(distance * 255U / (hw::OuterCount / 2U));
                const uint8_t value = edge > collapse
                    ? static_cast<uint8_t>(30U + (edge - collapse) / 2U)
                    : 7U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::Emergency: {
            const uint8_t phase = static_cast<uint8_t>((now / 135U) % 6U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t block = static_cast<uint8_t>((i / 3U + sectionIndex + phase) % 6U);
                    const RgbwColor color = block < 2U ? RgbwColor(240U, 240U, 240U) : red;
                    const uint8_t value = block < 4U ? 235U : 5U;
                    setSection(section, i, scaled(color, value));
                }
            }
            break;
        }
        case ErrorAnimation::Meltdown: {
            const uint32_t frame = now / 58U;
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t heat = hash8(frame * 71U + i * 43U + sectionIndex * 181U);
                    const uint8_t height = static_cast<uint8_t>(i * 255U / max<uint16_t>(1U, count - 1U));
                    const uint8_t value = static_cast<uint8_t>(80U + heat / 2U);
                    const RgbwColor color = blend(red, amber,
                        static_cast<uint8_t>(min<uint16_t>(255U, heat / 2U + height / 3U)));
                    setSection(section, i, scaled(color, value));
                }
            }
            break;
        }
        case ErrorAnimation::Crash: {
            const uint32_t cycle = now % 1900U;
            const uint16_t center = hw::OuterCount / 2U;
            if (cycle < 820U) {
                const uint16_t radius = static_cast<uint16_t>(cycle * (hw::OuterCount / 2U + 5U) / 820U);
                for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                    const uint16_t distance = path > center ? path - center : center - path;
                    const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                    if (delta >= 4U) continue;
                    const RgbwColor color = delta == 0U ? RgbwColor(255U, 230U, 175U) : amber;
                    setOuterVisualPathPixel(path, scaled(color, static_cast<uint8_t>(240U - delta * 55U)));
                }
            } else if (cycle > 1550U) {
                const uint8_t warning = static_cast<uint8_t>((cycle - 1550U) * 75U / 350U);
                fillSection(LedSection::Center, scaled(red, warning));
            }
            break;
        }
        case ErrorAnimation::RedTheater: {
            const uint8_t offset = static_cast<uint8_t>((now / 115U) % 8U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t bulb = static_cast<uint8_t>((path + offset) % 8U);
                const uint8_t value = bulb == 0U ? 230U : bulb == 1U ? 85U : 8U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::FaultRipple: {
            const uint32_t cycle = now % 2600U;
            const uint32_t epoch = now / 2600U;
            const uint16_t origin = hash8(epoch * 151U) % hw::OuterCount;
            const uint16_t radius = static_cast<uint16_t>(cycle * (hw::OuterCount / 2U + 4U) / 1900U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > origin ? path - origin : origin - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                const uint8_t value = cycle < 1900U && delta < 4U
                    ? static_cast<uint8_t>(210U - delta * 50U)
                    : (path == origin ? 75U : 3U);
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::HotZone: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 70U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 40U);
            const uint8_t bedPercent = temperaturePercent(context.bedTempC, 20.0f, 110.0f, 50U);
            const uint8_t peak = max(toolPercent, max(chamberPercent, bedPercent));
            const uint8_t pulse = static_cast<uint8_t>(80U +
                static_cast<uint16_t>(wave8(now / max<uint8_t>(7U, 24U - peak / 7U))) * 165U / 255U);
            auto hotMeter = [&](LedSection section, uint8_t percent, bool reverse) {
                const uint16_t count = sectionCount(section);
                const RgbwColor heat = temperatureColor(percent);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t meterIndex = reverse ? count - 1U - i : i;
                    const uint8_t coverage = progressCoverage(percent, count, meterIndex);
                    setSection(section, i, scaled(heat, coverage ? static_cast<uint8_t>(pulse * coverage / 255U) : 4U));
                }
            };
            hotMeter(LedSection::Left, chamberPercent, false);
            hotMeter(LedSection::Center, bedPercent, false);
            hotMeter(LedSection::Right, toolPercent, true);
            break;
        }
        case ErrorAnimation::PanicComets: {
            static constexpr uint8_t Speeds[3] = {24U, 31U, 39U};
            static constexpr uint8_t Tails[3] = {5U, 8U, 11U};
            for (uint8_t comet = 0; comet < 3U; ++comet) {
                const uint16_t head = static_cast<uint16_t>((now / Speeds[comet] + comet * 13U) % hw::OuterCount);
                for (uint8_t tail = 0; tail < Tails[comet]; ++tail) {
                    const uint16_t path = (head + hw::OuterCount - tail) % hw::OuterCount;
                    const uint8_t value = static_cast<uint8_t>(235U - tail * (190U / Tails[comet]));
                    setOuterVisualPathPixel(path, scaled(comet == 1U ? amber : red, value));
                }
            }
            break;
        }
        case ErrorAnimation::Lockdown: {
            const uint32_t cycle = now % 3000U;
            const uint16_t gate = cycle < 1500U
                ? static_cast<uint16_t>(cycle * (hw::OuterCount / 2U) / 1500U)
                : hw::OuterCount / 2U;
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t edgeDistance = min<uint16_t>(path, hw::OuterCount - 1U - path);
                const uint16_t distance = path > center ? path - center : center - path;
                uint8_t value = edgeDistance <= gate ? 62U : 3U;
                if (distance <= 1U && cycle >= 1500U) value = ((now / 170U) & 1U) ? 235U : 85U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::WarningTicks: {
            const uint8_t tick = static_cast<uint8_t>((now / 190U) % 12U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const bool mark = i % 3U == 0U;
                    const uint8_t markNumber = static_cast<uint8_t>((i / 3U + sectionIndex * 4U) % 12U);
                    const uint8_t value = !mark ? 3U : markNumber == tick ? 230U : 35U;
                    setSection(section, i, scaled(markNumber == tick ? amber : red, value));
                }
            }
            break;
        }
        case ErrorAnimation::BreachScan: {
            const uint32_t cycle = now % 4200U;
            const uint32_t epoch = now / 4200U;
            const uint16_t breach = hash8(epoch * 211U) % hw::OuterCount;
            const uint16_t scan = cycle < 3000U
                ? static_cast<uint16_t>(cycle * (hw::OuterCount - 1U) / 3000U)
                : breach;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > scan ? path - scan : scan - path;
                uint8_t value = distance < 4U ? static_cast<uint8_t>(200U - distance * 48U) : 3U;
                RgbwColor color = red;
                if (cycle >= 3000U && path == breach) {
                    value = ((now / 120U) & 1U) ? 250U : 55U;
                    color = amber;
                }
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case ErrorAnimation::FaultSparks: {
            const uint32_t frame = now / 85U;
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t noise = hash8(frame * 83U + path * 197U);
                const uint8_t chance = distance < 8U ? 205U : 238U;
                const uint8_t value = noise > chance ? static_cast<uint8_t>(95U + (noise - chance) * 6U) : 2U;
                setOuterVisualPathPixel(path, scaled(noise > 248U ? amber : red, value));
            }
            break;
        }
        case ErrorAnimation::RedJuggle: {
            static constexpr uint16_t Periods[3] = {1700U, 2300U, 2900U};
            for (uint8_t ball = 0; ball < 3U; ++ball) {
                const uint32_t phase = (now + ball * 430U) % Periods[ball];
                const uint32_t half = Periods[ball] / 2U;
                const uint32_t travel = phase < half ? phase : Periods[ball] - phase;
                const uint16_t head = static_cast<uint16_t>(travel * (hw::OuterCount - 1U) / half);
                for (int8_t offset = -2; offset <= 2; ++offset) {
                    const int16_t path = static_cast<int16_t>(head) + offset;
                    if (path < 0 || path >= static_cast<int16_t>(hw::OuterCount)) continue;
                    const uint8_t value = static_cast<uint8_t>(220U - abs(offset) * 68U);
                    setOuterVisualPathPixel(static_cast<uint16_t>(path), scaled(ball == 1U ? amber : red, value));
                }
            }
            break;
        }
        case ErrorAnimation::Evacuate: {
            const uint8_t step = static_cast<uint8_t>((now / 95U) % 8U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t outward = i < count / 2U ? count / 2U - i : i - count / 2U;
                    const uint8_t arrow = static_cast<uint8_t>((outward + step) % 8U);
                    const uint8_t value = arrow < 2U ? 225U : arrow < 4U ? 65U : 4U;
                    setSection(section, i, scaled(amber, value));
                }
            }
            break;
        }
        case ErrorAnimation::CauseHint: {
            const uint8_t beat = ((now % 1250U) < 140U || ((now % 1250U) >= 260U && (now % 1250U) < 400U))
                ? 230U : 28U;
            fillSection(LedSection::Left, scaled(networkFault ? blue : green, networkFault ? beat : 48U));
            fillSection(LedSection::Right, scaled(thermalFault ? amber : green, thermalFault ? beat : 48U));
            const bool unknown = !networkFault && !thermalFault;
            fillSection(LedSection::Center, scaled(unknown ? red : amber, unknown ? beat : 75U));
            break;
        }
        case ErrorAnimation::StackLight: {
            const uint32_t cycle = now % 1800U;
            const uint8_t active = static_cast<uint8_t>((cycle / 600U) % 3U);
            fillSection(LedSection::Left, scaled(red, active == 0U ? 230U : 34U));
            fillSection(LedSection::Center, scaled(amber, active == 1U ? 230U : 34U));
            fillSection(LedSection::Right, scaled(networkFault || thermalFault ? red : green,
                                                  active == 2U ? 230U : 34U));
            break;
        }
        case ErrorAnimation::SmartHeartbeat: {
            uint8_t severity = 1U;
            if (networkFault) ++severity;
            if (thermalFault) severity += 2U;
            const uint32_t period = max<uint32_t>(650U, 1750U - severity * 270U);
            const uint32_t cycle = now % period;
            const uint32_t beatWidth = 70U + severity * 8U;
            const bool beat = cycle < beatWidth || (cycle >= beatWidth * 2U && cycle < beatWidth * 3U);
            const uint8_t value = beat ? 245U : static_cast<uint8_t>(14U + severity * 5U);
            fillSection(LedSection::Left, scaled(red, value));
            fillSection(LedSection::Center, scaled(thermalFault ? amber : red, value));
            fillSection(LedSection::Right, scaled(red, value));
            break;
        }
        case ErrorAnimation::LocationSplit: {
            const uint8_t scan = static_cast<uint8_t>((now / 80U) % hw::CenterCount);
            fillSection(LedSection::Left, scaled(networkFault ? blue : green, networkFault ? 155U : 35U));
            fillSection(LedSection::Right, scaled(thermalFault ? amber : green, thermalFault ? 155U : 35U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > scan ? i - scan : scan - i;
                const uint8_t value = distance < 4U ? static_cast<uint8_t>(190U - distance * 48U) : 8U;
                setSection(LedSection::Center, i, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::BlackoutFlash: {
            const uint32_t cycle = now % 2100U;
            const bool flash = cycle < 42U || (cycle >= 115U && cycle < 150U);
            const RgbwColor color = flash ? RgbwColor(255U, 255U, 255U) : red;
            const uint8_t value = flash ? 255U : (cycle > 1500U ? 12U : 0U);
            fillSection(LedSection::Left, scaled(color, value));
            fillSection(LedSection::Center, scaled(color, value));
            fillSection(LedSection::Right, scaled(color, value));
            break;
        }
        case ErrorAnimation::RecoveryWait: {
            const bool recovering = !networkFault && !thermalFault;
            const RgbwColor stateColor = recovering ? green : amber;
            const uint16_t head = static_cast<uint16_t>((now / 105U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = head >= path ? head - path : head + hw::OuterCount - path;
                const uint8_t value = distance < 8U
                    ? static_cast<uint8_t>(170U - distance * 19U)
                    : 5U;
                setOuterVisualPathPixel(path, scaled(stateColor, value));
            }
            const uint8_t centerPulse = static_cast<uint8_t>(35U + wave8(now / 45U) / 3U);
            fillSection(LedSection::Center, scaled(stateColor, centerPulse));
            break;
        }
        case ErrorAnimation::SirenScan: {
            const uint32_t cycle = now % 2400U;
            const uint32_t half = cycle < 1200U ? cycle : 2400U - cycle;
            const uint16_t scan = static_cast<uint16_t>(half * (hw::OuterCount - 1U) / 1200U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > scan ? path - scan : scan - path;
                const uint8_t value = distance < 7U ? static_cast<uint8_t>(235U - distance * 32U) : 7U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::DiagnosticBits: {
            uint8_t bits = 0U;
            if (networkFault) bits |= 0x01U;
            if (thermalFault) bits |= 0x02U;
            if (context.ventFailsafe) bits |= 0x04U;
            if (context.printerTelemetryAgeMs > 60000U) bits |= 0x08U;
            fillSection(LedSection::Left, scaled(networkFault ? blue : green, networkFault ? 130U : 30U));
            fillSection(LedSection::Right, scaled(thermalFault ? amber : green, thermalFault ? 130U : 30U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t bit = static_cast<uint8_t>(1U << (i % 4U));
                const bool set = (bits & bit) != 0U;
                const bool separator = i % 5U == 4U;
                setSection(LedSection::Center, i,
                           scaled(set ? red : green, separator ? 2U : (set ? 210U : 22U)));
            }
            break;
        }
        case ErrorAnimation::ServiceBeacon: {
            const uint32_t cycle = now % 3200U;
            const uint8_t pulse = cycle < 600U
                ? wave8(static_cast<uint8_t>(cycle * 255U / 600U))
                : 0U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(cycle < 1200U ? cycle * center / 1200U : center);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                const uint8_t value = delta < 3U ? static_cast<uint8_t>(80U + pulse / 2U) : 6U;
                setOuterVisualPathPixel(path, scaled(amber, value));
            }
            setOuterVisualPathPixel(center, scaled(red, static_cast<uint8_t>(95U + pulse / 2U)));
            break;
        }
        case ErrorAnimation::SafeShutdown: {
            const uint32_t cycle = now % 5200U;
            const uint8_t fade = cycle < 3600U
                ? static_cast<uint8_t>(255U - cycle * 235U / 3600U)
                : 20U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t boundary = static_cast<uint16_t>(cycle < 3600U
                ? cycle * center / 3600U : center);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t edge = min<uint16_t>(path, hw::OuterCount - 1U - path);
                const uint8_t value = edge >= boundary ? fade : 2U;
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            if (cycle >= 3900U && cycle < 4080U) {
                setOuterVisualPathPixel(center, scaled(amber, 145U));
            }
            break;
        }
        case ErrorAnimation::CalmAlert: {
            const uint8_t breath = static_cast<uint8_t>(38U + wave8(now / 52U) * 125U / 255U);
            fillSection(LedSection::Left, scaled(red, breath));
            fillSection(LedSection::Right, scaled(red, breath));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t edge = min<uint16_t>(i, hw::CenterCount - 1U - i);
                const uint8_t value = static_cast<uint8_t>(breath * (150U + edge * 10U) / 255U);
                setSection(LedSection::Center, i, scaled(amber, value));
            }
            break;
        }
        case ErrorAnimation::FaultLocator: {
            uint16_t faultPath = hw::OuterCount / 2U;
            if (networkFault) faultPath = hw::LeftCount / 2U;
            else if (thermalFault) faultPath = hw::LeftCount + hw::CenterCount + hw::RightCount / 2U;
            const uint16_t scan = static_cast<uint16_t>((now / 62U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t scanDistance = path > scan ? path - scan : scan - path;
                uint8_t value = scanDistance < 4U ? static_cast<uint8_t>(165U - scanDistance * 38U) : 5U;
                RgbwColor color = red;
                const uint16_t faultDistance = path > faultPath ? path - faultPath : faultPath - path;
                if (faultDistance <= 1U) {
                    value = ((now / 145U) & 1U) ? 240U : 70U;
                    color = networkFault ? blue : amber;
                }
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case ErrorAnimation::ThermalCut: {
            const uint8_t toolPercent = temperaturePercent(context.activeToolTempC, 20.0f, 300.0f, 75U);
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 45U);
            const uint8_t hottest = max(toolPercent, chamberPercent);
            const RgbwColor heat = temperatureColor(hottest);
            const uint8_t pulse = static_cast<uint8_t>(65U +
                wave8(now / max<uint8_t>(7U, 30U - hottest / 5U)) * 175U / 255U);
            const uint16_t cut = static_cast<uint16_t>(hottest * hw::OuterCount / 100U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const bool severed = path == cut || path + 1U == cut;
                setOuterVisualPathPixel(path, severed ? RgbwColor() : scaled(heat, pulse));
            }
            if (cut < hw::OuterCount) setOuterVisualPathPixel(cut, scaled(RgbwColor(255U, 255U, 255U), 180U));
            break;
        }
        case ErrorAnimation::NetworkLost: {
            const uint8_t offset = static_cast<uint8_t>(now / 105U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t packet = static_cast<uint8_t>((path + offset) % 13U);
                const bool payload = packet < 4U;
                const bool dropped = hash8((now / 650U) * 83U + path * 17U) > 224U;
                const RgbwColor color = dropped ? red : blue;
                const uint8_t value = dropped ? 180U : (payload ? 165U : 5U);
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case ErrorAnimation::ServiceCode: {
            uint8_t code = 1U;
            if (networkFault) code = 2U;
            if (thermalFault) code = 3U;
            if (networkFault && thermalFault) code = 4U;
            const uint32_t cycle = now % 4200U;
            const uint8_t flashIndex = static_cast<uint8_t>(cycle / 420U);
            const bool flash = flashIndex < code * 2U && (flashIndex & 1U) == 0U && (cycle % 420U) < 150U;
            fillSection(LedSection::Left, scaled(amber, flash ? 210U : 18U));
            fillSection(LedSection::Right, scaled(amber, flash ? 210U : 18U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const bool digit = i < code * 4U && (i % 4U) < 2U;
                setSection(LedSection::Center, i, scaled(red, digit ? 145U : 4U));
            }
            break;
        }
        case ErrorAnimation::Containment: {
            const uint32_t cycle = now % 3600U;
            const uint16_t gate = cycle < 1800U
                ? static_cast<uint16_t>(cycle * (hw::CenterCount / 2U) / 1800U)
                : hw::CenterCount / 2U;
            fillSection(LedSection::Left, scaled(red, 80U));
            fillSection(LedSection::Right, scaled(red, 80U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t edge = min<uint16_t>(i, hw::CenterCount - 1U - i);
                uint8_t value = edge <= gate ? 175U : 5U;
                if (gate >= hw::CenterCount / 2U &&
                    (i == hw::CenterCount / 2U || i + 1U == hw::CenterCount / 2U)) {
                    value = ((now / 160U) & 1U) ? 245U : 70U;
                }
                setSection(LedSection::Center, i, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::SafeBreath: {
            const uint8_t breath = wave8(now / 64U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t phase = static_cast<uint8_t>(breath + distance * 10U);
                const uint8_t value = static_cast<uint8_t>(18U + wave8(phase) * 80U / 255U);
                setOuterVisualPathPixel(path, scaled(red, value));
            }
            break;
        }
        case ErrorAnimation::Escalation: {
            const uint32_t cycle = now % 6000U;
            const uint8_t ramp = static_cast<uint8_t>(cycle * 255U / 6000U);
            const uint32_t period = max<uint32_t>(130U, 760U - ramp * 620U / 255U);
            const uint8_t pulse = (cycle % period) < period / 3U
                ? static_cast<uint8_t>(65U + ramp * 190U / 255U)
                : static_cast<uint8_t>(10U + ramp / 8U);
            fillSection(LedSection::Left, scaled(red, pulse));
            fillSection(LedSection::Center, scaled(blend(red, amber, ramp), pulse));
            fillSection(LedSection::Right, scaled(red, pulse));
            if (ramp > 220U) {
                const uint16_t flash = static_cast<uint16_t>((now / 45U) % hw::OuterCount);
                setOuterVisualPathPixel(flash, RgbwColor(245U, 245U, 245U));
            }
            break;
        }
        case ErrorAnimation::RepairBeacon: {
            const bool ready = !networkFault && !thermalFault;
            const RgbwColor stateColor = ready ? green : amber;
            const uint16_t head = static_cast<uint16_t>((now / 92U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > head ? path - head : head - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint8_t value = distance < 5U ? static_cast<uint8_t>(190U - distance * 38U) : 8U;
                setOuterVisualPathPixel(path, scaled(stateColor, value));
            }
            const uint8_t heartbeat = ((now % 1800U) < 110U) ? 220U : 42U;
            fillSection(LedSection::Center, scaled(ready ? green : red, heartbeat));
            break;
        }
        case ErrorAnimation::CoolingAlarm: {
            const uint8_t chamberPercent = temperaturePercent(context.chamberTempC, 20.0f, 80.0f, 45U);
            const RgbwColor heat = temperatureColor(chamberPercent);
            const RgbwColor cooling = decorativeHsv(LedCategory::Error, 145U, 235U, 255U);
            const uint8_t speed = max<uint8_t>(8U, 32U - chamberPercent / 4U);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t wave = wave8(static_cast<uint8_t>(now / speed + i * 22U));
                setSection(LedSection::Left, i, scaled(cooling, static_cast<uint8_t>(25U + wave / 2U)));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(heat, static_cast<uint8_t>(45U + wave * chamberPercent / 150U)));
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (hw::CenterCount - 1U));
                setSection(LedSection::Center, i,
                           scaled(blend(cooling, heat, amount), static_cast<uint8_t>(55U + wave8(now / speed + i * 13U) / 3U)));
            }
            break;
        }
        case ErrorAnimation::Count:
        default:
            break;
    }
}

void LedService::renderFinish(uint8_t animation, const LedAnimationContext& context) {
    const uint32_t now = context.nowMs;
    RgbwColor filament = fromRgb(context.filamentRgb);
    const uint8_t filamentMaximum = max(filament.r, max(filament.g, filament.b));
    if (filamentMaximum < 12U) {
        filament = decorativeHsv(LedCategory::Finish,
                                 static_cast<uint8_t>(now / 18U), 245U, 235U);
    }
    const RgbwColor gold = decorativeHsv(LedCategory::Finish, 28U, 235U, 255U);
    const RgbwColor green = decorativeHsv(LedCategory::Finish, 92U, 235U, 255U);

    switch (static_cast<FinishAnimation>(animation)) {
        case FinishAnimation::Sweep: {
            const uint16_t head = static_cast<uint16_t>((now / 42U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = head >= path
                    ? head - path : head + hw::OuterCount - path;
                if (distance > 9U) continue;
                const uint8_t value = static_cast<uint8_t>(245U - distance * 25U);
                const RgbwColor color = distance == 0U
                    ? RgbwColor(235U, 255U, 235U) : blend(green, gold, distance * 18U);
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case FinishAnimation::Rainbow: {
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t hue = static_cast<uint8_t>(now / 16U +
                        static_cast<uint32_t>(i) * 256U / count + sectionIndex * 9U);
                    const uint8_t value = static_cast<uint8_t>(130U +
                        wave8(static_cast<uint8_t>(now / 25U + i * 13U)) / 3U);
                    setSection(section, i,
                               decorativeHsv(LedCategory::Finish, hue, 245U, value));
                }
            }
            break;
        }
        case FinishAnimation::Pulse: {
            const uint32_t cycle = now % 1900U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(
                cycle * (center + 5U) / 1900U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                const uint8_t base = static_cast<uint8_t>(18U + wave8(now / 42U) / 8U);
                const uint8_t value = delta < 5U
                    ? static_cast<uint8_t>(245U - delta * 46U) : base;
                setOuterVisualPathPixel(path,
                    scaled(delta == 0U ? RgbwColor(255U, 248U, 218U) : gold, value));
            }
            break;
        }
        case FinishAnimation::Filament: {
            const uint8_t breath = static_cast<uint8_t>(95U + wave8(now / 34U) / 2U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t crest = wave8(static_cast<uint8_t>(now / 18U - path * 13U));
                RgbwColor color = scaled(filament,
                    static_cast<uint8_t>(breath * (180U + crest / 4U) / 255U));
                if (crest > 238U) color = blend(color, RgbwColor(255U, 255U, 255U), 70U);
                setOuterVisualPathPixel(path, color);
            }
            break;
        }
        case FinishAnimation::Fireworks: {
            const uint32_t cycle = now % 2400U;
            const uint32_t epoch = now / 2400U;
            const uint16_t origin = static_cast<uint16_t>(8U +
                hash8(epoch * 193U) % (hw::OuterCount - 16U));
            if (cycle < 650U) {
                const uint16_t launch = static_cast<uint16_t>(cycle * origin / 650U);
                for (uint8_t tail = 0; tail < 6U; ++tail) {
                    if (launch < tail) continue;
                    setOuterVisualPathPixel(launch - tail,
                        scaled(gold, static_cast<uint8_t>(235U - tail * 36U)));
                }
            } else if (cycle < 1850U) {
                const uint16_t radius = static_cast<uint16_t>((cycle - 650U) * 18U / 1200U);
                const uint8_t burstHue = hash8(epoch * 307U + 71U);
                for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                    const uint16_t direct = path > origin ? path - origin : origin - path;
                    const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                    const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                    if (delta > 2U) continue;
                    const uint8_t fade = static_cast<uint8_t>(255U -
                        (cycle - 650U) * 150U / 1200U - delta * 35U);
                    setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(burstHue + path * 11U), 235U, fade));
                }
            }
            break;
        }
        case FinishAnimation::Curtain: {
            const uint32_t cycle = now % 4200U;
            const uint16_t half = hw::CenterCount / 2U;
            const uint16_t opening = static_cast<uint16_t>(min<uint32_t>(half,
                cycle < 1900U ? cycle * half / 1900U : half));
            fillSection(LedSection::Left, scaled(gold,
                static_cast<uint8_t>(65U + wave8(now / 45U) / 3U)));
            fillSection(LedSection::Right, scaled(gold,
                static_cast<uint8_t>(65U + wave8(now / 45U + 96U) / 3U)));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                if (distance >= opening) continue;
                const uint8_t fold = wave8(static_cast<uint8_t>(now / 31U + i * 28U));
                setSection(LedSection::Center, i,
                    scaled(gold, static_cast<uint8_t>(95U + fold / 2U)));
            }
            break;
        }
        case FinishAnimation::Confetti: {
            const uint32_t frame = now / 95U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t age = static_cast<uint8_t>((frame + path * 7U) & 31U);
                const uint8_t seed = hash8((frame - age) * 157U + path * 83U);
                if (seed < 205U || age > 10U) continue;
                const uint8_t value = static_cast<uint8_t>(235U - age * 19U);
                setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Finish,
                    static_cast<uint8_t>(seed + path * 29U), 240U, value));
            }
            break;
        }
        case FinishAnimation::GoldRain: {
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t phase = static_cast<uint8_t>(now / 36U +
                        i * 37U + sectionIndex * 71U);
                    const uint8_t drop = wave8(phase);
                    const uint8_t sparkle = drop > 220U
                        ? static_cast<uint8_t>(90U + (drop - 220U) * 4U) : 10U;
                    setSection(section, i, scaled(gold, sparkle));
                }
            }
            break;
        }
        case FinishAnimation::StrobeParty: {
            const uint32_t beat = now % 960U;
            const uint8_t step = static_cast<uint8_t>((now / 120U) & 7U);
            const bool accent = beat < 70U || (beat >= 240U && beat < 305U) ||
                                (beat >= 600U && beat < 690U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint8_t hue = static_cast<uint8_t>(step * 39U + sectionIndex * 73U);
                const uint8_t value = accent ? 235U : 18U;
                fillSection(section,
                    decorativeHsv(LedCategory::Finish, hue, 245U, value));
            }
            break;
        }
        case FinishAnimation::BouncingBalls: {
            static constexpr uint16_t Periods[4] = {2100U, 2670U, 3230U, 3810U};
            static constexpr uint8_t Hues[4] = {0U, 42U, 108U, 176U};
            for (uint8_t ball = 0; ball < 4U; ++ball) {
                const uint32_t phase = (now + ball * 431U) % Periods[ball];
                const uint32_t halfPeriod = Periods[ball] / 2U;
                const uint32_t travel = phase <= halfPeriod ? phase : Periods[ball] - phase;
                const uint16_t head = static_cast<uint16_t>(
                    travel * (hw::OuterCount - 1U) / halfPeriod);
                for (uint8_t tail = 0; tail < 4U; ++tail) {
                    const uint16_t path = head >= tail ? head - tail : 0U;
                    const uint8_t value = static_cast<uint8_t>(245U - tail * 55U);
                    setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(Hues[ball] + now / 70U), 240U, value));
                }
            }
            break;
        }
        case FinishAnimation::RainbowExplosion: {
            const uint32_t cycle = now % 2600U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(min<uint32_t>(center + 6U,
                cycle < 1250U ? cycle * (center + 6U) / 1250U : center + 6U));
            const uint8_t fade = cycle < 1250U ? 255U
                : cycle < 2250U ? static_cast<uint8_t>(255U - (cycle - 1250U) * 225U / 1000U)
                                : 20U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                if (distance > radius) continue;
                const uint16_t wake = radius - distance;
                const uint8_t value = wake < 5U
                    ? static_cast<uint8_t>(fade * (5U - wake) / 5U)
                    : static_cast<uint8_t>(fade / 5U);
                setOuterVisualPathPixel(path, decorativeHsv(LedCategory::Finish,
                    static_cast<uint8_t>(distance * 17U + now / 32U), 245U, value));
            }
            break;
        }
        case FinishAnimation::Disco: {
            const uint32_t beat = now / 180U;
            const uint8_t local = static_cast<uint8_t>((now % 180U) * 255U / 180U);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = VisualOuterSections[sectionIndex];
                const uint16_t count = sectionCount(section);
                const uint8_t baseHue = hash8(beat * 97U + sectionIndex * 83U);
                for (uint16_t i = 0; i < count; ++i) {
                    const uint8_t block = static_cast<uint8_t>(i / 3U);
                    const uint8_t hue = static_cast<uint8_t>(baseHue + block * 37U);
                    const bool active = ((block + beat + sectionIndex) & 1U) == 0U;
                    const uint8_t value = active
                        ? static_cast<uint8_t>(225U - local / 5U) : 12U;
                    setSection(section, i,
                        decorativeHsv(LedCategory::Finish, hue, 250U, value));
                }
            }
            break;
        }
        case FinishAnimation::Heart: {
            const uint32_t cycle = now % 1650U;
            uint8_t envelope = 12U;
            if (cycle < 100U) envelope = static_cast<uint8_t>(45U + cycle * 210U / 100U);
            else if (cycle < 220U) envelope = static_cast<uint8_t>(255U - (cycle - 100U) * 225U / 120U);
            else if (cycle >= 310U && cycle < 405U) envelope = static_cast<uint8_t>(45U + (cycle - 310U) * 195U / 95U);
            else if (cycle >= 405U && cycle < 560U) envelope = static_cast<uint8_t>(240U - (cycle - 405U) * 220U / 155U);
            const uint16_t center = hw::OuterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t shape = static_cast<uint8_t>(max<int>(70, 255 - distance * 8));
                const RgbwColor rose = decorativeHsv(LedCategory::Finish,
                    static_cast<uint8_t>(247U + distance / 3U), 235U, 255U);
                setOuterVisualPathPixel(path,
                    scaled(rose, static_cast<uint8_t>(envelope * shape / 255U)));
            }
            break;
        }
        case FinishAnimation::ColorSpiral: {
            const uint8_t rotation = static_cast<uint8_t>(now / 13U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t helix = wave8(static_cast<uint8_t>(rotation + path * 19U));
                const uint8_t hue = static_cast<uint8_t>(rotation / 2U + path * 11U + helix / 6U);
                const uint8_t value = static_cast<uint8_t>(70U + helix * 165U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Finish, hue, 245U, value));
            }
            break;
        }
        case FinishAnimation::Sparkle: {
            const uint32_t frame = now / 125U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t shimmer = hash8(frame * 137U + path * 71U);
                RgbwColor color = scaled(filament, 42U);
                if (shimmer > 232U) {
                    const uint8_t glint = static_cast<uint8_t>(130U + (shimmer - 232U) * 5U);
                    color = blend(color, RgbwColor(255U, 250U, 225U), glint);
                }
                setOuterVisualPathPixel(path, color);
            }
            break;
        }
        case FinishAnimation::Champagne: {
            fillSection(LedSection::Center, scaled(gold, 10U));
            const uint32_t step = now / 85U;
            for (uint8_t bubble = 0; bubble < 6U; ++bubble) {
                const uint16_t leftPosition = static_cast<uint16_t>((step + bubble * 4U) % (hw::LeftCount + 5U));
                const uint16_t rightPosition = static_cast<uint16_t>((step + bubble * 6U + 2U) % (hw::RightCount + 5U));
                if (leftPosition < hw::LeftCount) {
                    setSection(LedSection::Left, leftPosition,
                               scaled(gold, static_cast<uint8_t>(150U + bubble * 15U)));
                } else {
                    const uint16_t burst = static_cast<uint16_t>((bubble * 3U + step) % hw::CenterCount);
                    setSection(LedSection::Center, burst, RgbwColor(255U, 248U, 210U));
                }
                if (rightPosition < hw::RightCount) {
                    setSection(LedSection::Right, hw::RightCount - 1U - rightPosition,
                               scaled(gold, static_cast<uint8_t>(145U + bubble * 16U)));
                } else {
                    const uint16_t burst = static_cast<uint16_t>((hw::CenterCount - 1U -
                        (bubble * 4U + step) % hw::CenterCount));
                    setSection(LedSection::Center, burst, RgbwColor(255U, 248U, 210U));
                }
            }
            break;
        }
        case FinishAnimation::WipeOut: {
            const uint32_t cycle = now % 3600U;
            const uint16_t half = (hw::OuterCount + 1U) / 2U;
            const uint16_t reach = cycle < 1800U
                ? static_cast<uint16_t>(cycle * half / 1800U)
                : static_cast<uint16_t>((3600U - cycle) * half / 1800U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t edge = min<uint16_t>(path, hw::OuterCount - 1U - path);
                if (edge >= reach) continue;
                const uint8_t amount = static_cast<uint8_t>(path * 255U / (hw::OuterCount - 1U));
                const RgbwColor color = blend(filament, gold, amount);
                setOuterVisualPathPixel(path, scaled(color,
                    static_cast<uint8_t>(105U + wave8(now / 26U + path * 9U) / 2U)));
            }
            break;
        }
        case FinishAnimation::Fill: {
            const uint32_t cycle = now % 3900U;
            const uint32_t exact = min<uint32_t>(hw::OuterCount * 255U,
                cycle < 2600U ? cycle * hw::OuterCount * 255U / 2600U
                              : hw::OuterCount * 255U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint32_t start = static_cast<uint32_t>(path) * 255U;
                const uint8_t coverage = exact <= start ? 0U
                    : exact >= start + 255U ? 255U : static_cast<uint8_t>(exact - start);
                if (!coverage) continue;
                const uint8_t shimmer = cycle >= 2600U
                    ? static_cast<uint8_t>(160U + wave8(now / 18U + path * 15U) / 3U)
                    : 205U;
                setOuterVisualPathPixel(path,
                    scaled(gold, static_cast<uint8_t>(coverage * shimmer / 255U)));
            }
            break;
        }
        case FinishAnimation::Waterfall: {
            const uint16_t phase = static_cast<uint16_t>((now / 58U) % (hw::OuterCount + 12U));
            for (uint8_t stream = 0; stream < 3U; ++stream) {
                const uint16_t head = static_cast<uint16_t>((phase + stream * 14U) % hw::OuterCount);
                for (uint8_t tail = 0; tail < 8U; ++tail) {
                    if (head < tail) continue;
                    const uint16_t path = head - tail;
                    const uint8_t hue = static_cast<uint8_t>(145U + stream * 36U + tail * 2U);
                    const uint8_t value = static_cast<uint8_t>(225U - tail * 25U);
                    setOuterVisualPathPixel(path,
                        decorativeHsv(LedCategory::Finish, hue, 210U, value));
                }
            }
            break;
        }
        case FinishAnimation::Starburst: {
            const uint32_t cycle = now % 1550U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(cycle < 650U
                ? cycle * (center + 4U) / 650U : center + 4U);
            const uint8_t fade = cycle < 650U ? 255U
                : static_cast<uint8_t>(max<int>(0, 255 - (cycle - 650U) * 255U / 900U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                if (delta > 1U && distance != 0U) continue;
                const uint8_t value = distance == 0U
                    ? static_cast<uint8_t>(fade / 2U)
                    : static_cast<uint8_t>(max<int>(0, fade - delta * 70));
                const RgbwColor color = distance == 0U ? RgbwColor(255U, 255U, 245U)
                    : decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(now / 20U + path * 17U), 240U, 255U);
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case FinishAnimation::VictoryLap: {
            const uint16_t head = static_cast<uint16_t>((now / 34U) % hw::OuterCount);
            const uint16_t gate = hw::LeftCount + hw::CenterCount / 2U;
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const bool checker = path >= gate - 2U && path <= gate + 2U;
                if (checker) {
                    const bool white = ((path + now / 180U) & 1U) == 0U;
                    setOuterVisualPathPixel(path, scaled(white ? RgbwColor(245U, 245U, 235U) : gold, 70U));
                }
                const uint16_t distance = head >= path
                    ? head - path : head + hw::OuterCount - path;
                if (distance > 7U) continue;
                const uint8_t value = static_cast<uint8_t>(250U - distance * 31U);
                setOuterVisualPathPixel(path, scaled(distance == 0U ? green : filament, value));
            }
            break;
        }
        case FinishAnimation::GoldTheater: {
            const uint8_t offset = static_cast<uint8_t>((now / 115U) % 6U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t bulb = static_cast<uint8_t>((path + offset) % 6U);
                const uint8_t value = bulb == 0U ? 245U : bulb == 1U ? 110U : 22U;
                const RgbwColor color = bulb == 0U ? RgbwColor(255U, 245U, 205U) : gold;
                setOuterVisualPathPixel(path, scaled(color, value));
            }
            break;
        }
        case FinishAnimation::RibbonDance: {
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t first = wave8(static_cast<uint8_t>(now / 17U + path * 18U));
                const uint8_t second = wave8(static_cast<uint8_t>(now / 23U - path * 18U + 128U));
                const uint8_t firstValue = first > 170U ? static_cast<uint8_t>((first - 170U) * 3U) : 0U;
                const uint8_t secondValue = second > 170U ? static_cast<uint8_t>((second - 170U) * 3U) : 0U;
                RgbwColor color = scaled(filament, firstValue);
                color = blend(color, scaled(gold, secondValue),
                              static_cast<uint8_t>(secondValue > firstValue ? 190U : 70U));
                setOuterVisualPathPixel(path, color);
            }
            break;
        }
        case FinishAnimation::TrophyGlow: {
            const uint8_t breath = static_cast<uint8_t>(105U + wave8(now / 52U) / 2U);
            const uint16_t half = hw::CenterCount / 2U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                uint8_t value = distance < 3U ? 245U
                    : distance < 6U ? 150U : static_cast<uint8_t>(breath / 2U);
                if (i == 2U || i == hw::CenterCount - 3U) value = 95U;
                setSection(LedSection::Center, i, scaled(gold, value));
            }
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t handle = i >= 2U && i <= 7U
                    ? static_cast<uint8_t>(breath * (i == 2U || i == 7U ? 2U : 1U) / 2U) : 14U;
                setSection(LedSection::Left, i, scaled(gold, handle));
                setSection(LedSection::Right, hw::RightCount - 1U - i, scaled(gold, handle));
            }
            break;
        }
        case FinishAnimation::StarGlitter: {
            const uint32_t frame = now / 140U;
            const RgbwColor night = decorativeHsv(LedCategory::Finish, 168U, 210U, 255U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t star = hash8(path * 109U + frame * 31U);
                RgbwColor color = scaled(night, 10U);
                if (star > 236U) {
                    const uint8_t warmth = hash8(path * 53U) & 31U;
                    color = RgbwColor(255U, static_cast<uint8_t>(245U - warmth / 2U),
                                     static_cast<uint8_t>(210U + warmth));
                    color = scaled(color, static_cast<uint8_t>(135U + (star - 236U) * 6U));
                }
                setOuterVisualPathPixel(path, color);
            }
            break;
        }
        case FinishAnimation::DualComets: {
            const uint16_t firstHead = static_cast<uint16_t>((now / 45U) % hw::OuterCount);
            const uint16_t secondHead = hw::OuterCount - 1U - firstHead;
            for (uint8_t tail = 0; tail < 10U; ++tail) {
                const uint8_t value = static_cast<uint8_t>(245U - tail * 23U);
                const uint16_t first = (firstHead + hw::OuterCount - tail) % hw::OuterCount;
                const uint16_t second = (secondHead + tail) % hw::OuterCount;
                setOuterVisualPathPixel(first, scaled(filament, value));
                setOuterVisualPathPixel(second, scaled(gold, value));
            }
            break;
        }
        case FinishAnimation::Applause: {
            const uint32_t cycle = now % 1800U;
            const uint16_t half = (hw::OuterCount + 1U) / 2U;
            const uint16_t inward = static_cast<uint16_t>(min<uint32_t>(half - 1U,
                (cycle % 650U) * half / 650U));
            const bool secondClap = cycle >= 760U && cycle < 1410U;
            const uint8_t strength = secondClap ? 220U : 255U;
            for (uint8_t side = 0; side < 2U; ++side) {
                const uint16_t head = side == 0U ? inward : hw::OuterCount - 1U - inward;
                for (uint8_t tail = 0; tail < 5U; ++tail) {
                    const int16_t path = side == 0U
                        ? static_cast<int16_t>(head) - tail
                        : static_cast<int16_t>(head) + tail;
                    if (path < 0 || path >= static_cast<int16_t>(hw::OuterCount)) continue;
                    setOuterVisualPathPixel(static_cast<uint16_t>(path),
                        scaled(gold, static_cast<uint8_t>(strength - tail * 38U)));
                }
            }
            if (inward >= half - 2U) {
                setOuterVisualPathPixel(half - 1U, RgbwColor(255U, 255U, 240U));
                setOuterVisualPathPixel(half, RgbwColor(255U, 255U, 240U));
            }
            break;
        }
        case FinishAnimation::PrismBloom: {
            const uint32_t cycle = now % 3400U;
            const uint16_t center = hw::OuterCount / 2U;
            const uint16_t radius = static_cast<uint16_t>(min<uint32_t>(center + 4U,
                cycle < 1900U ? cycle * (center + 4U) / 1900U : center + 4U));
            const uint8_t hold = cycle < 2600U ? 255U
                : static_cast<uint8_t>(255U - (cycle - 2600U) * 235U / 800U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                if (distance > radius) continue;
                const uint8_t saturation = static_cast<uint8_t>(min<uint16_t>(245U, distance * 16U));
                const uint8_t hue = static_cast<uint8_t>(distance * 15U + (path > center ? 20U : 150U));
                const uint8_t value = static_cast<uint8_t>(hold * (220U - min<uint16_t>(160U, distance * 6U)) / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Finish, hue, saturation, value));
            }
            break;
        }
        case FinishAnimation::PixelToast: {
            const uint32_t cycle = now % 3000U;
            const uint16_t rise = static_cast<uint16_t>(min<uint32_t>(hw::CenterCount,
                cycle < 1100U ? cycle * hw::CenterCount / 1100U : hw::CenterCount));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                if (i >= rise) continue;
                const uint8_t crisp = hash8(i * 79U + now / 210U);
                const RgbwColor color = crisp > 220U ? RgbwColor(255U, 245U, 210U) : gold;
                setSection(LedSection::Center, i,
                           scaled(color, static_cast<uint8_t>(150U + crisp / 3U)));
            }
            if (cycle >= 1100U && cycle < 1600U) {
                const uint16_t pop = static_cast<uint16_t>((cycle - 1100U) * 5U / 500U);
                if (pop < hw::LeftCount) {
                    setSection(LedSection::Left, hw::LeftCount - 1U - pop, filament);
                    setSection(LedSection::Right, pop, filament);
                }
            } else {
                fillSection(LedSection::Left, scaled(filament, 28U));
                fillSection(LedSection::Right, scaled(filament, 28U));
            }
            break;
        }
        case FinishAnimation::CrownChase: {
            const uint8_t jewel = static_cast<uint8_t>((now / 145U) % 5U);
            fillSection(LedSection::Left, scaled(gold, 45U));
            fillSection(LedSection::Right, scaled(gold, 45U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t tooth = static_cast<uint8_t>((i * 5U) / hw::CenterCount);
                const bool crownPeak = (i % 4U) == 1U || (i % 4U) == 2U;
                uint8_t value = crownPeak ? 185U : 85U;
                RgbwColor color = gold;
                if (tooth == jewel && crownPeak) {
                    value = 255U;
                    color = decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(jewel * 51U + now / 35U), 245U, 255U);
                }
                setSection(LedSection::Center, i, scaled(color, value));
            }
            const uint16_t sideMarker = static_cast<uint16_t>((now / 85U) % hw::LeftCount);
            setSection(LedSection::Left, sideMarker, RgbwColor(255U, 250U, 220U));
            setSection(LedSection::Right, hw::RightCount - 1U - sideMarker, RgbwColor(255U, 250U, 220U));
            break;
        }
        case FinishAnimation::CooldownProgress: {
            const uint8_t chamberHeat = temperaturePercent(context.chamberTempC, 20.0f, 70.0f, 25U);
            const uint8_t toolHeat = temperaturePercent(context.activeToolTempC, 30.0f, 260.0f, 35U);
            const uint8_t cooling = static_cast<uint8_t>(100U - max(chamberHeat, toolHeat));
            const RgbwColor chamberColor = temperatureColor(chamberHeat);
            const RgbwColor toolColor = temperatureColor(toolHeat);
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t chamberCoverage = progressCoverage(chamberHeat, hw::LeftCount, i);
                const uint8_t toolCoverage = progressCoverage(toolHeat, hw::RightCount, i);
                setSection(LedSection::Left, i,
                           scaled(chamberColor, chamberCoverage ? static_cast<uint8_t>(55U + chamberCoverage * 160U / 255U) : 8U));
                setSection(LedSection::Right, hw::RightCount - 1U - i,
                           scaled(toolColor, toolCoverage ? static_cast<uint8_t>(55U + toolCoverage * 160U / 255U) : 8U));
            }
            const RgbwColor ready = cooling > 70U ? green
                : decorativeHsv(LedCategory::Finish, 145U, 225U, 255U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(cooling, hw::CenterCount, i);
                setSection(LedSection::Center, i,
                           scaled(ready, coverage ? static_cast<uint8_t>(65U + coverage * 150U / 255U) : 7U));
            }
            break;
        }
        case FinishAnimation::PrintSignature: {
            fillSection(LedSection::Center, scaled(filament, 135U));
            const uint16_t head = static_cast<uint16_t>((now / 52U) % hw::OuterCount);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > head ? path - head : head - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                if (distance > 7U) continue;
                const uint8_t value = static_cast<uint8_t>(235U - distance * 29U);
                const RgbwColor signature = distance == 0U
                    ? RgbwColor(255U, 255U, 245U) : filament;
                setOuterVisualPathPixel(path, scaled(signature, value));
            }
            break;
        }
        case FinishAnimation::SmartApplause: {
            const uint8_t jobScale = static_cast<uint8_t>(min<uint32_t>(100U,
                context.printDurationSec / 180U));
            const uint32_t period = max<uint32_t>(620U, 1050U - jobScale * 4U);
            const uint32_t beat = now % period;
            const bool clap = beat < 85U || (beat >= 180U && beat < 265U);
            const uint8_t value = clap
                ? static_cast<uint8_t>(170U + jobScale * 85U / 100U) : 18U;
            fillSection(LedSection::Left, scaled(gold, value));
            fillSection(LedSection::Right, scaled(gold, value));
            const uint16_t half = hw::CenterCount / 2U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                const uint8_t centerValue = clap
                    ? static_cast<uint8_t>(max<int>(45, value - distance * 17)) : 10U;
                setSection(LedSection::Center, i, scaled(gold, centerValue));
            }
            break;
        }
        case FinishAnimation::TakeMe: {
            const uint16_t half = hw::CenterCount / 2U;
            const uint8_t step = static_cast<uint8_t>((now / 180U) % 6U);
            fillSection(LedSection::Left, scaled(green, 48U));
            fillSection(LedSection::Right, scaled(green, 48U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                const bool arrow = distance < 9U && (distance / 2U) == step;
                setSection(LedSection::Center, i,
                           scaled(green, arrow ? 245U : (distance < 3U ? 105U : 8U)));
            }
            const uint8_t sidePulse = static_cast<uint8_t>(70U + wave8(now / 42U) / 2U);
            setSection(LedSection::Left, hw::LeftCount - 1U, scaled(green, sidePulse));
            setSection(LedSection::Right, 0U, scaled(green, sidePulse));
            break;
        }
        case FinishAnimation::CoolToTouch: {
            const uint8_t toolHeat = temperaturePercent(context.activeToolTempC, 35.0f, 230.0f, 30U);
            const uint8_t bedHeat = temperaturePercent(context.bedTempC, 30.0f, 100.0f, 25U);
            const uint8_t hottest = max(toolHeat, bedHeat);
            const bool safe = hottest <= 18U;
            const RgbwColor statusColor = safe ? green : temperatureColor(hottest);
            const uint8_t pulse = safe ? static_cast<uint8_t>(70U + wave8(now / 70U) / 5U)
                : static_cast<uint8_t>(65U + wave8(now / max<uint8_t>(8U, 34U - hottest / 5U)) * 165U / 255U);
            fillSection(LedSection::Left, scaled(statusColor, pulse));
            fillSection(LedSection::Right, scaled(statusColor, pulse));
            const uint8_t safePercent = static_cast<uint8_t>(100U - hottest);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t coverage = progressCoverage(safePercent, hw::CenterCount, i);
                setSection(LedSection::Center, i,
                           scaled(statusColor, coverage ? static_cast<uint8_t>(50U + coverage * 170U / 255U) : 6U));
            }
            break;
        }
        case FinishAnimation::LastLayerGlow: {
            const uint16_t half = hw::CenterCount / 2U;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                const uint8_t ridge = wave8(static_cast<uint8_t>(now / 35U + distance * 21U));
                setSection(LedSection::Center, i,
                           scaled(filament, static_cast<uint8_t>(105U + ridge / 2U)));
            }
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t shimmerLeft = wave8(static_cast<uint8_t>(now / 29U + i * 18U));
                const uint8_t shimmerRight = wave8(static_cast<uint8_t>(now / 31U - i * 18U));
                setSection(LedSection::Left, i, scaled(filament, static_cast<uint8_t>(35U + shimmerLeft / 3U)));
                setSection(LedSection::Right, i, scaled(filament, static_cast<uint8_t>(35U + shimmerRight / 3U)));
            }
            break;
        }
        case FinishAnimation::GalleryMode: {
            const uint8_t white = static_cast<uint8_t>(85U + wave8(now / 92U) / 10U);
            fillSection(LedSection::Center, RgbwColor(white, white,
                                                     static_cast<uint8_t>(white * 9U / 10U)));
            const uint8_t side = static_cast<uint8_t>(42U + wave8(now / 73U) / 10U);
            fillSection(LedSection::Left, scaled(filament, side));
            fillSection(LedSection::Right, scaled(filament, side));
            const uint16_t reflection = static_cast<uint16_t>((now / 160U) % hw::CenterCount);
            setSection(LedSection::Center, reflection, RgbwColor(175U, 175U, 165U));
            break;
        }
        case FinishAnimation::FilamentFireworks: {
            RgbwColor palette[4];
            for (uint8_t slot = 0; slot < 4U; ++slot) {
                palette[slot] = (context.filamentColorMask & (1U << slot))
                    ? fromRgb(context.filamentColorsRgb[slot])
                    : decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(28U + slot * 61U), 240U, 255U);
                if (max(palette[slot].r, max(palette[slot].g, palette[slot].b)) < 12U) {
                    palette[slot] = decorativeHsv(LedCategory::Finish,
                        static_cast<uint8_t>(28U + slot * 61U), 240U, 255U);
                }
            }
            const uint32_t cycle = now % 2100U;
            const uint32_t epoch = now / 2100U;
            const uint8_t selected = static_cast<uint8_t>(epoch & 3U);
            const uint16_t origin = static_cast<uint16_t>(
                hash8(epoch * 151U + selected * 43U) % hw::OuterCount);
            const uint16_t radius = static_cast<uint16_t>(cycle * 19U / 1500U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t direct = path > origin ? path - origin : origin - path;
                const uint16_t distance = min<uint16_t>(direct, hw::OuterCount - direct);
                const uint16_t delta = distance > radius ? distance - radius : radius - distance;
                if (cycle >= 1500U || delta > 2U) continue;
                const uint8_t value = static_cast<uint8_t>(max<int>(20,
                    245 - static_cast<int>(cycle * 120U / 1500U) - delta * 45));
                setOuterVisualPathPixel(path, scaled(palette[selected], value));
            }
            break;
        }
        case FinishAnimation::InspectionLight: {
            const uint16_t sweep = static_cast<uint16_t>((now / 62U) % (hw::CenterCount * 2U - 2U));
            const uint16_t head = sweep < hw::CenterCount
                ? sweep : hw::CenterCount * 2U - 2U - sweep;
            fillSection(LedSection::Left, RgbwColor(65U, 65U, 60U));
            fillSection(LedSection::Right, RgbwColor(65U, 65U, 60U));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i > head ? i - head : head - i;
                const uint8_t value = distance < 4U
                    ? static_cast<uint8_t>(245U - distance * 48U) : 58U;
                setSection(LedSection::Center, i,
                           RgbwColor(value, value, static_cast<uint8_t>(value * 9U / 10U)));
            }
            break;
        }
        case FinishAnimation::QuietPride: {
            const uint8_t breath = static_cast<uint8_t>(70U + wave8(now / 88U) / 8U);
            fillSection(LedSection::Left, scaled(filament, breath));
            fillSection(LedSection::Right, scaled(filament, breath));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint8_t arch = wave8(static_cast<uint8_t>(64U +
                    i * 128U / max<uint16_t>(1U, hw::CenterCount - 1U)));
                setSection(LedSection::Center, i,
                           scaled(gold, static_cast<uint8_t>(95U + arch / 3U)));
            }
            break;
        }
        case FinishAnimation::CalmDone: {
            const uint8_t breath = static_cast<uint8_t>(48U + wave8(now / 96U) / 7U);
            const uint16_t half = hw::CenterCount / 2U;
            fillSection(LedSection::Left, scaled(filament, breath));
            fillSection(LedSection::Right, scaled(filament, breath));
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint16_t distance = i < half ? half - 1U - i : i - half;
                const uint8_t centerGlow = static_cast<uint8_t>(max<int>(54,
                    132 - static_cast<int>(distance) * 8));
                const uint8_t settle = static_cast<uint8_t>(centerGlow + breath / 4U);
                setSection(LedSection::Center, i,
                           blend(scaled(filament, settle), scaled(green, settle), 92U));
            }
            break;
        }
        case FinishAnimation::SilkUnveil: {
            const uint8_t drift = static_cast<uint8_t>(now / 31U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t foldA = wave8(static_cast<uint8_t>(drift + path * 17U));
                const uint8_t foldB = wave8(static_cast<uint8_t>(drift / 2U - path * 9U + 73U));
                const uint8_t sheen = static_cast<uint8_t>(
                    (static_cast<uint16_t>(foldA) * 3U + foldB) / 4U);
                const uint8_t value = static_cast<uint8_t>(38U + sheen * 145U / 255U);
                RgbwColor silk = scaled(filament, value);
                if (sheen > 176U) {
                    const uint8_t pearl = static_cast<uint8_t>((sheen - 176U) * 130U / 79U);
                    silk = blend(silk, RgbwColor(255U, 244U, 220U), pearl);
                }
                setOuterVisualPathPixel(path, silk);
            }
            break;
        }
        case FinishAnimation::GoldenHour: {
            const uint16_t center = (hw::OuterCount - 1U) / 2U;
            const uint8_t sun = static_cast<uint8_t>(now / 85U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                const uint8_t horizon = static_cast<uint8_t>(
                    distance * 255U / max<uint16_t>(1U, center));
                const uint8_t hue = static_cast<uint8_t>(10U + horizon * 27U / 255U + sun / 18U);
                const uint8_t glow = wave8(static_cast<uint8_t>(sun + path * 5U));
                const uint8_t value = static_cast<uint8_t>(86U + glow * 92U / 255U +
                    (255U - horizon) * 45U / 255U);
                setOuterVisualPathPixel(path,
                    decorativeHsv(LedCategory::Finish, hue, 225U, value));
            }
            break;
        }
        case FinishAnimation::Starfall: {
            const RgbwColor night = decorativeHsv(LedCategory::Finish, 166U, 190U, 12U);
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                setOuterVisualPathPixel(path, night);
            }
            const uint16_t center = (hw::OuterCount - 1U) / 2U;
            for (uint8_t star = 0; star < 5U; ++star) {
                const uint16_t travel = center + 7U;
                const uint16_t step = static_cast<uint16_t>((now / (72U + star * 11U) +
                    hash8(star * 67U)) % travel);
                if (step > center) continue;
                const uint16_t leftHead = step;
                const uint16_t rightHead = hw::OuterCount - 1U - step;
                const RgbwColor starColor = star & 1U
                    ? decorativeHsv(LedCategory::Finish, 33U, 95U, 255U)
                    : RgbwColor(220U, 235U, 255U);
                for (uint8_t tail = 0; tail < 4U; ++tail) {
                    const uint8_t value = static_cast<uint8_t>(245U - tail * 58U);
                    if (leftHead >= tail) {
                        setOuterVisualPathPixel(leftHead - tail, scaled(starColor, value));
                    }
                    if (rightHead + tail < hw::OuterCount) {
                        setOuterVisualPathPixel(rightHead + tail, scaled(starColor, value));
                    }
                }
            }
            break;
        }
        case FinishAnimation::SignatureSweep: {
            const uint32_t cycle = now % 3100U;
            const uint32_t ink = min<uint32_t>(hw::CenterCount * 255U,
                cycle * hw::CenterCount * 255U / 2200U);
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                const uint32_t start = static_cast<uint32_t>(i) * 255U;
                if (ink <= start) continue;
                const uint8_t coverage = static_cast<uint8_t>(min<uint32_t>(255U, ink - start));
                const uint16_t age = static_cast<uint16_t>(
                    min<uint32_t>(255U, (ink - start) / 2U));
                RgbwColor color = scaled(filament,
                    static_cast<uint8_t>(90U + age * 95U / 255U));
                if (coverage < 255U) color = blend(color, RgbwColor(255U, 252U, 235U), 210U);
                setSection(LedSection::Center, i, scaled(color, coverage));
            }
            const uint8_t seal = static_cast<uint8_t>(62U + wave8(now / 74U) / 5U);
            fillSection(LedSection::Left, scaled(gold, seal));
            fillSection(LedSection::Right, scaled(gold, seal));
            setSection(LedSection::Left, hw::LeftCount - 1U, scaled(filament, 210U));
            setSection(LedSection::Right, 0U, scaled(filament, 210U));
            break;
        }
        case FinishAnimation::InspectReady: {
            const uint32_t cycle = now % 6000U;
            const uint8_t phase = static_cast<uint8_t>(cycle / 1500U);
            const uint16_t phaseTime = static_cast<uint16_t>(cycle % 1500U);
            constexpr LedSection stages[3] = {
                LedSection::Left, LedSection::Center, LedSection::Right
            };
            fillSection(LedSection::Left, RgbwColor(28U, 28U, 25U));
            fillSection(LedSection::Center, RgbwColor(32U, 32U, 29U));
            fillSection(LedSection::Right, RgbwColor(28U, 28U, 25U));
            if (phase < 3U) {
                const LedSection section = stages[phase];
                const uint16_t count = sectionCount(section);
                const uint16_t head = min<uint16_t>(count - 1U,
                    static_cast<uint16_t>(phaseTime * count / 1500U));
                for (uint16_t i = 0; i < count; ++i) {
                    const uint16_t distance = i > head ? i - head : head - i;
                    if (distance > 3U) continue;
                    const uint8_t value = static_cast<uint8_t>(235U - distance * 56U);
                    setSection(section, i, RgbwColor(value, value,
                                                     static_cast<uint8_t>(value * 9U / 10U)));
                }
            } else {
                fillSection(LedSection::Left, scaled(green, 82U));
                fillSection(LedSection::Right, scaled(green, 82U));
                for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                    const bool check = (i >= 5U && i <= 9U) || (i >= 9U && i <= 16U);
                    const uint8_t value = check ? 220U : 38U;
                    setSection(LedSection::Center, i, scaled(green, value));
                }
            }
            break;
        }
        case FinishAnimation::PrintEcho: {
            const uint16_t center = (hw::OuterCount - 1U) / 2U;
            const uint16_t reach = center + 5U;
            const uint16_t phase = static_cast<uint16_t>((now / 58U) % (reach + 13U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t distance = path > center ? path - center : center - path;
                uint8_t strongest = 8U;
                bool goldEcho = false;
                for (uint8_t echo = 0; echo < 3U; ++echo) {
                    const int16_t radius = static_cast<int16_t>(phase) - echo * 7;
                    if (radius < 0) continue;
                    const uint16_t delta = distance > static_cast<uint16_t>(radius)
                        ? distance - static_cast<uint16_t>(radius)
                        : static_cast<uint16_t>(radius) - distance;
                    if (delta <= 2U) {
                        const uint8_t value = static_cast<uint8_t>(220U - echo * 40U - delta * 52U);
                        if (value > strongest) {
                            strongest = value;
                            goldEcho = (echo & 1U) != 0U;
                        }
                    }
                }
                setOuterVisualPathPixel(path, scaled(goldEcho ? gold : filament, strongest));
            }
            break;
        }
        case FinishAnimation::SoftApplause: {
            const uint32_t cycle = now % 3200U;
            const uint16_t half = hw::OuterCount / 2U;
            const uint16_t approach = static_cast<uint16_t>(
                min<uint32_t>(half, cycle * half / 2300U));
            const uint8_t fade = cycle < 2300U ? 255U
                : static_cast<uint8_t>(max<int>(0, 255 - (cycle - 2300U) * 255U / 900U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint16_t leftDistance = path > approach ? path - approach : approach - path;
                const uint16_t rightHead = hw::OuterCount - 1U - approach;
                const uint16_t rightDistance = path > rightHead ? path - rightHead : rightHead - path;
                const uint16_t distance = min(leftDistance, rightDistance);
                const uint8_t value = distance <= 5U
                    ? static_cast<uint8_t>(fade * (6U - distance) / 6U)
                    : static_cast<uint8_t>(10U * fade / 255U);
                setOuterVisualPathPixel(path, scaled(gold, value));
            }
            break;
        }
        case FinishAnimation::CooldownAura: {
            const uint8_t toolHeat = temperaturePercent(context.activeToolTempC, 35.0f, 230.0f, 30U);
            const uint8_t bedHeat = temperaturePercent(context.bedTempC, 30.0f, 100.0f, 25U);
            const uint8_t heat = max(toolHeat, bedHeat);
            const RgbwColor hot = temperatureColor(heat);
            const RgbwColor cool = decorativeHsv(LedCategory::Finish, 140U, 220U, 255U);
            const RgbwColor aura = blend(cool, hot, heat);
            const uint16_t divisor = max<uint16_t>(20U, static_cast<uint16_t>(74U - heat / 2U));
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const uint8_t breath = wave8(static_cast<uint8_t>(now / divisor + path * 6U));
                const uint8_t value = static_cast<uint8_t>(42U + breath * 100U / 255U + heat / 5U);
                setOuterVisualPathPixel(path, scaled(aura, value));
            }
            break;
        }
        case FinishAnimation::ShowcaseLoop: {
            const uint32_t cycle = now % 12000U;
            const uint8_t segment = static_cast<uint8_t>(cycle / 3000U);
            const uint8_t transition = static_cast<uint8_t>((cycle % 3000U) * 255U / 3000U);
            auto sceneColor = [&](uint8_t scene, uint16_t path) {
                const uint16_t center = (hw::OuterCount - 1U) / 2U;
                const uint16_t distance = path > center ? path - center : center - path;
                switch (scene & 3U) {
                    case 0: {
                        const uint8_t reveal = wave8(static_cast<uint8_t>(now / 34U - distance * 13U));
                        return scaled(filament, static_cast<uint8_t>(55U + reveal * 155U / 255U));
                    }
                    case 1: {
                        const uint16_t head = static_cast<uint16_t>((now / 76U) % hw::OuterCount);
                        const uint16_t direct = path > head ? path - head : head - path;
                        const uint16_t around = min<uint16_t>(direct, hw::OuterCount - direct);
                        return scaled(gold, around < 7U
                            ? static_cast<uint8_t>(235U - around * 31U) : 24U);
                    }
                    case 2: {
                        const uint8_t hue = static_cast<uint8_t>(now / 25U + path * 256U / hw::OuterCount);
                        const uint8_t value = static_cast<uint8_t>(85U + wave8(hue + path * 7U) / 3U);
                        return decorativeHsv(LedCategory::Finish, hue, 235U, value);
                    }
                    case 3:
                    default: {
                        const uint8_t gallery = static_cast<uint8_t>(75U +
                            wave8(static_cast<uint8_t>(now / 88U + path * 3U)) / 8U);
                        return blend(scaled(filament, gallery),
                                     RgbwColor(235U, 225U, 200U), 105U);
                    }
                }
            };
            for (uint16_t path = 0; path < hw::OuterCount; ++path) {
                const RgbwColor current = sceneColor(segment, path);
                const RgbwColor next = sceneColor(static_cast<uint8_t>(segment + 1U), path);
                const uint8_t eased = wave8(static_cast<uint8_t>(transition / 2U));
                setOuterVisualPathPixel(path, blend(current, next, eased));
            }
            break;
        }
        case FinishAnimation::Count:
        default:
            break;
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
