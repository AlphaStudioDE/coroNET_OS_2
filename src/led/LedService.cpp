#include "LedService.h"

#include <esp_heap_caps.h>

#include "../config/HardwareConfig.h"
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

LedService gLedService;

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

RgbwColor blend(const RgbwColor& a, const RgbwColor& b, uint8_t amount) {
    const uint16_t inverse = 255U - amount;
    return RgbwColor(
        static_cast<uint8_t>((static_cast<uint16_t>(a.r) * inverse + static_cast<uint16_t>(b.r) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.g) * inverse + static_cast<uint16_t>(b.g) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.b) * inverse + static_cast<uint16_t>(b.b) * amount + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(a.w) * inverse + static_cast<uint16_t>(b.w) * amount + 127U) / 255U));
}

RgbwColor scaled(const RgbwColor& color, uint8_t scale) {
    return RgbwColor(
        static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 127U) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.w) * scale + 127U) / 255U));
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
    spi_->writeBytes(txBuffer_, hw::LedCount * 16U);
    delayMicroseconds(100);

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry, "coronet-led", TaskStackBytes, this, TaskPriority, &task_, TaskCore);
    if (created != pdPASS) {
        Serial.println("[led] task creation failed");
        return;
    }

    bootStartedMs_ = millis();
    bootActive_ = true;
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
    previewAnimation_ = animation;
    previewUntilMs_ = millis() + durationMs;
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
    Serial.printf("[led] ready=%u boot=%u preview=%u shows=%lu skipped=%lu frames=%lu dropped=%lu\n",
                  started_ ? 1U : 0U, bootActive_ ? 1U : 0U, previewActive_ ? 1U : 0U,
                  static_cast<unsigned long>(shows_), static_cast<unsigned long>(skippedShows_),
                  static_cast<unsigned long>(state().ledFrameCount),
                  static_cast<unsigned long>(state().ledDroppedFrames));
}

void LedService::taskEntry(void* context) {
    static_cast<LedService*>(context)->taskLoop();
}

void LedService::taskLoop() {
    TickType_t wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(FrameIntervalMs);
    for (;;) {
        const uint32_t now = millis();
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

    if (!settings.ledEnabled || quietSuppressesLeds(settings, system)) {
        bootActive_ = false;
        smoothAndShow();
        return;
    }

    const uint32_t bootElapsed = now - bootStartedMs_;
    if (bootActive_ && bootElapsed < BootDurationMs) {
        renderBoot(bootElapsed);
    } else {
        bootActive_ = false;
        const bool preview = previewActive_ && static_cast<int32_t>(previewUntilMs_ - now) > 0;
        const LedCategory category = preview ? previewCategory_
                                             : (settings.ledOtherMode ? LedCategory::Other
                                                                      : categoryForState(system));
        const uint8_t animation = preview ? previewAnimation_
                                          : settings.ledAnimation[static_cast<uint8_t>(category)];
        renderCategory(category, animation, now,
                       preview ? 62U : system.printProgress,
                       preview ? 42.0f : system.chamberTempC,
                       preview ? 0xFF7A00UL : system.filamentColorRgb);
        applyInsidePolicy();
    }

    applyOutputPolicies();
    smoothAndShow(bootElapsed < 300U);
}

void LedService::renderBoot(uint32_t elapsedMs) {
    const uint8_t timeHue = static_cast<uint8_t>(elapsedMs / 20U);
    if (elapsedMs < 7000U) {
        const uint8_t reveal = static_cast<uint8_t>(min<uint32_t>(255U, elapsedMs * 255U / 7000U));
        const uint8_t breath = static_cast<uint8_t>(24U + wave8(static_cast<uint8_t>(elapsedMs / 18U)) / 3U);
        for (uint16_t i = 0; i < hw::InsideCount; ++i) {
            const uint8_t hue = static_cast<uint8_t>(timeHue + i * 13U);
            setSection(LedSection::Inside, i, hsv(hue, 245, static_cast<uint8_t>((breath * reveal) / 255U)));
        }
        const uint16_t scanner = static_cast<uint16_t>((elapsedMs / 120U) % hw::OuterCount);
        for (uint16_t path = 0; path < hw::OuterCount; ++path) {
            const uint16_t distance = path > scanner ? path - scanner : scanner - path;
            if (distance > 3U) continue;
            const uint8_t value = static_cast<uint8_t>((4U - distance) * reveal / 5U);
            if (path < hw::RightCount) setSection(LedSection::Right, path, hsv(timeHue, 250, value));
            else if (path < hw::RightCount + hw::CenterCount) {
                setSection(LedSection::Center, path - hw::RightCount, hsv(timeHue + 24U, 245, value));
            } else setSection(LedSection::Left, path - hw::RightCount - hw::CenterCount,
                              hsv(timeHue + 48U, 250, value));
        }
        return;
    }

    if (elapsedMs < 15000U) {
        const uint32_t local = elapsedMs - 7000U;
        for (uint8_t sectionIndex = 0; sectionIndex < 4U; ++sectionIndex) {
            const LedSection section = static_cast<LedSection>(sectionIndex);
            const uint16_t count = sectionCount(section);
            for (uint16_t i = 0; i < count; ++i) {
                const uint8_t phase = static_cast<uint8_t>(local / 13U + i * 18U + sectionIndex * 29U);
                const uint8_t hue = static_cast<uint8_t>(timeHue + i * 9U + sectionIndex * 34U);
                const uint8_t value = static_cast<uint8_t>(35U + wave8(phase) * 170U / 255U);
                setSection(section, i, hsv(hue, 245, value));
            }
        }
        return;
    }

    if (elapsedMs < 30000U) {
        const uint32_t local = elapsedMs - 15000U;
        for (uint8_t sectionIndex = 0; sectionIndex < 4U; ++sectionIndex) {
            const LedSection section = static_cast<LedSection>(sectionIndex);
            const uint16_t count = sectionCount(section);
            for (uint16_t i = 0; i < count; ++i) {
                const uint8_t hue = static_cast<uint8_t>(i * 256U / count + local / 15U + sectionIndex * 21U);
                const uint8_t wave = wave8(static_cast<uint8_t>(local / 9U + i * 22U));
                setSection(section, i, hsv(hue, 235, static_cast<uint8_t>(90U + wave * 165U / 255U)));
            }
        }
        return;
    }

    const uint8_t handoff = static_cast<uint8_t>(min<uint32_t>(255U, (elapsedMs - 30000U) * 255U / 5000U));
    RgbwColor bootFrame[hw::LedCount] = {};
    for (uint16_t i = 0; i < hw::LedCount; ++i) {
        const uint8_t hue = static_cast<uint8_t>(i * 7U + elapsedMs / 18U);
        bootFrame[i] = hsv(hue, 220, static_cast<uint8_t>(150U + wave8(hue) / 3U));
    }
    const SystemState& system = state();
    renderCategory(categoryForState(system),
                   settingsService().settings().ledAnimation[static_cast<uint8_t>(categoryForState(system))],
                   millis(), system.printProgress, system.chamberTempC, system.filamentColorRgb);
    applyInsidePolicy();
    for (uint16_t i = 0; i < hw::LedCount; ++i) targetFrame_[i] = blend(bootFrame[i], targetFrame_[i], handoff);
}

void LedService::renderCategory(LedCategory category, uint8_t animation, uint32_t now,
                                uint8_t progress, float chamberTempC, uint32_t filamentRgb) {
    switch (category) {
        case LedCategory::Print: renderPrint(animation, now, progress, chamberTempC, filamentRgb); break;
        case LedCategory::Pause: renderPause(animation, now, progress, filamentRgb); break;
        case LedCategory::Error: renderError(animation, now); break;
        case LedCategory::Finish: renderFinish(animation, now, filamentRgb); break;
        case LedCategory::Other: renderOther(animation, now); break;
        case LedCategory::Idle:
        default: renderIdle(animation, now); break;
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
                if (path < hw::RightCount) setSection(LedSection::Right, path, color);
                else if (path < hw::RightCount + hw::CenterCount) setSection(LedSection::Center, path - hw::RightCount, color);
                else setSection(LedSection::Left, path - hw::RightCount - hw::CenterCount, color);
            }
            break;
        }
        case 2: {
            const float temp = isnan(state().chamberTempC) ? 25.0f : state().chamberTempC;
            const uint8_t hot = clampByte(static_cast<int>((temp - 20.0f) * 255.0f / 40.0f));
            const RgbwColor thermal = blend(RgbwColor(0, 70, 255), RgbwColor(255, 20, 0), hot);
            for (uint8_t sectionIndex = 0; sectionIndex < 3U; ++sectionIndex) {
                const LedSection section = static_cast<LedSection>(sectionIndex);
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
                const LedSection section = static_cast<LedSection>(sectionIndex);
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
                const LedSection section = static_cast<LedSection>(sectionIndex);
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

void LedService::renderPrint(uint8_t animation, uint32_t now, uint8_t progress,
                             float chamberTempC, uint32_t filamentRgb) {
    progress = min<uint8_t>(progress, 100U);
    const RgbwColor filament = fromRgb(filamentRgb);
    fillSection(LedSection::Left, filament);
    fillSection(LedSection::Right, filament);
    const uint16_t lit = static_cast<uint16_t>((progress * hw::CenterCount + 99U) / 100U);

    switch (animation % 4U) {
        case 1: {
            const bool redFilament = filament.r > 150U && filament.r > filament.g * 3U / 2U && filament.r > filament.b * 3U / 2U;
            const RgbwColor laser = redFilament ? RgbwColor(0, 255, 40) : RgbwColor(255, 0, 0);
            const uint16_t head = lit ? static_cast<uint16_t>((now / 55U) % lit) : 0U;
            for (uint16_t i = 0; i < lit; ++i) {
                const uint16_t distance = i > head ? i - head : head - i;
                setSection(LedSection::Center, i, scaled(laser, distance == 0 ? 255 : distance == 1 ? 90 : 18));
            }
            break;
        }
        case 2: {
            const float temperature = isnan(chamberTempC) ? 20.0f : chamberTempC;
            const uint8_t balance = clampByte(static_cast<int>((temperature - 20.0f) * 255.0f / 40.0f));
            for (uint16_t i = 0; i < hw::LeftCount; ++i) {
                const uint8_t point = static_cast<uint8_t>(i * 255U / (hw::LeftCount - 1U));
                const RgbwColor color = blend(RgbwColor(0, 60, 255), RgbwColor(255, 12, 0),
                                               static_cast<uint8_t>((point + balance) / 2U));
                setSection(LedSection::Left, i, color);
                setSection(LedSection::Right, i, color);
            }
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                setSection(LedSection::Center, i, scaled(filament, i < lit ? 235 : 18));
            }
            break;
        }
        case 3: {
            RgbwColor opposite = complementary(filament);
            const bool rainbowFallback = opposite.r == 0 && opposite.g == 0 && opposite.b == 0;
            for (uint16_t i = 0; i < lit; ++i) {
                const uint8_t value = static_cast<uint8_t>(70U + wave8(static_cast<uint8_t>(now / 12U + i * 24U)) * 185U / 255U);
                setSection(LedSection::Center, i,
                           rainbowFallback ? hsv(static_cast<uint8_t>(now / 18U + i * 13U), 255, value)
                                           : scaled(opposite, value));
            }
            break;
        }
        case 0:
        default: {
            const RgbwColor opposite = complementary(filament);
            const bool rainbowFallback = opposite.r == 0 && opposite.g == 0 && opposite.b == 0;
            for (uint16_t i = 0; i < hw::CenterCount; ++i) {
                if (i >= lit) continue;
                setSection(LedSection::Center, i,
                           rainbowFallback ? hsv(static_cast<uint8_t>(i * 13U + now / 20U), 255, 220)
                                           : opposite);
            }
            break;
        }
    }
}

void LedService::renderPause(uint8_t animation, uint32_t now, uint8_t progress, uint32_t filamentRgb) {
    const RgbwColor filament = fromRgb(filamentRgb);
    const uint8_t pulse = static_cast<uint8_t>(35U + wave8(static_cast<uint8_t>(now / (animation % 3U == 0 ? 18U : 30U))) * 180U / 255U);
    fillSection(LedSection::Left, scaled(RgbwColor(255, 90, 0), pulse));
    fillSection(LedSection::Right, scaled(RgbwColor(255, 90, 0), pulse));
    const uint16_t marker = min<uint16_t>(hw::CenterCount - 1U, progress * hw::CenterCount / 100U);
    for (uint16_t i = 0; i < hw::CenterCount; ++i) {
        const uint16_t distance = i > marker ? i - marker : marker - i;
        setSection(LedSection::Center, i, distance <= 1U ? scaled(filament, pulse) : RgbwColor(12, 8, 2));
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
        const LedSection section = static_cast<LedSection>(sectionIndex);
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
        const LedSection section = static_cast<LedSection>(sectionIndex);
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
        const uint8_t scaleValue = static_cast<uint8_t>(static_cast<uint16_t>(average) * 255U / 100U);
        for (uint16_t i = 0; i < hw::LedCount; ++i) targetFrame_[i] = scaled(targetFrame_[i], scaleValue);
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
        const RgbwColor next = blend(currentFrame_[i], targetFrame_[i], blendAmount);
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
    spi_->writeBytes(txBuffer_, hw::LedCount * 16U);
    delayMicroseconds(100);
    ++shows_;
    return true;
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
    for (uint16_t i = 0; i < hw::LedCount; ++i) {
        const RgbwColor& color = currentFrame_[i];
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
    setPhysical(sectionPhysicalIndex(section, logical), color);
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
    const bool mirror = settingsService().settings().mirrorLedLayout;
    if (mirror) {
        switch (section) {
            case LedSection::Left: return hw::RightStart + logical;
            case LedSection::Center: return hw::CenterStart + logical;
            case LedSection::Right: return hw::LeftEnd - logical;
            case LedSection::Inside: return hw::InsideStart + logical;
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

RgbwColor LedService::sectionColor(LedSection section, const RgbwColor& color) const {
    const uint8_t percent = settingsService().settings().ledBrightness[static_cast<uint8_t>(section)];
    return scaled(color, static_cast<uint8_t>(static_cast<uint16_t>(percent) * 255U / 100U));
}

RgbwColor LedService::decorativeHsv(LedCategory category, uint8_t hue,
                                    uint8_t saturation, uint8_t value) const {
    const int16_t degrees = settingsService().settings().ledColorRemixDegrees[static_cast<uint8_t>(category)];
    const int16_t shift = static_cast<int16_t>(degrees * 256L / 360L);
    return hsv(static_cast<uint8_t>(hue + shift), saturation, value);
}

}
