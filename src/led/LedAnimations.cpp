#include "LedAnimations.h"

namespace coronet {

namespace {

constexpr const char* IdleNames[] = {
    "Slow Orbit", "Ember Breath", "Horizon", "Quiet Spectrum",
};

constexpr const char* PrintNames[] = {
    "Progress Bar", "Laser", "Wave", "Thermal", "Stripes",
    "Progress Pulse", "Comet", "Active Section", "Running", "Breathe",
    "Wipe", "Shimmer", "Bicolor", "Thermometer", "Snake",
    "Rainbow Progress", "Heartbeat", "DNA Helix", "Pixel Rain", "Orbit",
};

constexpr const char* PauseNames[] = {
    "Gentle Hold", "Amber Pulse", "Frozen Progress", "Pause Beacon",
};

constexpr const char* ErrorNames[] = {
    "Signal Pulse", "Split Alarm", "Red Wave", "Fault Beacon",
};

constexpr const char* FinishNames[] = {
    "Completion Bloom", "Color Release", "Finish Sweep", "Soft Celebration",
};

constexpr const char* OtherNames[] = {
    "Synth Current", "Ambient Drift", "Prism Field", "Color Theater",
};

template <size_t N>
constexpr uint8_t arrayCount(const char* const (&)[N]) {
    static_assert(N <= 255U, "LED animation count must fit in settings storage");
    return static_cast<uint8_t>(N);
}

}

uint8_t ledAnimationCount(LedCategory category) {
    switch (category) {
        case LedCategory::Idle: return arrayCount(IdleNames);
        case LedCategory::Print: return arrayCount(PrintNames);
        case LedCategory::Pause: return arrayCount(PauseNames);
        case LedCategory::Error: return arrayCount(ErrorNames);
        case LedCategory::Finish: return arrayCount(FinishNames);
        case LedCategory::Other: return arrayCount(OtherNames);
        case LedCategory::Count:
        default: return 1U;
    }
}

uint8_t normalizeLedAnimation(LedCategory category, uint8_t animation) {
    const uint8_t count = ledAnimationCount(category);
    return count ? static_cast<uint8_t>(animation % count) : 0U;
}

const char* ledAnimationName(LedCategory category, uint8_t animation) {
    animation = normalizeLedAnimation(category, animation);
    switch (category) {
        case LedCategory::Idle: return IdleNames[animation];
        case LedCategory::Print: return PrintNames[animation];
        case LedCategory::Pause: return PauseNames[animation];
        case LedCategory::Error: return ErrorNames[animation];
        case LedCategory::Finish: return FinishNames[animation];
        case LedCategory::Other: return OtherNames[animation];
        case LedCategory::Count:
        default: return "Unknown";
    }
}

static_assert(arrayCount(PrintNames) == static_cast<uint8_t>(PrintAnimation::Count));

}
