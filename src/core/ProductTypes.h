#pragma once

#include <stdint.h>

namespace coronet {

enum class LedSection : uint8_t {
    Right = 0,
    Center,
    Left,
    Inside,
    Count,
};

enum class LedCategory : uint8_t {
    Idle = 0,
    Print,
    Pause,
    Error,
    Finish,
    Other,
    Count,
};

enum class InsideColorStyle : uint8_t {
    White = 0,
    Ambient,
};

enum class SoundScenario : uint8_t {
    Start = 0,
    Finish,
    Error,
    Pause,
    Idle,
    Count,
};

enum class VentMode : uint8_t {
    Automatic = 0,
    CavityTarget,
    Manual,
};

enum class PandaBreathMode : uint8_t {
    Off = 0,
    Automatic,
    PreheatHold,
    Tempering,
    ForcedOn,
    FilamentDrying,
    Count,
};

enum class PandaWorkflowPhase : uint8_t {
    Idle = 0,
    WaitingForPrint,
    Preheating,
    Holding,
    PrintHold,
    Tempering,
    Drying,
    Complete,
    Fault,
};

enum class PandaDryPreset : uint8_t {
    Pla = 0,
    Petg,
    Custom,
    Count,
};

enum class ScreenSaverMode : uint8_t {
    Disabled = 0,
    DisplayOff,
    Clock,
};

enum class ClockStyle : uint8_t {
    Digital = 0,
    Retro,
    Analog,
    LinearHorizon,
    Bauhaus,
    DotMatrix,
    Arc,
    Count,
};

enum class QuietTarget : uint8_t {
    Off = 0,
    Sound,
    Leds,
    SoundAndLeds,
};

struct RgbwColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t w = 0;

    constexpr RgbwColor() = default;
    constexpr RgbwColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0)
        : r(red), g(green), b(blue), w(white) {}
};

constexpr uint8_t enumCount(LedSection) {
    return static_cast<uint8_t>(LedSection::Count);
}

constexpr uint8_t enumCount(LedCategory) {
    return static_cast<uint8_t>(LedCategory::Count);
}

constexpr uint8_t enumCount(SoundScenario) {
    return static_cast<uint8_t>(SoundScenario::Count);
}

}
