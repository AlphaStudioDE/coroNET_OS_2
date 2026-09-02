#include "LedAnimations.h"

namespace coronet {

namespace {

constexpr const char* IdleNames[] = {
    "Rainbow", "Fireplace", "Ocean", "Star Pulse", "Meteor",
    "Twinkle", "Larson", "Lava", "Gradient", "Plasma",
    "Section Breathe", "Snow", "Color Wipe", "Moonlight", "Tetris",
    "Running", "Bubbles", "Drift", "Candle", "Starfield",
    "Aurora Ribbon", "Rainbow Glitter", "Soft Comet", "Kaleidoscope", "Breathing Orbit",
    "Pixel Fireflies", "Cosmic Dust", "Theater Glow", "Tidal Pool", "Neon Drift",
    "Ready Breath", "Ambient Clock", "Temp Idle", "Last Print Echo", "WiFi Beacon",
    "Sleepy Core", "Material Shelf", "Status Ring", "Chamber Lantern", "Ready Split",
    "Calm Tide", "Zen Garden", "Dusk Horizon", "Silk Flow", "Northern Sleep",
    "Dew Sparks", "Lamp Glow", "Cloud Drift", "Quiet Comet", "Section Calm",
};

constexpr const char* PrintNames[] = {
    "Progress Bar", "Laser", "Wave", "Thermal", "Stripes",
    "Progress Pulse", "Comet", "Active Section", "Running", "Breathe",
    "Wipe", "Shimmer", "Bicolor", "Thermometer", "Snake",
    "Rainbow Progress", "Heartbeat", "DNA Helix", "Pixel Rain", "Orbit",
    "Extruder Spark", "Layer Scan", "Heat Ripple", "Filament Comets", "Progress Theater",
    "Nozzle Trace", "Build Plate", "Micro Steps", "Flow Wave", "Toolhead Orbit",
    "Thermal Balance", "Material Core", "Heat Soak", "Stability Monitor", "Layer Engine",
    "Time Tunnel", "Chamber Aura", "Filament Flow", "Process Stack", "Health Beacon",
    "Finish Pressure", "Dual Temp Meter", "Layer Pulse", "Toolpath Echo", "Thermal Ribbon",
    "Infill Grid", "Filament Beads", "Time Flow", "Stepper Ticks", "Calm Build",
    "Quality Guard", "Nozzle Heat", "Layer Fill",
};

constexpr const char* PauseNames[] = {
    "Amber", "Hazard", "Freeze", "Radar", "Heartbeat",
    "Progress Bar", "Crossfade", "Phase", "Yellow-White", "Watchful Eyes",
    "Amber Strobe", "Zigzag", "Neon", "Hourglass", "Amber Wave",
    "Bounce", "Slow Comet", "Spinner", "Morse Wait", "Blue Breathe",
    "Soft Hold", "Amber Theater", "Breathing Dots", "Waiting Ripple", "Parking Lights",
    "Dim Sparks", "Slow Scan", "Frozen Gold", "Clock Tick", "Calm Orbit",
    "Holding Pattern", "Breathing Amber", "Resume Gate", "Temp Keepalive", "Soft Attention",
    "Operator Wait", "Frozen Layer", "Filament Hold", "Do Not Touch", "Heat Hold Split",
    "Calm Down", "Still Water", "Soft Lantern", "Hold Orb", "Suspended Layer",
    "Gentle Reminder", "Breath Gate", "Waiting Room", "Tool Park", "Resume Ramp",
};

constexpr const char* ErrorNames[] = {
    "Blink", "SOS", "Alarm", "Critical", "Police",
    "Red Breathe", "Heartbeat", "Strobe", "Red Wave", "Xenon",
    "Siren", "Thunder", "Countdown", "Glitch", "Alarm Chase",
    "Danger Stripe", "Pulse Alert", "Redout", "Emergency", "Meltdown",
    "Crash", "Red Theater", "Fault Ripple", "Hot Zone", "Panic Comets",
    "Lockdown", "Warning Ticks", "Breach Scan", "Fault Sparks", "Red Juggle",
    "Evacuate", "Cause Hint", "Stack Light", "Smart Heartbeat", "Location Split",
    "Blackout Flash", "Recovery Wait", "Siren Scan", "Diagnostic Bits", "Service Beacon",
    "Safe Shutdown", "Calm Alert", "Fault Locator", "Thermal Cut", "Network Lost",
    "Service Code", "Containment", "Safe Breath", "Escalation", "Repair Beacon",
    "Cooling Alarm",
};

constexpr const char* FinishNames[] = {
    "Sweep", "Rainbow", "Pulse", "Filament", "Fireworks",
    "Curtain", "Confetti", "Gold Rain", "Strobe Party", "Bouncing Balls",
    "Rainbow Explosion", "Disco", "Heart", "Color Spiral", "Sparkle",
    "Champagne", "Wipe Out", "Fill", "Waterfall", "Starburst",
    "Victory Lap", "Gold Theater", "Ribbon Dance", "Trophy Glow", "Star Glitter",
    "Dual Comets", "Applause", "Prism Bloom", "Pixel Toast", "Crown Chase",
    "Cooldown Progress", "Print Signature", "Smart Applause", "Take Me", "Cool To Touch",
    "Last Layer Glow", "Gallery Mode", "Filament Fireworks", "Inspection Light", "Quiet Pride",
    "Calm Done", "Silk Unveil", "Golden Hour", "Starfall", "Signature Sweep",
    "Inspect Ready", "Print Echo", "Soft Applause", "Cooldown Aura", "Showcase Loop",
};

constexpr const char* OtherNames[] = {
    "Matrix", "Candle", "Static Rainbow", "Neon Club", "Synthwave",
    "Jellyfish", "Snow", "Sunset", "Volcano", "Techno",
    "Dragon Blood", "Aurora", "Cyberpunk", "Nebula", "Submarine",
    "Pride", "Plasma", "Bouncing Balls", "Cop Car", "Strobe Party",
    "Sunrise", "Ocean Depth", "Radiation", "Pastel", "Electric",
    "Rainbow Pulse", "Carnival", "Neon Sign", "Motion Detect", "Retro TV",
    "Crystal", "Fire and Ice", "Laser Grid", "Galaxy Spin", "Comet Twins",
    "Deep Sea Pulse", "Solar Wind", "Pixel Circus", "Mint Breeze", "Ruby Scan",
    "Arcade Chase", "Stardust", "Ice Cave", "Firework Trail", "Chroma Ring",
    "Ghost Light", "Toxic Wave", "Copper Spark", "Blueprint", "Magma Flow",
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

static_assert(arrayCount(IdleNames) == static_cast<uint8_t>(IdleAnimation::Count));
static_assert(arrayCount(PrintNames) == static_cast<uint8_t>(PrintAnimation::Count));
static_assert(arrayCount(PauseNames) == static_cast<uint8_t>(PauseAnimation::Count));
static_assert(arrayCount(ErrorNames) == static_cast<uint8_t>(ErrorAnimation::Count));
static_assert(arrayCount(FinishNames) == static_cast<uint8_t>(FinishAnimation::Count));
static_assert(arrayCount(OtherNames) == static_cast<uint8_t>(OtherAnimation::Count));

}
