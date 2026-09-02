#pragma once

#include <Arduino.h>

#include "../core/ProductTypes.h"

namespace coronet {

enum class PrintAnimation : uint8_t {
    ProgressBar = 0,
    Laser,
    Wave,
    Thermal,
    Stripes,
    ProgressPulse,
    Comet,
    ActiveSection,
    Running,
    Breathe,
    Wipe,
    Shimmer,
    Bicolor,
    Thermometer,
    Snake,
    RainbowProgress,
    Heartbeat,
    DnaHelix,
    PixelRain,
    Orbit,
    ExtruderSpark,
    LayerScan,
    HeatRipple,
    FilamentComets,
    ProgressTheater,
    NozzleTrace,
    BuildPlate,
    MicroSteps,
    FlowWave,
    ToolheadOrbit,
    ThermalBalance,
    MaterialCore,
    HeatSoak,
    StabilityMonitor,
    LayerEngine,
    TimeTunnel,
    ChamberAura,
    FilamentFlow,
    ProcessStack,
    HealthBeacon,
    FinishPressure,
    DualTempMeter,
    LayerPulse,
    ToolpathEcho,
    ThermalRibbon,
    InfillGrid,
    FilamentBeads,
    TimeFlow,
    StepperTicks,
    CalmBuild,
    QualityGuard,
    NozzleHeat,
    LayerFill,
    Count,
};

enum class PauseAnimation : uint8_t {
    Amber = 0,
    Hazard,
    Freeze,
    Radar,
    Heartbeat,
    ProgressBar,
    Crossfade,
    Phase,
    YellowWhite,
    WatchfulEyes,
    AmberStrobe,
    Zigzag,
    Neon,
    Hourglass,
    AmberWave,
    Bounce,
    SlowComet,
    Spinner,
    MorseWait,
    BlueBreathe,
    SoftHold,
    AmberTheater,
    BreathingDots,
    WaitingRipple,
    ParkingLights,
    DimSparks,
    SlowScan,
    FrozenGold,
    ClockTick,
    CalmOrbit,
    HoldingPattern,
    BreathingAmber,
    ResumeGate,
    TempKeepalive,
    SoftAttention,
    OperatorWait,
    FrozenLayer,
    FilamentHold,
    DoNotTouch,
    HeatHoldSplit,
    CalmDown,
    StillWater,
    SoftLantern,
    HoldOrb,
    SuspendedLayer,
    GentleReminder,
    BreathGate,
    WaitingRoom,
    ToolPark,
    ResumeRamp,
    Count,
};

enum class ErrorAnimation : uint8_t {
    Blink = 0,
    Sos,
    Alarm,
    Critical,
    Police,
    RedBreathe,
    Heartbeat,
    Strobe,
    RedWave,
    Xenon,
    Siren,
    Thunder,
    Countdown,
    Glitch,
    AlarmChase,
    DangerStripe,
    PulseAlert,
    Redout,
    Emergency,
    Meltdown,
    Count,
};

constexpr uint32_t SnakeFinishDurationMs = 1400U;

struct LedAnimationContext {
    uint32_t nowMs = 0;
    uint8_t progress = 0;
    uint8_t activeTool = 0;
    float activeToolTempC = NAN;
    float bedTempC = NAN;
    float chamberTempC = NAN;
    uint32_t filamentRgb = 0xFFFFFF;
    uint32_t filamentColorsRgb[4] = {};
    uint8_t filamentColorMask = 0;
    uint32_t printDurationSec = 0;
    uint32_t printEtaSec = 0;
    uint32_t printerTelemetryAgeMs = UINT32_MAX;
    bool printerOnline = false;
    bool ventFailsafe = false;
    bool preview = false;
    bool finishing = false;
};

uint8_t ledAnimationCount(LedCategory category);
const char* ledAnimationName(LedCategory category, uint8_t animation);
uint8_t normalizeLedAnimation(LedCategory category, uint8_t animation);

}
