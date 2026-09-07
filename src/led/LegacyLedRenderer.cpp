#include "LegacyLedRenderer.h"

#include <math.h>
#include <string.h>

namespace coronet {
namespace legacy {

struct RgbColor {
    uint8_t R = 0;
    uint8_t G = 0;
    uint8_t B = 0;

    constexpr RgbColor() = default;
    constexpr RgbColor(uint8_t red, uint8_t green, uint8_t blue)
        : R(red), G(green), B(blue) {}
};

struct RgbwColor {
    uint8_t R = 0;
    uint8_t G = 0;
    uint8_t B = 0;
    uint8_t W = 0;

    constexpr RgbwColor() = default;
    constexpr RgbwColor(uint8_t red, uint8_t green, uint8_t blue,
                        uint8_t white = 0)
        : R(red), G(green), B(blue), W(white) {}
};

enum FinishAnimation {
    FINISH_SWEEP    = 0,
    FINISH_RAINBOW  = 1,
    FINISH_PULSE    = 2,
    FINISH_FILAMENT = 3,
    FINISH_FIREWORKS = 4,
    FINISH_CURTAIN   = 5,
    FINISH_CONFETTI  = 6,
    FINISH_GOLD_RAIN = 7,
    FINISH_STROBE_PARTY = 8,
    FINISH_BOUNCING_BALLS = 9,
    FINISH_RAINBOW_EXPLODE = 10,
    FINISH_DISCO    = 11,
    FINISH_HEART    = 12,
    FINISH_COLOR_SPIRAL = 13,
    FINISH_SPARKLE     = 14,
    FINISH_CHAMPAGNE   = 15,
    FINISH_WIPE_OUT    = 16,
    FINISH_FILL_PROG   = 17,
    FINISH_WATERFALL   = 18,
    FINISH_STARBURST   = 19,
    FINISH_VICTORY_LAP = 20,
    FINISH_GOLD_THEATER = 21,
    FINISH_RIBBON_DANCE = 22,
    FINISH_TROPHY_GLOW = 23,
    FINISH_STAR_GLITTER = 24,
    FINISH_DUAL_COMETS = 25,
    FINISH_APPLAUSE = 26,
    FINISH_PRISM_BLOOM = 27,
    FINISH_PIXEL_TOAST = 28,
    FINISH_CROWN_CHASE = 29,
    FINISH_COOLDOWN_PROGRESS = 30,
    FINISH_PRINT_SIGNATURE = 31,
    FINISH_APPLAUSE_SMART = 32,
    FINISH_TAKE_ME = 33,
    FINISH_COOL_TO_TOUCH = 34,
    FINISH_LAST_LAYER_GLOW = 35,
    FINISH_GALLERY_MODE = 36,
    FINISH_FILAMENT_FIREWORKS = 37,
    FINISH_INSPECTION_LIGHT = 38,
    FINISH_QUIET_PRIDE = 39,
    FINISH_CALM_DONE = 40,
    FINISH_SILK_UNVEIL = 41,
    FINISH_GOLDEN_HOUR = 42,
    FINISH_STARFALL = 43,
    FINISH_SIGNATURE_SWEEP = 44,
    FINISH_INSPECT_READY = 45,
    FINISH_PRINT_ECHO = 46,
    FINISH_SOFT_APPLAUSE = 47,
    FINISH_COOLDOWN_AURA = 48,
    FINISH_SHOWCASE_LOOP = 49,
    FINISH_COUNT       = 50
};

enum IdleAnimation {
    IDLE_RAINBOW     = 0,
    IDLE_FIRE        = 1,
    IDLE_OCEAN       = 2,
    IDLE_PULSE_STAR  = 3,
    IDLE_METEOR      = 4,
    IDLE_TWINKLE     = 5,
    IDLE_LARSON      = 6,
    IDLE_LAVA        = 7,
    IDLE_GRADIENT    = 8,
    IDLE_PLASMA      = 9,
    IDLE_PHASE_BREATHE = 10,
    IDLE_SNOW        = 11,
    IDLE_COLOR_WIPE  = 12,
    IDLE_MOONLIGHT   = 13,
    IDLE_TETRIS      = 14,
    IDLE_RUNNING     = 15,
    IDLE_BUBBLES     = 16,
    IDLE_DRIFT       = 17,
    IDLE_CANDLE_WARM = 18,
    IDLE_STARFIELD   = 19,
    IDLE_AURORA_RIBBON = 20,
    IDLE_RAINBOW_GLITTER = 21,
    IDLE_SOFT_COMET = 22,
    IDLE_KALEIDOSCOPE = 23,
    IDLE_BREATHING_ORBIT = 24,
    IDLE_PIXEL_FIRELIES = 25,
    IDLE_COSMIC_DUST = 26,
    IDLE_THEATER_GLOW = 27,
    IDLE_TIDAL_POOL = 28,
    IDLE_NEON_DRIFT = 29,
    IDLE_READY_BREATH = 30,
    IDLE_AMBIENT_CLOCK = 31,
    IDLE_TEMPERATURE_IDLE = 32,
    IDLE_LAST_PRINT_ECHO = 33,
    IDLE_WIFI_BEACON = 34,
    IDLE_SLEEPY_CORE = 35,
    IDLE_MATERIAL_SHELF = 36,
    IDLE_STATUS_RING = 37,
    IDLE_CHAMBER_LANTERN = 38,
    IDLE_PRINT_READY_SPLIT = 39,
    IDLE_CALM_TIDE = 40,
    IDLE_ZEN_GARDEN = 41,
    IDLE_DUSK_HORIZON = 42,
    IDLE_SILK_FLOW = 43,
    IDLE_NORTHERN_SLEEP = 44,
    IDLE_DEW_SPARKS = 45,
    IDLE_LAMP_GLOW = 46,
    IDLE_CLOUD_DRIFT = 47,
    IDLE_QUIET_COMET = 48,
    IDLE_BREATHE_SECTIONS = 49,
    IDLE_COUNT       = 50
};

enum PrintAnimation {
    PRINT_PROGRESS   = 0,
    PRINT_LASER_TIP  = 1,
    PRINT_WAVE       = 2,
    PRINT_THERMAL    = 3,
    PRINT_STRIPES    = 4,
    PRINT_PULSE_PROG = 5,
    PRINT_COMET      = 6,
    PRINT_ACTIVE_SEC = 7,
    PRINT_RUNNING    = 8,
    PRINT_BREATHE_FIL= 9,
    PRINT_WIPE_PROG  = 10,
    PRINT_SHIMMER    = 11,
    PRINT_BICOLOR    = 12,
    PRINT_THERMOMETER= 13,
    PRINT_SNAKE         = 14,
    PRINT_LAYER         = 15,
    PRINT_HEARTBEAT_PROG= 16,
    PRINT_DNA           = 17,
    PRINT_PIXEL_RAIN    = 18,
    PRINT_CLOCKWISE     = 19,
    PRINT_EXTRUDER_SPARK = 20,
    PRINT_LAYER_SCAN = 21,
    PRINT_HEAT_RIPPLE = 22,
    PRINT_FILAMENT_COMETS = 23,
    PRINT_PROGRESS_THEATER = 24,
    PRINT_NOZZLE_TRACE = 25,
    PRINT_BUILD_PLATE = 26,
    PRINT_MICRO_STEPS = 27,
    PRINT_FLOW_WAVE = 28,
    PRINT_TOOLHEAD_ORBIT = 29,
    PRINT_THERMAL_BALANCE = 30,
    PRINT_MATERIAL_CORE = 31,
    PRINT_HEAT_SOAK = 32,
    PRINT_STABILITY_MONITOR = 33,
    PRINT_LAYER_ENGINE = 34,
    PRINT_TIME_TUNNEL = 35,
    PRINT_CHAMBER_AURA = 36,
    PRINT_FILAMENT_FLOW = 37,
    PRINT_PROCESS_STACK = 38,
    PRINT_HEALTH_BEACON = 39,
    PRINT_FINISH_PRESSURE = 40,
    PRINT_DUAL_TEMP_METER = 41,
    PRINT_LAYER_PULSE = 42,
    PRINT_TOOLPATH_ECHO = 43,
    PRINT_THERMAL_RIBBON = 44,
    PRINT_INFILL_GRID = 45,
    PRINT_FILAMENT_BEADS = 46,
    PRINT_TIME_REMAINING_FLOW = 47,
    PRINT_STEPPER_TICKS = 48,
    PRINT_CALM_BUILD = 49,
    PRINT_QUALITY_GUARD = 50,
    PRINT_NOZZLE_HEAT_TRACE = 51,
    PRINT_LAYER_FILL = 52,
    PRINT_COUNT         = 53
};

enum PauseAnimation {
    PAUSE_AMBER      = 0,
    PAUSE_BLINK_LR   = 1,
    PAUSE_FREEZE     = 2,
    PAUSE_RADAR      = 3,
    PAUSE_HEARTBEAT  = 4,
    PAUSE_PROGRESS_BAR = 5,
    PAUSE_CROSSFADE  = 6,
    PAUSE_PHASE      = 7,
    PAUSE_YELLOW_WHITE = 8,
    PAUSE_ICU        = 9,
    PAUSE_STROBE_AMBER = 10,
    PAUSE_ZIGZAG     = 11,
    PAUSE_NEON_SIGN    = 12,
    PAUSE_SANDCLOCK   = 13,
    PAUSE_AMBER_WAVE  = 14,
    PAUSE_BOUNCE_WAIT = 15,
    PAUSE_COMET_SLOW  = 16,
    PAUSE_SPINNER     = 17,
    PAUSE_MORSE_WAIT  = 18,
    PAUSE_BREATHE_BLUE= 19,
    PAUSE_SOFT_HOLD = 20,
    PAUSE_AMBER_THEATER = 21,
    PAUSE_BREATHING_DOTS = 22,
    PAUSE_WAITING_RIPPLE = 23,
    PAUSE_PARKING_LIGHTS = 24,
    PAUSE_DIM_SPARKS = 25,
    PAUSE_SLOW_SCAN = 26,
    PAUSE_FROZEN_GOLD = 27,
    PAUSE_CLOCK_TICK = 28,
    PAUSE_CALM_ORBIT = 29,
    PAUSE_HOLDING_PATTERN = 30,
    PAUSE_BREATHING_AMBER = 31,
    PAUSE_RESUME_GATE = 32,
    PAUSE_TEMP_KEEPALIVE = 33,
    PAUSE_ATTENTION_SOFT = 34,
    PAUSE_OPERATOR_WAIT = 35,
    PAUSE_FROZEN_LAYER = 36,
    PAUSE_FILAMENT_HOLD = 37,
    PAUSE_DO_NOT_TOUCH = 38,
    PAUSE_HEAT_HOLD_SPLIT = 39,
    PAUSE_CALM_DOWN = 40,
    PAUSE_STILL_WATER = 41,
    PAUSE_SOFT_LANTERN = 42,
    PAUSE_HOLD_ORB = 43,
    PAUSE_SUSPENDED_LAYER = 44,
    PAUSE_GENTLE_REMINDER = 45,
    PAUSE_BREATH_GATE = 46,
    PAUSE_WAITING_ROOM = 47,
    PAUSE_TOOL_PARK = 48,
    PAUSE_RESUME_RAMP = 49,
    PAUSE_COUNT       = 50
};

enum ErrorAnimation {
    ERROR_BLINK      = 0,
    ERROR_SOS        = 1,
    ERROR_FIRE_ALARM = 2,
    ERROR_CRITICAL   = 3,
    ERROR_POLICE     = 4,
    ERROR_BREATHE_RED= 5,
    ERROR_HEARTBEAT  = 6,
    ERROR_STROBE     = 7,
    ERROR_WAVE_RED   = 8,
    ERROR_XENON      = 9,
    ERROR_SIREN      = 10,
    ERROR_THUNDER     = 11,
    ERROR_COUNTDOWN   = 12,
    ERROR_GLITCH      = 13,
    ERROR_ALARM_CHASE = 14,
    ERROR_DANGER_STRIPE=15,
    ERROR_PULSE_ALERT = 16,
    ERROR_REDOUT      = 17,
    ERROR_EMERGENCY   = 18,
    ERROR_MELTDOWN    = 19,
    ERROR_CRASH       = 20,
    ERROR_RED_THEATER = 21,
    ERROR_FAULT_RIPPLE = 22,
    ERROR_HOT_ZONE = 23,
    ERROR_PANIC_COMETS = 24,
    ERROR_LOCKDOWN = 25,
    ERROR_WARNING_TICKS = 26,
    ERROR_BREACH_SCAN = 27,
    ERROR_FAULT_SPARKS = 28,
    ERROR_RED_JUGGLE = 29,
    ERROR_EVACUATE = 30,
    ERROR_ROOT_CAUSE_HINT = 31,
    ERROR_STACK_LIGHT = 32,
    ERROR_HEARTBEAT_SMART = 33,
    ERROR_LOCATION_SPLIT = 34,
    ERROR_BLACKOUT_FLASH = 35,
    ERROR_RECOVERY_WAIT = 36,
    ERROR_SIREN_SCAN_SMART = 37,
    ERROR_DIAGNOSTIC_BITS = 38,
    ERROR_SERVICE_BEACON = 39,
    ERROR_SAFE_SHUTDOWN = 40,
    ERROR_CALM_ALERT = 41,
    ERROR_FAULT_LOCATOR = 42,
    ERROR_THERMAL_CUT = 43,
    ERROR_NETWORK_LOST = 44,
    ERROR_SERVICE_CODE = 45,
    ERROR_CONTAINMENT = 46,
    ERROR_SAFE_BREATH = 47,
    ERROR_ESCALATION = 48,
    ERROR_REPAIR_BEACON = 49,
    ERROR_COOLING_ALARM = 50,
    ERROR_COUNT       = 51
};

enum OtherAnimation {
    OTHER_MATRIX     = 0,
    OTHER_CANDLE     = 1,
    OTHER_STATIC_RAINBOW = 2,
    OTHER_NEON_CLUB  = 3,
    OTHER_SYNTHWAVE  = 4,
    OTHER_JELLYFISH  = 5,
    OTHER_SNOW       = 6,
    OTHER_SUNSET     = 7,
    OTHER_VOLCANO    = 8,
    OTHER_TECHNO     = 9,
    OTHER_DRAGON_BLOOD = 10,
    OTHER_AURORA     = 11,
    OTHER_CYBERPUNK  = 12,
    OTHER_NEBULA     = 13,
    OTHER_SUBMARINE  = 14,
    OTHER_PRIDE      = 15,
    OTHER_PLASMA     = 16,
    OTHER_BOUNCING_BALLS = 17,
    OTHER_COP_CAR    = 18,
    OTHER_STROBE_PARTY = 19,
    OTHER_SUNRISE    = 20,
    OTHER_OCEANIC_DEPTH = 21,
    OTHER_RADIATION  = 22,
    OTHER_PASTEL     = 23,
    OTHER_ELECTRIC   = 24,
    OTHER_RAINBOW_PULSE = 25,
    OTHER_CARNIVAL   = 26,
    OTHER_NEON_SIGN  = 27,
    OTHER_MOTION_DETECT = 28,
    OTHER_RETRO_TV   = 29,
    OTHER_CRYSTAL    = 30,
    OTHER_FIRE_ICE   = 31,
    OTHER_LASER_GRID = 32,
    OTHER_GALAXY_SPIN = 33,
    OTHER_COMET_TWINS = 34,
    OTHER_DEEP_SEA_PULSE = 35,
    OTHER_SOLAR_WIND = 36,
    OTHER_PIXEL_CIRCUS = 37,
    OTHER_MINT_BREEZE = 38,
    OTHER_RUBY_SCAN = 39,
    OTHER_ARCADE_CHASE = 40,
    OTHER_STARDUST = 41,
    OTHER_ICE_CAVE = 42,
    OTHER_FIREWORK_TRAIL = 43,
    OTHER_CHROMA_RING = 44,
    OTHER_GHOST_LIGHT = 45,
    OTHER_TOXIC_WAVE = 46,
    OTHER_COPPER_SPARK = 47,
    OTHER_BLUEPRINT = 48,
    OTHER_MAGMA_FLOW = 49,
    OTHER_CANDY_STRIPE = 50,
    OTHER_QUANTUM_DOTS = 51,
    OTHER_SHOWROOM_LOOP = 52,
    OTHER_AUDIO_REACTIVE_FAKE = 53,
    OTHER_WEATHER_MOOD = 54,
    OTHER_CLOCK_AURORA = 55,
    OTHER_FILAMENT_GALLERY = 56,
    OTHER_MAINTENANCE_MODE = 57,
    OTHER_CALIBRATION_RULER = 58,
    OTHER_HEATMAP_DEMO = 59,
    OTHER_PRODUCT_HERO = 60,
    OTHER_NIGHT_LIGHT = 61,
    OTHER_FOCUS_MODE = 62,
    OTHER_PARTY_LOCK = 63,
    OTHER_RETRO_TERMINAL = 64,
    OTHER_PLASMA_CORE = 65,
    OTHER_STATUS_MIRROR = 66,
    OTHER_BREATHE_WITH_TIME = 67,
    OTHER_DEMO_ALL_SECTIONS = 68,
    OTHER_CINEMA_IDLE = 69,
    OTHER_LUXURY_AMBIENT = 70,
    OTHER_SPECTRUM_SCANNER = 71,
    OTHER_CALM_DOWN = 72,
    OTHER_MEDITATION = 73,
    OTHER_BIOLUMINESCENCE = 74,
    OTHER_LIQUID_GLASS = 75,
    OTHER_EMBER_ROOM = 76,
    OTHER_NEON_RAIN = 77,
    OTHER_SOLAR_ECLIPSE = 78,
    OTHER_CRYSTAL_PRISM = 79,
    OTHER_ROYAL_AURORA = 80,
    OTHER_DATA_STREAM = 81,
    OTHER_COUNT      = 82
};

constexpr uint16_t LED_COUNT = 60;
constexpr uint16_t RIGHT_START = 0;
constexpr uint16_t RIGHT_END = 10;
constexpr uint16_t FRONT_START = 11;
constexpr uint16_t FRONT_END = 30;
constexpr uint16_t LEFT_START = 31;
constexpr uint16_t LEFT_END = 41;
constexpr uint16_t INSIDE_START = 42;
constexpr uint16_t INSIDE_END = 59;
constexpr uint16_t LEFT_COUNT = LEFT_END - LEFT_START + 1;
constexpr uint16_t FRONT_COUNT = FRONT_END - FRONT_START + 1;
constexpr uint16_t RIGHT_COUNT = RIGHT_END - RIGHT_START + 1;
constexpr uint16_t OUTER_START = RIGHT_START;
constexpr uint16_t OUTER_END = LEFT_END;
constexpr uint16_t OUTER_COUNT = OUTER_END - OUTER_START + 1;
constexpr uint16_t INNER_START = INSIDE_START;
constexpr uint16_t INNER_END = INSIDE_END;
constexpr uint16_t INNER_COUNT = INNER_END - INNER_START + 1;
constexpr uint8_t OUT_MAX = 255;
constexpr uint8_t W_MAX_OUT = 255;

struct Runtime {
    IdleAnimation idleAnim = IDLE_RAINBOW;
    PrintAnimation printAnim = PRINT_PROGRESS;
    PauseAnimation pauseAnim = PAUSE_AMBER;
    ErrorAnimation errorAnim = ERROR_BLINK;
    FinishAnimation finishAnim = FINISH_RAINBOW;
    OtherAnimation otherAnim = OTHER_MATRIX;
    uint32_t finishAnimStart = 0;
    uint8_t sectionBright[5] = {0, 10, 10, 10, 10};
    bool mirrorLedLayout = false;
};

struct PrinterRuntime {
    uint8_t progress = 0;
    uint8_t activeTool = 0;
    float activeToolTempC = NAN;
    uint32_t activeToolTempUpdatedMs = 0;
    bool moonrakerOnline = false;
    RgbColor filamentColor = RgbColor(255, 255, 255);
};

struct VentRuntime {
    float currentTempC = NAN;
    float chamberTempC = NAN;
    float targetTempC = NAN;
    bool failsafeActive = false;
};

struct PreviewRuntime {
    bool active = false;
    uint8_t cat = 0;
    uint32_t startMs = 0;
    uint32_t demoFilR = 255;
    uint32_t demoFilG = 122;
    uint32_t demoFilB = 0;
};

static Runtime rtLed;
static PrinterRuntime rtPrinter;
static VentRuntime rtVent;
static PreviewRuntime gLedAnimPreview;
static int16_t gLedPreviewPrintAnimOverride = -1;
static bool gSnakeFinishBurstActive = false;
static RgbColor gCachedToolColors[4];
static String gCachedToolMaterials[4];
static RgbwColor targetFrame[LED_COUNT];
static bool previousPreview = false;
static LedCategory previousCategory = LedCategory::Count;
static uint8_t previousAnimation[static_cast<uint8_t>(LedCategory::Count)] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
static uint32_t previousRenderMs = 0;

static inline uint8_t scale8(uint8_t value, uint8_t maximum) {
    return static_cast<uint8_t>(static_cast<uint16_t>(value) * maximum / 255U);
}

static RgbColor hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
    const uint8_t region = h / 43U;
    const uint8_t remainder = static_cast<uint8_t>((h - region * 43U) * 6U);
    const uint8_t p = static_cast<uint8_t>(static_cast<uint16_t>(v) * (255U - s) / 255U);
    const uint8_t q = static_cast<uint8_t>(static_cast<uint16_t>(v) *
        (255U - static_cast<uint16_t>(s) * remainder / 255U) / 255U);
    const uint8_t t = static_cast<uint8_t>(static_cast<uint16_t>(v) *
        (255U - static_cast<uint16_t>(s) * (255U - remainder) / 255U) / 255U);
    switch (region) {
        default:
        case 0: return RgbColor(v, t, p);
        case 1: return RgbColor(q, v, p);
        case 2: return RgbColor(p, v, t);
        case 3: return RgbColor(p, q, v);
        case 4: return RgbColor(t, p, v);
        case 5: return RgbColor(v, p, q);
    }
}

static void rgbToHsv8(uint8_t r, uint8_t g, uint8_t b,
                      uint8_t& h, uint8_t& s, uint8_t& v) {
    const uint8_t maximum = max(r, max(g, b));
    const uint8_t minimum = min(r, min(g, b));
    v = maximum;
    const uint8_t delta = maximum - minimum;
    if (!maximum || !delta) { h = 0; s = 0; return; }
    s = static_cast<uint8_t>(static_cast<uint16_t>(delta) * 255U / maximum);
    int16_t value = 0;
    if (maximum == r) value = static_cast<int16_t>(43 * static_cast<int16_t>(g - b) / delta);
    else if (maximum == g) value = static_cast<int16_t>(85 + 43 * static_cast<int16_t>(b - r) / delta);
    else value = static_cast<int16_t>(171 + 43 * static_cast<int16_t>(r - g) / delta);
    while (value < 0) value += 255;
    h = static_cast<uint8_t>(value);
}

static RgbwColor rgbToRgbwLimited(const RgbColor& color, uint8_t outMax,
                                  uint8_t wMax) {
    (void)wMax;
    return RgbwColor(scale8(color.R, outMax), scale8(color.G, outMax),
                     scale8(color.B, outMax), 0);
}

static void clearTarget() { memset(targetFrame, 0, sizeof(targetFrame)); }
static void setTargetPixel(uint16_t index, const RgbwColor& color) {
    if (index < LED_COUNT) targetFrame[index] = color;
}
static void fillTargetRange(uint16_t start, uint16_t end, const RgbwColor& color) {
    for (uint16_t i = start; i <= end && i < LED_COUNT; ++i) targetFrame[i] = color;
}
static uint8_t sectionBrightToV(uint8_t brightness) {
    return static_cast<uint8_t>(min<uint16_t>(10U, brightness) * 255U / 10U);
}
static uint16_t sectionLedCount(uint8_t section) {
    if (section == 1) return LEFT_COUNT;
    if (section == 2) return FRONT_COUNT;
    if (section == 3) return RIGHT_COUNT;
    return INNER_COUNT;
}
static uint16_t sectionVisualLed(uint8_t section, uint16_t logical) {
    const uint16_t count = sectionLedCount(section);
    if (logical >= count) logical = count - 1U;
    if (section == 1) return LEFT_END - logical;
    if (section == 2) return FRONT_END - logical;
    if (section == 3) return RIGHT_START + logical;
    return INSIDE_START + logical;
}
static uint8_t outerSectionIdx(uint16_t index) {
    if (index >= LEFT_START && index <= LEFT_END) return 1;
    if (index >= FRONT_START && index <= FRONT_END) return 2;
    return 3;
}
static uint8_t sin8fast(uint8_t x) {
    const uint16_t y = x < 128U ? static_cast<uint16_t>(x) * (255U - x) / 32U
                                : static_cast<uint16_t>(255U - x) * x / 32U;
    return static_cast<uint8_t>(min<uint16_t>(255U, y));
}
static void fillSec(uint8_t section, const RgbwColor& color) {
    const uint16_t count = sectionLedCount(section);
    for (uint16_t i = 0; i < count; ++i) setTargetPixel(sectionVisualLed(section, i), color);
}
static RgbwColor secV(uint8_t section, uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t value = sectionBrightToV(rtLed.sectionBright[constrain(section, 1, 4)]);
    return rgbToRgbwLimited(RgbColor(static_cast<uint8_t>(static_cast<uint16_t>(r) * value / 255U),
        static_cast<uint8_t>(static_cast<uint16_t>(g) * value / 255U),
        static_cast<uint8_t>(static_cast<uint16_t>(b) * value / 255U)), OUT_MAX, W_MAX_OUT);
}
static RgbwColor secHSV(uint8_t section, uint8_t h, uint8_t s, uint8_t v) {
    const uint8_t value = sectionBrightToV(rtLed.sectionBright[constrain(section, 1, 4)]);
    return rgbToRgbwLimited(hsvToRgb(h, s,
        static_cast<uint8_t>(static_cast<uint16_t>(v) * value / 255U)), OUT_MAX, W_MAX_OUT);
}
static bool ledAnimPreviewActive() { return gLedAnimPreview.active; }
static bool ledAllowsInsideRgbNow() { return true; }
static void renderInsideNormalToTarget() {}
static void setInsideAmbientRgb(const RgbColor&, uint8_t, uint8_t = 0) {}
static uint8_t clampPctFloat(float value, float low, float high, uint8_t fallback) {
    if (isnan(value) || high <= low) return fallback;
    if (value <= low) return 0;
    if (value >= high) return 100;
    return static_cast<uint8_t>((value - low) * 100.0f / (high - low));
}
static RgbColor tempColorFromPct(uint8_t pct) {
    const uint8_t hue = pct < 80U
        ? static_cast<uint8_t>(160U - static_cast<uint16_t>(pct) * 120U / 80U)
        : static_cast<uint8_t>(40U - static_cast<uint16_t>(pct - 80U) * 40U / 20U);
    return hsvToRgb(hue, 255, 255);
}
static void fillSectionMeter(uint8_t section, uint8_t pct, const RgbColor& color,
                             bool reverse = false) {
    const uint16_t count = sectionLedCount(section);
    uint16_t lit = static_cast<uint16_t>(static_cast<uint32_t>(pct) * count / 100U);
    if (pct && !lit) lit = 1;
    for (uint16_t i = 0; i < count; ++i) {
        const uint16_t logical = reverse ? count - 1U - i : i;
        const uint8_t value = logical < lit ? 220U : 14U;
        setTargetPixel(sectionVisualLed(section, i), secV(section,
            static_cast<uint8_t>(static_cast<uint16_t>(color.R) * value / 255U),
            static_cast<uint8_t>(static_cast<uint16_t>(color.G) * value / 255U),
            static_cast<uint8_t>(static_cast<uint16_t>(color.B) * value / 255U)));
    }
}

union LegacyAccentColor {
    uint32_t full;
    struct { uint8_t blue, green, red, alpha; } ch;
};
static LegacyAccentColor navAccentColor() {
    LegacyAccentColor color{};
    color.ch.red = 0;
    color.ch.green = 194;
    color.ch.blue = 235;
    color.ch.alpha = 255;
    return color;
}
using lv_color32_t = LegacyAccentColor;

static inline RgbwColor outerExtraColor(uint16_t idx, uint8_t hue, uint8_t sat, uint8_t val,
                                        uint8_t r, uint8_t g, uint8_t b, bool useRgb) {
    const uint8_t sec = outerSectionIdx(idx);
    if (useRgb) {
        return secV(sec,
                    (uint8_t)((uint16_t)r * val / 255),
                    (uint8_t)((uint16_t)g * val / 255),
                    (uint8_t)((uint16_t)b * val / 255));
    }
    return secHSV(sec, hue, sat, val);
}

static inline uint8_t extraSpark(uint16_t i, uint32_t t, uint8_t salt) {
    uint32_t x = (uint32_t)i * 1103515245UL + (t / 37U) * 2654435761UL + (uint32_t)salt * 2246822519UL;
    x ^= x >> 16;
    return (uint8_t)x;
}

static void renderOuterExtraPattern(uint8_t variant, uint32_t t, uint8_t hue, uint8_t sat,
                                    uint8_t r, uint8_t g, uint8_t b, bool useRgb,
                                    uint8_t progressPct = 0) {
    variant %= 20;
    const uint16_t span = (OUTER_COUNT > 1) ? (OUTER_COUNT - 1) : 1;
    const uint16_t pingPeriod = span * 2;

    switch (variant) {
    case 0: {
        uint8_t phase = (uint8_t)(t / 90);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool on = ((i + phase) % 3) == 0;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 3), sat, on ? OUT_MAX : 18, r, g, b, useRgb));
        }
        break;
    }
    case 1: {
        uint16_t wave = (uint16_t)(((t / 28U) + progressPct) % pingPeriod);
        uint16_t pos = (wave <= span) ? wave : (pingPeriod - wave);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t dist = (i > pos) ? (i - pos) : (pos - i);
            uint8_t v = (dist < 7) ? (uint8_t)(OUT_MAX - dist * (OUT_MAX / 7)) : 0;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 2), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 2: {
        for (uint8_t d = 0; d < 5; d++) {
            uint16_t wave = (uint16_t)(((t / (34U + d * 9U)) + d * 13U) % pingPeriod);
            uint16_t pos = (wave <= span) ? wave : (pingPeriod - wave);
            uint16_t idx = OUTER_START + pos;
            setTargetPixel(idx, outerExtraColor(idx, hue + d * 35, sat, OUT_MAX, r, g, b, useRgb));
            if (pos > 0) setTargetPixel(idx - 1, outerExtraColor(idx - 1, hue + d * 35, sat, 90, r, g, b, useRgb));
            if (pos + 1 < OUTER_COUNT) setTargetPixel(idx + 1, outerExtraColor(idx + 1, hue + d * 35, sat, 90, r, g, b, useRgb));
        }
        break;
    }
    case 3: {
        uint8_t beat = sin8fast((uint8_t)(t / 9));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t wave = sin8fast((uint8_t)(beat + i * 10));
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 4), sat, (uint8_t)(35 + wave * 180 / 255), r, g, b, useRgb));
        }
        break;
    }
    case 4: {
        uint16_t center = OUTER_COUNT / 2;
        uint16_t ring = (uint16_t)((t / 45U) % (center + 6));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t dist = (i > center) ? (i - center) : (center - i);
            uint16_t delta = (dist > ring) ? (dist - ring) : (ring - dist);
            uint8_t v = (delta < 3) ? (uint8_t)(OUT_MAX - delta * 70) : 8;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(dist * 9), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 5: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t spark = extraSpark(i, t, variant);
            uint8_t v = (spark > 238) ? OUT_MAX : (uint8_t)(18 + sin8fast((uint8_t)(t / 24 + i * 7)) / 5);
            setTargetPixel(idx, outerExtraColor(idx, hue + spark / 4, sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 6: {
        uint16_t head = (uint16_t)((t / 35U) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d1 = (head >= i) ? (head - i) : (OUTER_COUNT + head - i);
            uint16_t mirror = OUTER_COUNT - 1 - head;
            uint16_t d2 = (i >= mirror) ? (i - mirror) : (OUTER_COUNT + i - mirror);
            uint16_t d = min(d1, d2);
            uint8_t v = (d < 8) ? (uint8_t)(OUT_MAX - d * 28) : 0;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 3), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 7: {
        uint8_t phase = (uint8_t)(t / 120);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t band = (uint8_t)(((i / 3) + phase) % 6);
            uint8_t v = (band == 0 || band == 3) ? OUT_MAX : (band == 1 || band == 4) ? 90 : 18;
            setTargetPixel(idx, outerExtraColor(idx, hue + band * 18, sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 8: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t wave = sin8fast((uint8_t)(t / 18 + i * 12));
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(wave / 5), sat, (uint8_t)(20 + wave * 210 / 255), r, g, b, useRgb));
        }
        break;
    }
    case 9: {
        uint16_t lit = (uint16_t)((uint32_t)progressPct * OUTER_COUNT / 100U);
        if (progressPct == 0) lit = OUTER_COUNT / 3;
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool active = i < lit || progressPct == 0;
            uint8_t spark = extraSpark(i, t, variant);
            uint8_t v = active ? ((spark > 230) ? OUT_MAX : 55) : 8;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 2), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 10: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t a = sin8fast((uint8_t)(t / 19 + i * 17));
            uint8_t c = sin8fast((uint8_t)(t / 31 + i * 5 + 80));
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), hue + (uint8_t)((a + c) / 3), sat, (uint8_t)(45 + c * 180 / 255)));
        }
        break;
    }
    case 11: {
        uint8_t gate = (uint8_t)((t / 75) % 7);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t lane = (uint8_t)((i + gate) % 7);
            uint8_t v = (lane == 0) ? OUT_MAX : (lane == 1 || lane == 6) ? 90 : 12;
            setTargetPixel(idx, outerExtraColor(idx, hue + lane * 10, sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 12: {
        uint16_t ring = (uint16_t)((t / 38U) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t dA = (i > ring) ? (i - ring) : (ring - i);
            uint16_t mirror = OUTER_COUNT - 1 - i;
            uint16_t dB = (mirror > ring) ? (mirror - ring) : (ring - mirror);
            uint16_t d = min(dA, dB);
            uint8_t v = (d < 4) ? (uint8_t)(OUT_MAX - d * 55) : 14;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(d * 16), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 13: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t p = (sin8fast((uint8_t)(t / 45 + i * 8)) + sin8fast((uint8_t)(t / 27 + i * 3 + 50))) / 2;
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), hue + p / 3, 190, (uint8_t)(35 + p * 180 / 255)));
        }
        break;
    }
    case 14: {
        uint8_t shift = (uint8_t)(t / 55);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool on = (((i + shift) / 2) % 4) != 0;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 6), sat, on ? OUT_MAX : 0, r, g, b, useRgb));
        }
        break;
    }
    case 15: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t band = sin8fast((uint8_t)(t / 13 + i * 21));
            uint8_t spark = extraSpark(i, t, variant);
            uint8_t v = (uint8_t)(band / 2 + (spark > 245 ? 110 : 20));
            setTargetPixel(idx, outerExtraColor(idx, hue + band / 6, sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 16: {
        uint8_t breath = sin8fast((uint8_t)(t / 18));
        for (uint8_t sec = 1; sec <= 3; sec++) {
            if (useRgb) fillSec(sec, secV(sec, (uint8_t)((uint16_t)r * breath / 255), (uint8_t)((uint16_t)g * breath / 255), (uint8_t)((uint16_t)b * breath / 255)));
            else fillSec(sec, secHSV(sec, hue + sec * 12, sat, (uint8_t)(25 + breath * 210 / 255)));
        }
        break;
    }
    case 17: {
        uint16_t pos = (uint16_t)((t / 30U) % (OUTER_COUNT / 2 + 1));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t mirror = OUTER_COUNT - 1 - i;
            uint16_t sidePos = (i < OUTER_COUNT / 2) ? i : mirror;
            uint16_t d = (sidePos > pos) ? (sidePos - pos) : (pos - sidePos);
            uint8_t v = (d < 5) ? (uint8_t)(OUT_MAX - d * 42) : 10;
            setTargetPixel(idx, outerExtraColor(idx, hue + (uint8_t)(i * 4), sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 18: {
        uint8_t fall = (uint8_t)(t / 65);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t lane = (uint8_t)((i + fall) % 9);
            uint8_t v = (lane == 0) ? OUT_MAX : (lane == 1) ? 120 : (lane == 2) ? 45 : 0;
            setTargetPixel(idx, outerExtraColor(idx, hue + lane * 9, sat, v, r, g, b, useRgb));
        }
        break;
    }
    case 19: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t spark = extraSpark(i, t, variant);
            uint8_t v = (spark > 220) ? (uint8_t)(120 + spark / 2) : (uint8_t)(sin8fast((uint8_t)(t / 40 + i * 11)) / 7);
            setTargetPixel(idx, outerExtraColor(idx, hue + spark, sat, v, r, g, b, useRgb));
        }
        break;
    }
    }
}


static void renderIdleTarget() {
    // BUG-1 fix: hueBase was static-zero after IDLE_RUNNING refactor - frozen rainbow
    uint32_t t = millis();
    uint8_t hueBase = (uint8_t)(t / 20);  // smooth millis-based hue scroll
    // COLOR_WIPE accumulates pixels across frames - don't clear
    if (rtLed.idleAnim != IDLE_COLOR_WIPE) clearTarget();
    switch (rtLed.idleAnim) {

    case IDLE_RAINBOW: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t h = hueBase + (uint8_t)((i * 256) / OUTER_COUNT);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 255, OUT_MAX));
        }
        break;
    }
    case IDLE_FIRE: {
        // Fire: per-LED heat array for L and R, Center glows amber
        // PERF-3 fix: heat update step-guarded at 30ms (FPS-independent flame speed)
        static const uint32_t FIRE_STEP_MS = 30;
        static uint32_t lastFireStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t heatL[LEFT_COUNT] = {}, heatR[RIGHT_COUNT] = {};
        if (t - lastFireStep >= FIRE_STEP_MS) {
            lastFireStep = t - ((t - lastFireStep) % FIRE_STEP_MS);
            for (int i = (int)LEFT_COUNT - 1; i > 0; i--) {
                heatL[i] = (heatL[i-1] + heatL[i] + heatL[i]) / 3;
                heatR[i] = (heatR[i-1] + heatR[i] + heatR[i]) / 3;
            }
            // Cool the base before adding heat - prevents saturation at 255
            heatL[0] = (uint8_t)max((int32_t)0, (int32_t)heatL[0] - (int32_t)random(30, 80));
            heatR[0] = (uint8_t)max((int32_t)0, (int32_t)heatR[0] - (int32_t)random(30, 80));
            // Then add random heat burst
            heatL[0] = (uint8_t)min((int32_t)255, (int32_t)heatL[0] + (int32_t)random(60, 160));
            heatR[0] = (uint8_t)min((int32_t)255, (int32_t)heatR[0] + (int32_t)random(60, 160));
        }
        // Map heat to fire color: black->red->orange->yellow
        auto heatColor = [](uint8_t h) -> RgbColor {
            if (h < 85) return RgbColor(h * 3, 0, 0);
            if (h < 170) return RgbColor(255, (h - 85) * 3, 0);
            return RgbColor(255, 255, (h - 170) * 3);
        };
        uint8_t bL = sectionBrightToV(rtLed.sectionBright[1]);
        uint8_t bR = sectionBrightToV(rtLed.sectionBright[3]);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            // LEFT visual: i=0=top, i=10=bottom.
            RgbColor cl = heatColor(heatL[(LEFT_COUNT - 1) - i]);
            // RIGHT visual: i=0=top, i=10=bottom.
            RgbColor cr = heatColor(heatR[i]);
            setTargetPixel(sectionVisualLed(1, i), rgbToRgbwLimited(RgbColor((uint16_t)cl.R*bL/255,(uint16_t)cl.G*bL/255,(uint16_t)cl.B*bL/255), 255, W_MAX_OUT));
            setTargetPixel(sectionVisualLed(3, i), rgbToRgbwLimited(RgbColor((uint16_t)cr.R*bR/255,(uint16_t)cr.G*bR/255,(uint16_t)cr.B*bR/255), 255, W_MAX_OUT));
        }
        uint32_t t = millis();
        uint8_t bC = (uint8_t)(sectionBrightToV(rtLed.sectionBright[2]) * (180 + (t/20 % 40)) / 255);
        fillSec(2, rgbToRgbwLimited(RgbColor(bC, bC/3, 0), 255, W_MAX_OUT));
        break;
    }
    case IDLE_OCEAN: {
        uint32_t t = millis();
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t ph = (uint8_t)((t / 20) + i * 13);
            uint8_t wave = sin8fast(ph);
            // cyan (128,255,255) blending to white by wave
            uint8_t r = (uint8_t)(128 + (uint16_t)wave * 127 / 255);
            uint8_t g = 255;
            uint8_t b = 255;
            setTargetPixel(idx, secV(outerSectionIdx(idx), r, g, b));
        }
        break;
    }
    case IDLE_PULSE_STAR: {
        // millis-based: 6ms/tick -> full breath cycle = 512*6 = 3.07s
        uint32_t breathT = t / 6;
        uint16_t ph = breathT % 512;
        uint8_t breath = (ph < 256) ? (uint8_t)ph : (uint8_t)(511 - ph);
        uint8_t v = (uint8_t)(60 + breath / 2);
        uint8_t starHue = (uint8_t)((breathT / 512) * 21);
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secHSV(sec, starHue, 255, v));
        break;
    }
    case IDLE_METEOR: {
        // millis-based: 40ms/step, cycle = (OUTER_COUNT+11)*40ms
        static const int16_t METEOR_RANGE = (int16_t)OUTER_COUNT + 11;
        static const uint32_t METEOR_STEP_MS = 40;
        EXT_RAM_BSS_ATTR static uint8_t tail[OUTER_COUNT] = {};
        static uint32_t lastMeteorStep = 0;
        static int16_t mpos = -3;
        static uint8_t mhue = 0;
        if (t - lastMeteorStep >= METEOR_STEP_MS) {
            lastMeteorStep = t - ((t - lastMeteorStep) % METEOR_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) tail[i] = (uint8_t)((uint16_t)tail[i] * 180 / 255);
            if (mpos >= 0 && mpos < (int16_t)OUTER_COUNT) tail[mpos] = 255;
            mpos++;
            if (mpos >= METEOR_RANGE) { mpos = -3; mhue += 40; }
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), mhue, 255, (uint8_t)((uint16_t)tail[i] * OUT_MAX / 255)));
        }
        break;
    }
    case IDLE_TWINKLE: {
        // WARN-5 fix: decay and spawn step-guarded at 20ms so fade speed is FPS-independent
        static const uint32_t TW_STEP_MS = 20;
        static uint32_t lastTwStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t twState[OUTER_COUNT] = {};
        if (t - lastTwStep >= TW_STEP_MS) {
            lastTwStep = t - ((t - lastTwStep) % TW_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) {
                if (twState[i] > 0) twState[i] = (uint8_t)((uint16_t)twState[i] * 210 / 255);
                else if (random(100) < 3) twState[i] = 255;
            }
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), (uint8_t)(i*17), 50, twState[i]));
        }
        break;
    }
    case IDLE_LARSON: {
        // millis-based: 50ms/step for scanner movement
        static const uint32_t LARSON_STEP_MS = 25;  // 25ms -> ~2s full scan (was 50ms/4s)
        EXT_RAM_BSS_ATTR static uint8_t ltail[OUTER_COUNT] = {};
        static uint32_t lastLarsonStep = 0;
        static int16_t lpos = 0;
        static int8_t  ldir = 1;
        if (t - lastLarsonStep >= LARSON_STEP_MS) {
            lastLarsonStep = t - ((t - lastLarsonStep) % LARSON_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) ltail[i] = (uint8_t)((uint16_t)ltail[i] * 190 / 255);
            if (lpos >= 0 && lpos < (int16_t)OUTER_COUNT) ltail[lpos] = 255;
            lpos += ldir;
            if (lpos >= (int16_t)OUTER_COUNT - 1 || lpos <= 0) ldir = -ldir;
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secV(outerSectionIdx(idx), ltail[i], 0, 0));
        }
        break;
    }
    case IDLE_LAVA: {
        uint32_t t = millis();
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t ph1 = (uint8_t)((t / 50 + i * 17));
            uint8_t ph2 = (uint8_t)((t / 80 + i * 11));
            uint8_t blend = (sin8fast(ph1) + sin8fast(ph2)) / 2;
            // Warm/cool blend: orange (0,hue=10) <-> purple (hue=200)
            uint8_t h = (uint8_t)(10 + (uint16_t)blend * 50 / 255);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 255, (uint8_t)(80 + blend/3)));
        }
        break;
    }
    case IDLE_GRADIENT: {
        // millis-based: t/5 -> ~4s rainbow cycle (was t/16 = 12s)
        uint32_t gBase = t / 5;
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t h = (uint8_t)((gBase / 3 + i * 6) & 0xFF);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 255, OUT_MAX));
        }
        break;
    }
    case IDLE_PLASMA: {
        uint32_t t = millis();
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t p1 = sin8fast((uint8_t)(t/20 + i * 15));
            uint8_t p2 = sin8fast((uint8_t)(t/13 + i * 9 + 60));
            uint8_t h = (p1 + p2) / 2;
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 255, OUT_MAX));
        }
        break;
    }
    case IDLE_PHASE_BREATHE: {
        uint32_t t = millis();
        auto breathSec = [&](uint8_t sec, uint32_t offset) {
            uint32_t ph = (t + offset) % 1024;
            uint8_t tri = (ph < 512) ? (ph/2) : ((1023-ph)/2);
            uint8_t v = (uint8_t)(50 + tri/2);
            fillSec(sec, secHSV(sec, 30, 255, v));
        };
        breathSec(1, 0);
        breathSec(2, 341);
        breathSec(3, 682);
        break;
    }
    case IDLE_SNOW: {
        // WARN-5 fix: decay step-guarded at 20ms
        static const uint32_t SNOW_STEP_MS = 20;
        static uint32_t lastSnowStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t snowState[OUTER_COUNT] = {};
        if (t - lastSnowStep >= SNOW_STEP_MS) {
            lastSnowStep = t - ((t - lastSnowStep) % SNOW_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) {
                if (snowState[i] > 0) snowState[i] = (uint8_t)((uint16_t)snowState[i] * 220 / 255);
                else if (random(100) < 2) snowState[i] = 255;
            }
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t sv = snowState[i];
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(20 + sv*235/255), (uint8_t)(20+sv*235/255), (uint8_t)min((uint32_t)255, (uint32_t)30+sv)));
        }
        break;
    }
    case IDLE_COLOR_WIPE: {
        // millis-based: one pixel per 30ms
        static const uint32_t WIPE_STEP_MS = 30;
        static uint32_t lastWipeStep = 0;
        static uint16_t wpos = 0;
        static uint8_t  wcol = 0;
        static bool     wdir = false;
        if (t - lastWipeStep >= WIPE_STEP_MS) {
            lastWipeStep = t - ((t - lastWipeStep) % WIPE_STEP_MS);
            if (!wdir) {
                uint16_t idx = OUTER_START + wpos;
                setTargetPixel(idx, secHSV(outerSectionIdx(idx), wcol, 255, OUT_MAX));
            } else {
                uint16_t idx = OUTER_START + (OUTER_COUNT - 1 - wpos);
                setTargetPixel(idx, RgbwColor(0,0,0,0));
            }
            if (++wpos >= OUTER_COUNT) { wpos = 0; if (wdir) wcol += 43; wdir = !wdir; }
        }
        break;
    }
    case IDLE_MOONLIGHT: {
        uint32_t t = millis();
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t ph = (uint8_t)(t/40 + i*5);
            uint8_t w = (uint8_t)(120 + sin8fast(ph)/3);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(w*3/4), (uint8_t)(w*3/4), w));
        }
        break;
    }
    case IDLE_TETRIS: {
        // Blocks fall through all 3 outer sections as one continuous stream:
        // LEFT(0-10) -> FRONT(11-30) -> RIGHT(31-41)
        // RIGHT is physically reversed (31=bottom, 41=top) but numerically
        // continuous - blocks appear to flow through the right strip bottom->top.
        static const uint32_t TETRIS_STEP_MS = 120;
        EXT_RAM_BSS_ATTR static uint8_t blk[OUTER_COUNT] = {};
        EXT_RAM_BSS_ATTR static uint8_t blkCol[OUTER_COUNT] = {};
        static uint32_t lastTetrisStep = 0;
        if (t - lastTetrisStep >= TETRIS_STEP_MS) {
            lastTetrisStep = t - ((t - lastTetrisStep) % TETRIS_STEP_MS);
            // Shift entire outer strip: new blocks enter at index 0, exit at 39
            for (int i = (int)OUTER_COUNT - 1; i > 0; i--) {
                blk[i] = blk[i-1];
                blkCol[i] = blkCol[i-1];
            }
            blk[0] = (random(4) == 0) ? 255 : 0;
            blkCol[0] = (uint8_t)random(256);
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            if (blk[i]) setTargetPixel(idx, secHSV(outerSectionIdx(idx), blkCol[i], 255, OUT_MAX));
        }
        break;
    }
    case IDLE_RUNNING: default: {
        // millis-based: t/30 for pattern, t/20 for hue
        uint32_t tRun = t / 30;
        uint8_t hueRun = (uint8_t)(t / 20);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t v = ((tRun + i) % 5 < 2) ? OUT_MAX : 20;
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), hueRun, 255, v));
        }
        break;
    }
    case IDLE_BUBBLES: {
        // Bubbles rise up left/right strips, pop at top
        static const uint32_t BUB_STEP = 80;
        static uint32_t lastBub = 0;
        EXT_RAM_BSS_ATTR static float bL[5]={0,2,4,6,8},bR[5]={1,3,5,7,9};
        static uint8_t bhL[5]={160,180,200,220,170},bhR[5]={160,180,200,220,170};
        if ((uint32_t)(t-lastBub)>=BUB_STEP) {
            lastBub=t-((t-lastBub)%BUB_STEP);
            for(int b=0;b<5;b++){bL[b]-=0.6f;if(bL[b]<0){bL[b]=10.0f;bhL[b]=(uint8_t)random(140,230);}
                bR[b]-=0.6f;if(bR[b]<0){bR[b]=10.0f;bhR[b]=(uint8_t)random(140,230);}}
        }
        for(uint16_t i=0;i<LEFT_COUNT;i++){setTargetPixel(sectionVisualLed(1, i),secV(1,0,0,0));setTargetPixel(sectionVisualLed(3, i),secV(3,0,0,0));}
        for(int b=0;b<5;b++){
            uint8_t pL=(uint8_t)((LEFT_COUNT - 1)-(uint8_t)bL[b]),pR=(uint8_t)((RIGHT_COUNT - 1)-(uint8_t)bR[b]);
            setTargetPixel(sectionVisualLed(1, pL), secHSV(1,bhL[b],230,OUT_MAX));
            setTargetPixel(sectionVisualLed(3, pR), secHSV(3,bhR[b],230,OUT_MAX));
        }
        fillSec(2,secV(2,0,0,0));
        break;
    }
    case IDLE_DRIFT: {
        // Slow hue drift - aurora-like
        uint8_t base=(uint8_t)(t/12);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t h=base+(uint8_t)(i*5);
            uint8_t v=(uint8_t)(160+40.0f*sinf((float)(t/800.0f+i*0.3f)));
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),h,210,v));
        }
        break;
    }
    case IDLE_CANDLE_WARM: {
        // Warm orange-amber candle flicker all sections
        static const uint32_t CAND_STEP=40;
        static uint32_t lastCand=0;
        EXT_RAM_BSS_ATTR static uint8_t candV[OUTER_COUNT]={};
        if((uint32_t)(t-lastCand)>=CAND_STEP){
            lastCand=t-((t-lastCand)%CAND_STEP);
            for(uint16_t i=0;i<OUTER_COUNT;i++){int v=(int)candV[i]+(int)random(-20,25);if(v<80)v=80;if(v>220)v=220;candV[i]=(uint8_t)v;}
        }
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i; uint8_t v=candV[i];
            setTargetPixel(idx,rgbToRgbwLimited(RgbColor(v,v/3,0),255,W_MAX_OUT));
        }
        break;
    }
    case IDLE_STARFIELD: {
        // Stars twinkle at random brightness on all LEDs
        static const uint32_t STAR_STEP=120;
        static uint32_t lastStar=0;
        EXT_RAM_BSS_ATTR static uint8_t starV[OUTER_COUNT]={};
        EXT_RAM_BSS_ATTR static int8_t  starD[OUTER_COUNT]={};
        static bool starInit=false;
        if(!starInit){starInit=true;for(int i=0;i<(int)OUTER_COUNT;i++){starV[i]=(uint8_t)random(0,60);starD[i]=(random(0,2)?1:-1);}}
        if((uint32_t)(t-lastStar)>=STAR_STEP){
            lastStar=t-((t-lastStar)%STAR_STEP);
            for(int i=0;i<(int)OUTER_COUNT;i++){int v=(int)starV[i]+starD[i]*(int)random(5,25);if(v<=0){v=0;starD[i]=1;}if(v>=255){v=255;starD[i]=-1;}starV[i]=(uint8_t)v;}
        }
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint8_t v=starV[OUTER_START+i];setTargetPixel(OUTER_START+i,rgbToRgbwLimited(RgbColor(v,v,v),255,W_MAX_OUT));} // INSIDE-FIX: only outer
        break;
    }
    case IDLE_AURORA_RIBBON:    { renderOuterExtraPattern(13, t, 95, 210, 0, 0, 0, false); break; }
    case IDLE_RAINBOW_GLITTER:  { renderOuterExtraPattern(5,  t, hueBase, 255, 0, 0, 0, false); break; }
    case IDLE_SOFT_COMET:       { renderOuterExtraPattern(1,  t, 150, 180, 0, 0, 0, false); break; }
    case IDLE_KALEIDOSCOPE:     { renderOuterExtraPattern(10, t, hueBase, 220, 0, 0, 0, false); break; }
    case IDLE_BREATHING_ORBIT:  { renderOuterExtraPattern(16, t, 180, 190, 0, 0, 0, false); break; }
    case IDLE_PIXEL_FIRELIES:   { renderOuterExtraPattern(19, t, 45, 210, 0, 0, 0, false); break; }
    case IDLE_COSMIC_DUST:      { renderOuterExtraPattern(15, t, 190, 180, 0, 0, 0, false); break; }
    case IDLE_THEATER_GLOW:     { renderOuterExtraPattern(0,  t, 135, 160, 0, 0, 0, false); break; }
    case IDLE_TIDAL_POOL:       { renderOuterExtraPattern(8,  t, 145, 210, 0, 0, 0, false); break; }
    case IDLE_NEON_DRIFT:       { renderOuterExtraPattern(3,  t, 205, 230, 0, 0, 0, false); break; }
    case IDLE_READY_BREATH: {
        uint8_t breath = sin8fast((uint8_t)(t / 18));
        uint8_t v = (uint8_t)(55 + (uint16_t)breath * 115 / 255);
        fillSec(1, secV(1, 0, v / 2, v));
        fillSec(3, secV(3, 0, v / 2, v));
        fillSec(2, secV(2, v, v, v));
        setInsideAmbientRgb(RgbColor(0, 190, 255), (uint8_t)(35 + breath / 3), 18);
        break;
    }
    case IDLE_AMBIENT_CLOCK: {
        uint8_t tick = (uint8_t)(t / 1000);
        fillSec(1, secV(1, 0, 35, 70));
        fillSec(2, secV(2, 12, 24, 34));
        fillSec(3, secV(3, 0, 35, 70));
        uint16_t pos = (uint16_t)((t / 950UL) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 4; tr++) {
            uint16_t p = (pos + OUTER_COUNT - tr) % OUTER_COUNT;
            uint8_t v = (uint8_t)(180 - tr * 35);
            setTargetPixel(OUTER_START + p, secHSV(outerSectionIdx(OUTER_START + p), (uint8_t)(145 + tick), 120, v));
        }
        setInsideAmbientRgb(RgbColor(30, 110, 180), (uint8_t)(30 + sin8fast((uint8_t)(t / 45)) / 4), 10);
        break;
    }
    case IDLE_TEMPERATURE_IDLE: {
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 20.0f, 65.0f, 15);
        RgbColor temp = tempColorFromPct(chamberPct);
        fillSectionMeter(1, chamberPct, temp);
        fillSectionMeter(3, chamberPct, temp, true);
        fillSec(2, secV(2, temp.R / 3, temp.G / 3, temp.B / 3));
        setInsideAmbientRgb(temp, (uint8_t)(35 + chamberPct / 3), 6);
        break;
    }
    case IDLE_LAST_PRINT_ECHO: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = hsvToRgb((uint8_t)(t / 30), 255, 210);
        uint8_t breath = sin8fast((uint8_t)(t / 22));
        fillSec(1, secV(1, fil.R / 3, fil.G / 3, fil.B / 3));
        fillSec(3, secV(3, fil.R / 3, fil.G / 3, fil.B / 3));
        fillSectionMeter(2, rtPrinter.progress, fil);
        setInsideAmbientRgb(fil, (uint8_t)(28 + breath / 4), 8);
        break;
    }
    case IDLE_WIFI_BEACON: {
        bool online = rtPrinter.moonrakerOnline;
        uint8_t pulse = sin8fast((uint8_t)(t / 12));
        RgbColor ok(0, 210, 120), warn(255, 130, 0);
        RgbColor c = online ? ok : warn;
        fillSec(2, secV(2, c.R / 4, c.G / 4, c.B / 4));
        uint16_t pos = (uint16_t)((t / (online ? 90UL : 45UL)) % OUTER_COUNT);
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t p = (pos + OUTER_COUNT - i) % OUTER_COUNT;
            uint8_t v = (uint8_t)(210 - i * 36);
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)((uint16_t)c.R * v / 255), (uint8_t)((uint16_t)c.G * v / 255), (uint8_t)((uint16_t)c.B * v / 255)));
        }
        setInsideAmbientRgb(c, (uint8_t)(25 + pulse / 5), online ? 8 : 0);
        break;
    }
    case IDLE_SLEEPY_CORE: {
        uint8_t breath = sin8fast((uint8_t)(t / 30));
        fillSec(1, secV(1, 0, 10, 22));
        fillSec(2, secV(2, 4, 8, 14));
        fillSec(3, secV(3, 0, 10, 22));
        setInsideAmbientRgb(RgbColor(80, 25, 160), (uint8_t)(24 + breath / 5), 12);
        break;
    }
    case IDLE_MATERIAL_SHELF: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(180, 180, 210);
        renderOuterExtraPattern(13, t, (uint8_t)(t / 35), 190, fil.R, fil.G, fil.B, true);
        setInsideAmbientRgb(fil, (uint8_t)(40 + sin8fast((uint8_t)(t / 26)) / 5), 10);
        break;
    }
    case IDLE_STATUS_RING: {
        RgbColor c = rtPrinter.moonrakerOnline ? RgbColor(0, 200, 120) : RgbColor(255, 90, 0);
        uint8_t v = (uint8_t)(80 + sin8fast((uint8_t)(t / 16)) / 2);
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, (uint8_t)((uint16_t)c.R * v / 255), (uint8_t)((uint16_t)c.G * v / 255), (uint8_t)((uint16_t)c.B * v / 255)));
        setInsideAmbientRgb(c, (uint8_t)(35 + v / 4), 10);
        break;
    }
    case IDLE_CHAMBER_LANTERN: {
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 20.0f, 65.0f, 12);
        RgbColor c = tempColorFromPct(chamberPct);
        uint8_t breath = sin8fast((uint8_t)(t / 36));
        fillSec(1, secV(1, c.R / 4, c.G / 4, c.B / 4));
        fillSec(2, secV(2, c.R / 3, c.G / 3, c.B / 3));
        fillSec(3, secV(3, c.R / 4, c.G / 4, c.B / 4));
        setInsideAmbientRgb(c, (uint8_t)(45 + chamberPct / 4 + breath / 8), 42);
        break;
    }
    case IDLE_PRINT_READY_SPLIT: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(0, 180, 255);
        fillSec(1, secV(1, fil.R / 2, fil.G / 2, fil.B / 2));
        fillSec(3, secV(3, fil.R / 2, fil.G / 2, fil.B / 2));
        uint8_t scan = (uint8_t)(t / 90);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool on = (((i + scan) % 6) < 3);
            setTargetPixel(sectionVisualLed(2, i), on ? secV(2, 0, 210, 135) : secV(2, 0, 36, 22));
        }
        setInsideAmbientRgb(fil, (uint8_t)(48 + sin8fast((uint8_t)(t / 24)) / 5), 28);
        break;
    }
    case IDLE_CALM_TIDE: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t wave = sin8fast((uint8_t)(t / 44U + i * 9U));
            uint8_t v = (uint8_t)(18U + (uint16_t)wave * 70U / 255U);
            uint8_t g = (uint8_t)(70U + (uint16_t)wave * 80U / 255U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), 0, g, v));
        }
        break;
    }
    case IDLE_ZEN_GARDEN: {
        uint8_t breath = (uint8_t)(44U + sin8fast((uint8_t)(t / 42U)) / 5U);
        fillSec(1, secV(1, 15, breath, 28));
        fillSec(2, secV(2, breath, (uint8_t)(breath * 4U / 5U), 38));
        fillSec(3, secV(3, 15, breath, 28));
        for (uint8_t k = 0; k < 5; k++) {
            uint16_t pos = (uint16_t)((k * 8U + (sin8fast((uint8_t)(t / 70U + k * 33U)) / 42U)) % OUTER_COUNT);
            setTargetPixel(OUTER_START + pos, secV(outerSectionIdx(OUTER_START + pos), 95, 115, 75));
        }
        break;
    }
    case IDLE_DUSK_HORIZON: {
        uint8_t drift = (uint8_t)(t / 95U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t h = (uint8_t)(8U + ((uint16_t)(i * 28U + drift) / OUTER_COUNT));
            uint8_t v = (uint8_t)(50U + sin8fast((uint8_t)(t / 38U + i * 5U)) / 6U);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 210, v));
        }
        break;
    }
    case IDLE_SILK_FLOW: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t ribbon = sin8fast((uint8_t)(t / 24U + i * 15U));
            RgbColor a(170, 55, 255), b(0, 210, 190);
            uint8_t r = (uint8_t)(((uint16_t)a.R * ribbon + (uint16_t)b.R * (255U - ribbon)) / 255U);
            uint8_t g = (uint8_t)(((uint16_t)a.G * ribbon + (uint16_t)b.G * (255U - ribbon)) / 255U);
            uint8_t bl = (uint8_t)(((uint16_t)a.B * ribbon + (uint16_t)b.B * (255U - ribbon)) / 255U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), r / 2, g / 2, bl / 2));
        }
        break;
    }
    case IDLE_NORTHERN_SLEEP: {
        fillSec(1, secV(1, 0, 7, 18));
        fillSec(2, secV(2, 4, 9, 22));
        fillSec(3, secV(3, 0, 7, 18));
        uint8_t open = (uint8_t)(sin8fast((uint8_t)(t / 72U)) / 36U);
        for (uint8_t pane = 0; pane < 6; pane++) {
            uint16_t center = (uint16_t)(pane * OUTER_COUNT / 6U + open);
            uint8_t v = (uint8_t)(52U + sin8fast((uint8_t)(t / 90U + pane * 41U)) / 4U);
            for (uint8_t d = 0; d < 2; d++) {
                uint16_t p = (center + d) % OUTER_COUNT;
                setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)(v / 4U), (uint8_t)(v / 2U), v));
            }
        }
        break;
    }
    case IDLE_DEW_SPARKS: {
        fillSec(1, secV(1, 0, 28, 32));
        fillSec(2, secV(2, 0, 34, 38));
        fillSec(3, secV(3, 0, 28, 32));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint8_t spark = extraSpark(i, t / 2U, 131);
            if (spark > 235U) {
                uint16_t idx = OUTER_START + i;
                setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(spark / 2U), spark, spark));
            }
        }
        break;
    }
    case IDLE_LAMP_GLOW: {
        uint8_t core = (uint8_t)(78U + sin8fast((uint8_t)(t / 50U)) / 7U);
        fillSec(2, secV(2, core, (uint8_t)(core * 5U / 7U), 18));
        fillSec(1, secV(1, (uint8_t)(core / 3U), (uint8_t)(core / 5U), 6));
        fillSec(3, secV(3, (uint8_t)(core / 3U), (uint8_t)(core / 5U), 6));
        break;
    }
    case IDLE_CLOUD_DRIFT: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t cloud = (uint8_t)((sin8fast((uint8_t)(t / 62U + i * 10U)) + sin8fast((uint8_t)(t / 91U + i * 17U))) / 2U);
            uint8_t v = (uint8_t)(18U + (uint16_t)cloud * 92U / 255U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(v * 3U / 5U), (uint8_t)(v * 4U / 5U), v));
        }
        break;
    }
    case IDLE_QUIET_COMET: {
        fillSec(1, secV(1, 0, 8, 14));
        fillSec(2, secV(2, 0, 10, 18));
        fillSec(3, secV(3, 0, 8, 14));
        uint8_t drift = (uint8_t)(t / 180U);
        for (uint8_t k = 0; k < 7; k++) {
            uint16_t p = (uint16_t)((k * 6U + ((k & 1U) ? drift : (OUTER_COUNT - drift))) % OUTER_COUNT);
            uint8_t v = (uint8_t)(58U + sin8fast((uint8_t)(t / 65U + k * 29U)) / 4U);
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)(v / 5U), v, (uint8_t)(v * 2U / 3U)));
        }
        break;
    }
    case IDLE_BREATHE_SECTIONS: {
        uint8_t tick = (uint8_t)((t / 1000UL) % 3U);
        fillSec(1, secV(1, tick == 0 ? 92 : 18, tick == 0 ? 70 : 14, 0));
        fillSec(2, secV(2, tick == 1 ? 60 : 14, tick == 1 ? 92 : 18, tick == 1 ? 40 : 8));
        fillSec(3, secV(3, tick == 2 ? 65 : 12, 0, tick == 2 ? 92 : 20));
        uint16_t marker = (uint16_t)((t / 240UL) % OUTER_COUNT);
        setTargetPixel(OUTER_START + marker, secV(outerSectionIdx(OUTER_START + marker), 170, 170, 150));
        break;
    }
    } // end switch idleAnim
    if (!ledAllowsInsideRgbNow()) renderInsideNormalToTarget();
}

// -- PRINT animations ---------------------------------------------------------

static void renderPrintTarget() {
    clearTarget();
    uint16_t frontLen = FRONT_END - FRONT_START + 1;
    uint32_t t = millis();
    // P3: In preview use demo values so animation is always meaningful
    const bool printPreview = ledAnimPreviewActive() && (gLedAnimPreview.cat == 1);
    const PrintAnimation effectivePrintAnim =
        (printPreview && gLedPreviewPrintAnimOverride >= 0 && gLedPreviewPrintAnimOverride < PRINT_COUNT)
            ? (PrintAnimation)gLedPreviewPrintAnimOverride
            : rtLed.printAnim;
    const uint8_t previewRampPct = (uint8_t)min((uint32_t)100, ((uint32_t)(t - gLedAnimPreview.startMs) * 100UL) / 10000UL);
    auto printPreviewNeedsProgressRamp = [&]() -> bool {
        switch (effectivePrintAnim) {
            case PRINT_PROGRESS:
            case PRINT_LASER_TIP:
            case PRINT_WIPE_PROG:
            case PRINT_SNAKE:
            case PRINT_LAYER:
            case PRINT_HEARTBEAT_PROG:
            case PRINT_THERMOMETER:
            case PRINT_PROGRESS_THEATER:
            case PRINT_LAYER_PULSE:
            case PRINT_TIME_REMAINING_FLOW:
            case PRINT_LAYER_FILL:
                return true;
            default:
                return false;
        }
    };
    auto printPreviewUsesTelemetryDemo = [&]() -> bool {
        switch (effectivePrintAnim) {
            case PRINT_THERMAL_BALANCE:
            case PRINT_MATERIAL_CORE:
            case PRINT_HEAT_SOAK:
            case PRINT_STABILITY_MONITOR:
            case PRINT_LAYER_ENGINE:
            case PRINT_TIME_TUNNEL:
            case PRINT_CHAMBER_AURA:
            case PRINT_FILAMENT_FLOW:
            case PRINT_PROCESS_STACK:
            case PRINT_HEALTH_BEACON:
            case PRINT_FINISH_PRESSURE:
            case PRINT_DUAL_TEMP_METER:
            case PRINT_LAYER_PULSE:
            case PRINT_TOOLPATH_ECHO:
            case PRINT_THERMAL_RIBBON:
            case PRINT_INFILL_GRID:
            case PRINT_FILAMENT_BEADS:
            case PRINT_TIME_REMAINING_FLOW:
            case PRINT_STEPPER_TICKS:
            case PRINT_CALM_BUILD:
            case PRINT_QUALITY_GUARD:
            case PRINT_NOZZLE_HEAT_TRACE:
                return true;
            default:
                return false;
        }
    };
    const bool telemetryPreview = printPreview && printPreviewUsesTelemetryDemo();
    const uint8_t previewTelemetryPct = (effectivePrintAnim == PRINT_FINISH_PRESSURE)
        ? (uint8_t)(82U + ((uint16_t)sin8fast((uint8_t)(t / 18U)) * 18U / 255U))
        : (uint8_t)(52U + ((uint16_t)sin8fast((uint8_t)(t / 24U)) * 24U / 255U));
    uint8_t  effPct = printPreview
        ? (printPreviewNeedsProgressRamp() ? previewRampPct : (telemetryPreview ? previewTelemetryPct : (uint8_t)62))
        : (uint8_t)rtPrinter.progress;
    if (printPreview && effPct < 3) effPct = 3;

    uint32_t effR   = printPreview ? gLedAnimPreview.demoFilR  : (uint32_t)rtPrinter.filamentColor.R;
    uint32_t effG   = printPreview ? gLedAnimPreview.demoFilG  : (uint32_t)rtPrinter.filamentColor.G;
    uint32_t effB   = printPreview ? gLedAnimPreview.demoFilB  : (uint32_t)rtPrinter.filamentColor.B;
    const float effBedTempC     = printPreview ? (58.0f + (float)sin8fast((uint8_t)(t / 32U)) * 20.0f / 255.0f) : rtVent.currentTempC;
    const float effChamberTempC = printPreview ? (34.0f + (float)sin8fast((uint8_t)(t / 44U + 70U)) * 14.0f / 255.0f) : rtVent.chamberTempC;
    const float effToolTempC    = printPreview ? (198.0f + (float)sin8fast((uint8_t)(t / 28U + 120U)) * 28.0f / 255.0f) : rtPrinter.activeToolTempC;
    const float effTargetTempC  = printPreview ? 72.0f  : rtVent.targetTempC;
    const bool  effOnline       = printPreview ? true   : rtPrinter.moonrakerOnline;
    const bool  effFailsafe     = printPreview ? false  : rtVent.failsafeActive;
    // BLACK_FILAMENT: replace near-black filament color with cycling rainbow so LEDs don't
    // look like they're off. Only in non-preview mode (preview uses explicit demo colour).
    if (!printPreview && (effR + effG + effB) < 30) {
        uint8_t rainbowHue = (uint8_t)(millis() / 20);
        RgbColor rc = hsvToRgb(rainbowHue, 255, 220);
        effR = rc.R; effG = rc.G; effB = rc.B;
    }
    uint16_t lit = (uint16_t)effPct * frontLen / 100;
    if (lit > frontLen) lit = frontLen;
    // P5: always show at least 1 LED when progress > 0
    if (effPct > 0 && lit == 0) lit = 1;
    auto filColor = [&](uint8_t sec) -> RgbwColor {
        uint8_t v = sectionBrightToV(rtLed.sectionBright[sec]);
        return rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                          (uint16_t)effG*v/255,
                                          (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT);
    };
    auto progressContrastColor = [&](uint8_t sec) -> RgbwColor {
        uint8_t h = 0, s = 0, v = 0;
        rgbToHsv8((uint8_t)effR, (uint8_t)effG, (uint8_t)effB, h, s, v);
        RgbColor contrast;
        if (v < 25 || s < 35) {
            contrast = hsvToRgb((uint8_t)(millis() / 18U), 255, 255);
        } else {
            contrast = hsvToRgb((uint8_t)(h + 128U), max((uint8_t)180, s), 255);
        }
        uint8_t sv = sectionBrightToV(rtLed.sectionBright[sec]);
        return rgbToRgbwLimited(RgbColor((uint16_t)contrast.R * sv / 255,
                                          (uint16_t)contrast.G * sv / 255,
                                          (uint16_t)contrast.B * sv / 255), OUT_MAX, W_MAX_OUT);
    };
    auto telemetryMeter = [&](uint8_t sec, uint8_t pct, const RgbColor& c, bool reverse = false, uint8_t phase = 0) {
        uint16_t count = (sec == 1) ? LEFT_COUNT : (sec == 2) ? FRONT_COUNT : RIGHT_COUNT;
        uint16_t litCount = (uint16_t)((uint32_t)pct * count / 100U);
        if (pct > 0 && litCount == 0) litCount = 1;
        if (litCount > count) litCount = count;
        uint16_t scan = (uint16_t)((t / 75UL + phase) % count);
        for (uint16_t i = 0; i < count; i++) {
            uint16_t logical = reverse ? (count - 1 - i) : i;
            uint16_t idx = sectionVisualLed(sec, i);
            uint8_t v = 3;
            if (logical < litCount) {
                uint8_t wave = sin8fast((uint8_t)(t / 18U + logical * 19U + phase * 13U));
                v = (uint8_t)(58U + ((uint16_t)wave * 118U / 255U));
            }
            uint16_t d = (logical > scan) ? (logical - scan) : (scan - logical);
            if (d <= 1) {
                uint16_t add = (d == 0) ? 84U : 38U;
                uint16_t boosted = (uint16_t)v + add;
                v = (boosted > 230U) ? 230 : (uint8_t)boosted;
            }
            setTargetPixel(idx, secV(sec,
                (uint8_t)((uint16_t)c.R * v / 255U),
                (uint8_t)((uint16_t)c.G * v / 255U),
                (uint8_t)((uint16_t)c.B * v / 255U)));
        }
    };

    switch (effectivePrintAnim) {
    case PRINT_PROGRESS: {
        fillSec(1, filColor(1)); fillSec(3, filColor(3));
        uint8_t h = 0, s = 0, v = 0;
        rgbToHsv8((uint8_t)effR, (uint8_t)effG, (uint8_t)effB, h, s, v);
        const bool whiteOrGrayFilament = (v >= 180 && s < 35);
        RgbwColor cC = progressContrastColor(2);
        for (uint16_t i = 0; i < frontLen; i++) {
            if (i >= lit) continue;
            if (whiteOrGrayFilament) {
                setTargetPixel(sectionVisualLed(2, i), secHSV(2, (uint8_t)(t / 18U + i * 10U), 255, OUT_MAX));
            } else {
                setTargetPixel(sectionVisualLed(2, i), cC);
            }
        }
        break;
    }
    case PRINT_LASER_TIP: {
        // Filament color stays on the side sections; the center becomes a real
        // scanning laser: dark field, thin beam, hot focus point and short trail.
        fillSec(1, filColor(1));
        fillSec(3, filColor(3));

        uint8_t laserFilH = 0, laserFilS = 0, laserFilV = 0;
        rgbToHsv8((uint8_t)effR, (uint8_t)effG, (uint8_t)effB, laserFilH, laserFilS, laserFilV);
        const bool filamentLooksRed = (laserFilV >= 35 && laserFilS >= 65 &&
                                      (laserFilH <= 18 || laserFilH >= 238));
        const uint8_t laserHue = filamentLooksRed ? 85 : 0; // red laser; green fallback for red filament
        const uint16_t progressTip = (lit > 0) ? (lit - 1U) : 0U;
        const uint16_t scanSpan = (lit > 1) ? lit : 1U;
        uint16_t scanPhase = (uint16_t)((t / 34U) % (scanSpan * 2U));
        uint16_t scanPos = (scanPhase < scanSpan) ? scanPhase : (uint16_t)(scanSpan * 2U - 1U - scanPhase);
        if (scanPos >= frontLen) scanPos = frontLen - 1U;

        for (uint16_t i = 0; i < frontLen; i++) {
            uint16_t idx = sectionVisualLed(2, i);
            uint8_t value = 0;
            uint8_t sat = 255;

            if (i < lit) {
                value = 7; // dim guide line showing already scanned progress
                if (((i + t / 130U) % 5U) == 0U) value = 18;
            }

            uint16_t distScan = (i > scanPos) ? (i - scanPos) : (scanPos - i);
            if (distScan <= 3U) {
                uint8_t beam = (distScan == 0U) ? OUT_MAX : (uint8_t)(150U - distScan * 38U);
                if (beam > value) value = beam;
            }

            uint16_t distTip = (i > progressTip) ? (i - progressTip) : (progressTip - i);
            if (lit > 0 && distTip <= 1U) {
                value = (distTip == 0U) ? OUT_MAX : max(value, (uint8_t)95);
                sat = (distTip == 0U) ? 180 : 255; // bright red focus point without turning into filament color
            }

            if (value > 0) setTargetPixel(idx, secHSV(2, laserHue, sat, value));
        }
        break;
    }
    case PRINT_WAVE: {
        uint16_t ph = (t/10) % 512;
        uint8_t breath = (ph<256) ? ph/2 : (511-ph)/2;
        uint8_t bv = (uint8_t)(60 + breath);
        auto wc = [&](uint8_t sec) {
            uint8_t sv = sectionBrightToV(rtLed.sectionBright[sec]);
            uint8_t v = (uint8_t)((uint16_t)bv * sv / 255);
            fillSec(sec, rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                                     (uint16_t)effG*v/255,
                                                     (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT));
        };
        wc(1); wc(3);
        uint8_t h = 0, s = 0, v = 0;
        rgbToHsv8((uint8_t)effR, (uint8_t)effG, (uint8_t)effB, h, s, v);
        const bool whiteOrGrayFilament = (v >= 180 && s < 35);
        const bool noUsableFilamentHue = (v < 25 || s < 35);
        const uint8_t waveBaseHue = noUsableFilamentHue ? (uint8_t)(t / 18U) : (uint8_t)(h + 128U);
        for (uint16_t i = 0; i < frontLen; i++) {
            if (i >= lit) continue;
            uint8_t waveA = sin8fast((uint8_t)(t / 18U + i * 26U));
            uint8_t waveB = sin8fast((uint8_t)(t / 29U + i * 13U + 85U));
            uint8_t crest = (uint8_t)(((uint16_t)waveA * 3U + waveB) / 4U);
            uint8_t value = (uint8_t)(24U + ((uint16_t)crest * (OUT_MAX - 24U) / 255U));
            uint8_t localHue = whiteOrGrayFilament
                ? (uint8_t)(t / 20U + i * 10U)
                : (uint8_t)(waveBaseHue + ((uint16_t)(crest > 128 ? crest - 128 : 128 - crest) * 10U / 128U));
            setTargetPixel(sectionVisualLed(2, i), secHSV(2, localHue, 255, value));
        }
        break;
    }
    case PRINT_THERMAL: {
        // Chamber thermal map on the side sections:
        // 20C = cold blue, 40C = balanced center gradient, 60C = hot red.
        // Below/above the range breathes faster as it approaches 0C/80C.
        float chamberC = isnan(effChamberTempC) ? 40.0f : effChamberTempC;
        if (chamberC < 0.0f) chamberC = 0.0f;
        if (chamberC > 80.0f) chamberC = 80.0f;

        uint8_t redAmount = 0;
        if (chamberC <= 20.0f) redAmount = 0;
        else if (chamberC >= 60.0f) redAmount = 255;
        else redAmount = (uint8_t)(((chamberC - 20.0f) * 255.0f) / 40.0f);

        uint8_t thermalValue = (uint8_t)(155U + (uint16_t)(fabsf(chamberC - 40.0f) * 65.0f / 40.0f));
        if (chamberC < 20.0f || chamberC > 60.0f) {
            float edge = (chamberC < 20.0f) ? (20.0f - chamberC) : (chamberC - 60.0f);
            uint16_t period = (uint16_t)(1700.0f - edge * 62.0f); // 1700ms near limit, ~460ms at 0/80C
            if (period < 460U) period = 460U;
            uint8_t breath = sin8fast((uint8_t)(t / max((uint16_t)2, (uint16_t)(period / 256U))));
            thermalValue = (uint8_t)((uint16_t)thermalValue * (145U + ((uint16_t)breath * 110U / 255U)) / 255U);
        }

        auto thermalSidePixel = [&](uint8_t sec, uint16_t idx, uint8_t heatPos) {
            const int16_t threshold = 255 - redAmount;
            const int16_t width = 58;
            int16_t mix = ((int16_t)heatPos - (threshold - width)) * 255 / (width * 2);
            if (mix < 0) mix = 0;
            if (mix > 255) mix = 255;
            uint8_t redMix = (uint8_t)mix;
            uint8_t r = redMix;
            uint8_t g = (uint8_t)((uint16_t)min(redMix, (uint8_t)(255U - redMix)) * 45U / 128U);
            uint8_t b = (uint8_t)(255U - redMix);
            setTargetPixel(idx, secV(sec,
                (uint8_t)((uint16_t)r * thermalValue / 255U),
                (uint8_t)((uint16_t)g * thermalValue / 255U),
                (uint8_t)((uint16_t)b * thermalValue / 255U)));
        };

        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t heatL = (uint8_t)(i * 255U / (LEFT_COUNT - 1U)); // rear/end cold -> front hot
            uint8_t heatR = (uint8_t)(i * 255U / (RIGHT_COUNT - 1U)); // same physical direction as left
            thermalSidePixel(1, sectionVisualLed(1, i), heatL);
            thermalSidePixel(3, sectionVisualLed(3, i), heatR);
        }
        RgbwColor cC = filColor(2);
        for (uint16_t i = 0; i < frontLen; i++) if (i < lit) setTargetPixel(sectionVisualLed(2, i), cC);
        break;
    }
    case PRINT_STRIPES: {
        fillSec(1, filColor(1)); fillSec(3, filColor(3));
        RgbColor stripeColors[4];
        bool loaded[4] = {false, false, false, false};
        for (uint8_t tool = 0; tool < 4; tool++) {
            bool hasMaterial = gCachedToolMaterials[tool].length() > 0 && gCachedToolMaterials[tool] != "-";
            bool hasColor = ((uint16_t)gCachedToolColors[tool].R + gCachedToolColors[tool].G + gCachedToolColors[tool].B) > 25U;
            loaded[tool] = !printPreview && hasMaterial && hasColor;
            stripeColors[tool] = loaded[tool] ? gCachedToolColors[tool] : RgbColor(0, 0, 0);
        }
        if (printPreview) {
            stripeColors[0] = RgbColor(255, 120, 0);
            stripeColors[1] = RgbColor(0, 210, 255);
            stripeColors[2] = RgbColor(150, 70, 255);
            stripeColors[3] = RgbColor(0, 230, 120);
            for (uint8_t tool = 0; tool < 4; tool++) loaded[tool] = true;
        }
        auto hueDistance = [](uint8_t a, uint8_t b) -> uint8_t {
            uint8_t d = (a > b) ? (a - b) : (b - a);
            return (d > 127U) ? (uint8_t)(255U - d) : d;
        };
        for (uint8_t tool = 0; tool < 4; tool++) {
            if (loaded[tool]) continue;
            uint8_t h = (uint8_t)(37U + tool * 61U);
            for (uint8_t tries = 0; tries < 8; tries++) {
                bool tooClose = false;
                for (uint8_t other = 0; other < 4; other++) {
                    if (!loaded[other]) continue;
                    uint8_t oh = 0, os = 0, ov = 0;
                    rgbToHsv8(stripeColors[other].R, stripeColors[other].G, stripeColors[other].B, oh, os, ov);
                    if (ov > 25U && os > 35U && hueDistance(h, oh) < 28U) {
                        tooClose = true;
                        break;
                    }
                }
                if (!tooClose) break;
                h = (uint8_t)(h + 47U);
            }
            stripeColors[tool] = hsvToRgb(h, 235, 255);
            loaded[tool] = true;
        }
        uint8_t sv = sectionBrightToV(rtLed.sectionBright[2]);
        uint32_t sp = t / 95;
        for (uint16_t i = 0; i < frontLen; i++) {
            if (i >= lit) continue;
            uint8_t tool = (uint8_t)((i + sp) % 4U);
            uint8_t shimmer = sin8fast((uint8_t)(t / 13U + i * 31U + tool * 47U));
            uint8_t v = (uint8_t)(((uint16_t)sv * (90U + ((uint16_t)shimmer * 165U / 255U))) / 255U);
            const RgbColor& c = stripeColors[tool];
            setTargetPixel(sectionVisualLed(2, i), rgbToRgbwLimited(RgbColor((uint16_t)c.R*v/255,
                                                                       (uint16_t)c.G*v/255,
                                                                       (uint16_t)c.B*v/255), OUT_MAX, W_MAX_OUT));
        }
        break;
    }
    case PRINT_PULSE_PROG: {
        uint16_t spd = (uint16_t)(800 - (uint32_t)effPct * 6);
        uint16_t ph2 = (t % max((uint32_t)50,(uint32_t)spd)) * 512 / max((uint32_t)50,(uint32_t)spd);
        uint8_t bv2 = (ph2<256) ? ph2/2 : (511-ph2)/2;
        for (uint8_t sec=1; sec<=3; sec++) {
            uint8_t sv = sectionBrightToV(rtLed.sectionBright[sec]);
            uint8_t v = (uint8_t)((uint16_t)bv2 * sv / 255);
            fillSec(sec, rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                                     (uint16_t)effG*v/255,
                                                     (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT));
        }
        // Stable progress marker: the progress LED and its neighbours do not
        // pulse, so the user can read the approximate print position at a glance.
        uint16_t marker = (uint16_t)(((uint32_t)effPct * (FRONT_COUNT - 1U) + 50U) / 100U);
        if (marker >= FRONT_COUNT) marker = FRONT_COUNT - 1U;
        for (int8_t d = -1; d <= 1; d++) {
            int16_t pos = (int16_t)marker + d;
            if (pos < 0 || pos >= (int16_t)FRONT_COUNT) continue;
            setTargetPixel(sectionVisualLed(2, (uint16_t)pos), filColor(2));
        }
        break;
    }
    case PRINT_COMET: {
        // millis-based: comet advances one step per 30ms
        static const uint32_t COMET_STEP_MS = 30;
        static uint32_t lastCometStep = 0;
        static uint16_t cpos = 0;
        EXT_RAM_BSS_ATTR static uint8_t ctail[OUTER_COUNT] = {};
        if (t - lastCometStep >= COMET_STEP_MS) {
            lastCometStep = t - ((t - lastCometStep) % COMET_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) ctail[i] = (uint8_t)((uint16_t)ctail[i]*200/255);
            ctail[cpos] = 255;
            if (++cpos >= OUTER_COUNT) cpos = 0;
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START+i;
            uint8_t sv = sectionBrightToV(rtLed.sectionBright[outerSectionIdx(idx)]);
            uint8_t v = (uint8_t)((uint16_t)ctail[i]*sv/255);
            setTargetPixel(idx, rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                                             (uint16_t)effG*v/255,
                                                             (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT));
        }
        break;
    }
    case PRINT_ACTIVE_SEC: {
        RgbwColor cC = filColor(2);
        for (uint16_t i = 0; i < frontLen; i++) if (i < lit) setTargetPixel(sectionVisualLed(2, i), cC);
        uint8_t density = (uint8_t)(18U + (uint16_t)effPct * 150U / 100U);
        uint16_t tempo = (uint16_t)(190U - (uint16_t)effPct * 130U / 100U);
        if (tempo < 45U) tempo = 45U;
        uint32_t tick = t / tempo;
        for (uint8_t side = 0; side < 2; side++) {
            uint16_t start = side == 0 ? LEFT_START : RIGHT_START;
            uint16_t count = side == 0 ? LEFT_COUNT : RIGHT_COUNT;
            uint8_t sec = side == 0 ? 1 : 3;
            for (uint16_t i = 0; i < count; i++) {
                uint8_t seed = extraSpark(i + side * 37U, tick * 41U, 123);
                uint8_t after = extraSpark(i + side * 53U, tick * 23U + 7U, 211);
                if (seed < density) {
                    uint8_t hue = (uint8_t)(after + tick * 17U + i * 29U);
                    uint8_t v = (uint8_t)(80U + (uint16_t)extraSpark(i, tick * 13U, 77) * 150U / 255U);
                    setTargetPixel(start + i, secHSV(sec, hue, 255, v));
                } else if (effPct > 75 && after < (uint8_t)(effPct - 55U)) {
                    uint8_t hue = (uint8_t)(after + i * 31U);
                    setTargetPixel(start + i, secHSV(sec, hue, 210, 35));
                }
            }
        }
        break;
    }
    case PRINT_RUNNING: {
        fillSec(1, filColor(1)); fillSec(3, filColor(3));
        uint8_t sv = sectionBrightToV(rtLed.sectionBright[2]);
        uint32_t sp2 = t / 60;
        for (uint16_t i = 0; i < frontLen; i++) {
            bool on = ((i + sp2) % 5 < 3);
            uint8_t v = on ? sv : (uint8_t)(sv/6);
            setTargetPixel(sectionVisualLed(2, i), rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                                                       (uint16_t)effG*v/255,
                                                                       (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT));
        }
        break;
    }
    case PRINT_BREATHE_FIL: {
        uint16_t ph = (t/8) % 1024;
        uint8_t bv = (ph<512) ? ph/4 : (1023-ph)/4;
        bv += 60;
        for (uint8_t sec=1; sec<=3; sec++) {
            uint8_t sv = sectionBrightToV(rtLed.sectionBright[sec]);
            uint8_t v = (uint8_t)((uint16_t)bv * sv / 255);
            fillSec(sec, rgbToRgbwLimited(RgbColor((uint16_t)effR*v/255,
                                                     (uint16_t)effG*v/255,
                                                     (uint16_t)effB*v/255), OUT_MAX, W_MAX_OUT));
        }
        break;
    }
    case PRINT_WIPE_PROG: {
        auto windyFilamentSide = [&](uint8_t sec) {
            const uint16_t start = (sec == 1) ? LEFT_START : RIGHT_START;
            const uint16_t count = (sec == 1) ? LEFT_COUNT : RIGHT_COUNT;
            for (uint16_t i = 0; i < count; i++) {
                uint16_t logical = (sec == 1) ? i : (count - 1U - i);
                uint8_t waveA = sin8fast((uint8_t)(t / 34U + logical * 17U));
                uint8_t waveB = sin8fast((uint8_t)(t / 57U + logical * 9U + 80U));
                uint8_t weave = (uint8_t)(((uint16_t)waveA * 3U + waveB) / 4U);
                uint8_t v = (uint8_t)(150U + ((uint16_t)weave * 70U / 255U));
                setTargetPixel(start + i, secV(sec,
                    (uint8_t)((uint16_t)effR * v / 255U),
                    (uint8_t)((uint16_t)effG * v / 255U),
                    (uint8_t)((uint16_t)effB * v / 255U)));
            }
        };

        windyFilamentSide(1);
        windyFilamentSide(3);

        uint8_t h = 0, s = 0, v = 0;
        rgbToHsv8((uint8_t)effR, (uint8_t)effG, (uint8_t)effB, h, s, v);
        const bool whiteOrGrayFilament = (v >= 180 && s < 35);
        const bool noUsableFilamentHue = (v < 25 || s < 35);
        const uint8_t curtainHue = noUsableFilamentHue ? (uint8_t)(t / 16U) : (uint8_t)(h + 128U);
        const uint16_t pairCount = (FRONT_COUNT + 1U) / 2U;
        const uint16_t fadeProgress = (uint16_t)effPct * pairCount * 256U / 100U;

        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t distFromEdge = (i < (FRONT_COUNT - 1U - i)) ? i : (FRONT_COUNT - 1U - i);
            uint16_t pairFromCenter = pairCount - 1U - distFromEdge;
            uint16_t pairFadeStart = pairFromCenter * 256U;
            uint8_t alpha = 0;
            if (fadeProgress <= pairFadeStart) {
                alpha = 255;
            } else {
                uint16_t elapsed = fadeProgress - pairFadeStart;
                alpha = (elapsed >= 256U) ? 0 : (uint8_t)(255U - elapsed);
            }
            if (alpha == 0) continue;

            uint8_t ripple = sin8fast((uint8_t)(t / 42U + i * 13U));
            uint8_t value = (uint8_t)(((uint16_t)alpha * (182U + ((uint16_t)ripple * 42U / 255U))) / 255U);
            if (value == 0) continue;
            uint8_t localHue = whiteOrGrayFilament ? (uint8_t)(curtainHue + i * 9U) : curtainHue;
            setTargetPixel(sectionVisualLed(2, i), secHSV(2, localHue, 245, value));
        }
        break;
    }
    case PRINT_SHIMMER: {
        // WARN-2 fix: shimmer extras sampled once per 40ms, not per-frame per-LED
        static const uint32_t SHM_STEP_MS = 40;
        static uint32_t lastShmStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t shmExtra[OUTER_COUNT] = {};
        if (t - lastShmStep >= SHM_STEP_MS) {
            lastShmStep = t - ((t - lastShmStep) % SHM_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++)
                shmExtra[i] = (random(20) == 0) ? 60 : 0;
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t sec = outerSectionIdx(idx);
            uint8_t extra = shmExtra[i];
            uint8_t sv = sectionBrightToV(rtLed.sectionBright[sec]);
            setTargetPixel(idx, rgbToRgbwLimited(
                RgbColor((uint8_t)min((uint32_t)255, effR*sv/255+extra),
                         (uint8_t)min((uint32_t)255, effG*sv/255+extra),
                         (uint8_t)min((uint32_t)255, effB*sv/255+extra)), OUT_MAX, W_MAX_OUT));
        }
        break;
    }
    case PRINT_BICOLOR: {
        fillSec(1, filColor(1)); fillSec(3, filColor(3));
        lv_color32_t acc; acc.full = navAccentColor().full;
        uint32_t sp3 = t / 70;
        for (uint16_t i = 0; i < frontLen; i++) {
            bool useFil = ((i + sp3) % 4 < 2);
            if (useFil) setTargetPixel(sectionVisualLed(2, i), filColor(2));
            else setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)(acc.ch.red), (uint8_t)(acc.ch.green), (uint8_t)(acc.ch.blue)));
        }
        break;
    }
    case PRINT_SNAKE: {
        // Snake chases mice on the 42 outer LEDs.
        // Grows by 1 when: ate a mouse AND progress increased by >=5% since last grow.
        // Explodes (full outer strip flash) when printer state becomes 'complete'.
        static const uint32_t SNAKE_STEP_MS = 60;   // speed per cell
        static uint32_t snakeLastStep  = 0;
        // Snake body: positions in outer strip index (0..OUTER_COUNT-1)
        static const uint8_t SNAKE_MAX = 22;
        EXT_RAM_BSS_ATTR static uint8_t snakeBody[SNAKE_MAX] = {};
        static uint8_t  snakeLen       = 2;          // starts at 2
        static int8_t   snakeDir       = 1;           // +1 or -1
        static uint8_t  snakeHead      = 0;
        static uint8_t  mousePos       = OUTER_COUNT / 2; // initial mouse position
        static uint8_t  lastGrowPct    = 0;           // progress% at last grow
        static bool     snakeInit      = false;
        static bool     snakeExploding = false;
        static uint32_t snakeExplodeMs = 0;

        const uint8_t curPct = (uint8_t)effPct;

        // Reset when entering preview mode or when preview just started (startMs changed)
        static bool     snakePrevPreview = false;
        static uint32_t snakeLastStartMs = 0;
        if (!snakeInit || (printPreview && !snakePrevPreview) ||
            (printPreview && gLedAnimPreview.startMs != snakeLastStartMs)) {
            snakeInit = true;
            snakePrevPreview = printPreview;
            snakeLastStartMs = gLedAnimPreview.startMs;
            snakeLen  = 2;
            snakeDir  = 1;
            snakeHead = 0;
            lastGrowPct = 0;
            snakeExploding = false;
            for (uint8_t k = 0; k < SNAKE_MAX; k++)
                snakeBody[k] = (OUTER_COUNT - k) % OUTER_COUNT;
            mousePos = (uint8_t)(random(0, OUTER_COUNT));
        }
        snakePrevPreview = printPreview;

        if (gSnakeFinishBurstActive && !snakeExploding) {
            snakeExploding = true;
            snakeExplodeMs = t;
            snakeLen = SNAKE_MAX;
        }

        // Explosion when print complete (state transitions away from 'printing')
        // We detect it by effPct being fed a sentinel 255 from the caller - but
        // simpler: once snakeLen == SNAKE_MAX we trigger explode on next step.
        // Actually trigger explode if progress has not changed for 3s (print done).
        // Cleanest: let the caller reset via snakeInit when mode changes.

        if (snakeExploding) {
            // Rapidly flash the full outer strip white then black
            const uint32_t age = t - snakeExplodeMs;
            uint8_t v = 0;
            if (age < 200) v = 255;
            else if (age < 400) v = 0;
            else if (age < 600) v = 220;
            else if (age < 800) v = 0;
            else if (age < 1050) v = 180;
            else if (age < 1250) v = (uint8_t)(255 - ((age - 1050) * 255UL / 200UL));
            else { snakeExploding = false; }
            for (uint16_t i = 0; i < OUTER_COUNT; i++) {
                uint16_t idx = OUTER_START + i;
                uint8_t burst = v;
                if (age >= 800 && age < 1250) {
                    uint8_t spark = extraSpark(i, t, 91);
                    if (spark > 190) {
                        uint16_t mixed = (uint16_t)burst + (uint16_t)(spark / 3);
                        burst = (mixed > 255U) ? 255 : (uint8_t)mixed;
                    }
                }
                setTargetPixel(idx, secV(outerSectionIdx(idx), burst, burst, burst));
            }
            break;
        }

        if ((uint32_t)(t - snakeLastStep) >= SNAKE_STEP_MS) {
            snakeLastStep = t - ((t - snakeLastStep) % SNAKE_STEP_MS);

            // Advance head
            snakeHead = (uint8_t)((snakeHead + OUTER_COUNT + snakeDir) % OUTER_COUNT);
            // Shift body
            for (uint8_t k = SNAKE_MAX - 1; k > 0; k--) snakeBody[k] = snakeBody[k-1];
            snakeBody[0] = snakeHead;

            // Ate mouse?
            if (snakeHead == mousePos) {
                // Move mouse to a new random position not on the snake
                uint8_t newMouse;
                uint8_t tries = 0;
                do {
                    newMouse = (uint8_t)(random(0, OUTER_COUNT));
                    bool onSnake = false;
                    for (uint8_t k = 0; k < snakeLen && k < SNAKE_MAX; k++)
                        if (snakeBody[k] == newMouse) { onSnake = true; break; }
                    if (!onSnake) { mousePos = newMouse; break; }
                } while (++tries < 30);

                // Grow if also progress grew by >=5%
                if (curPct >= lastGrowPct + 5 && snakeLen < SNAKE_MAX) {
                    snakeLen++;
                    lastGrowPct = curPct;
                }

                // Reaching max size only means "fully fed" during printing.
                // The actual burst is reserved for the finish gate so Snake
                // cannot randomly flash white while a print is still running.
            }

            // Reverse direction occasionally at ends (simple wall bounce)
            if (snakeHead == 0 || snakeHead == OUTER_COUNT - 1) snakeDir = -snakeDir;
        }

        // Render
        // Clear outer strip
        for (uint16_t i = 0; i < OUTER_COUNT; i++)
            setTargetPixel(OUTER_START + i, RgbwColor(0, 0, 0, 0));

        // Mouse: warm food marker, not pure white; the white burst is reserved
        // for the print-finish explosion.
        bool mouseVisible = (t / 300) % 2 == 0;
        if (mouseVisible) {
            uint16_t mouseIdx = OUTER_START + mousePos;
            uint8_t pulse = (uint8_t)(130U + ((uint16_t)sin8fast((uint8_t)(t / 8U)) * 80U / 255U));
            setTargetPixel(mouseIdx, secV(outerSectionIdx(mouseIdx), pulse, (uint8_t)(pulse / 3U), 0));
        }

        // Snake body: gradient head=bright, tail=dim using effR/G/B
        for (uint8_t k = 0; k < snakeLen && k < SNAKE_MAX; k++) {
            uint8_t fade = (uint8_t)(255 - (k * 255 / max((uint8_t)1, snakeLen)));
            uint8_t r = (uint16_t)effR * fade / 255;
            uint8_t g = (uint16_t)effG * fade / 255;
            uint8_t b = (uint16_t)effB * fade / 255;
            uint8_t pos = snakeBody[k];
            setTargetPixel(OUTER_START + pos, rgbToRgbwLimited(RgbColor(r, g, b), 255, W_MAX_OUT));
        }
        break;
    }

    case PRINT_LAYER: {
        // Fill outer strip LED by LED proportional to progress
        uint16_t lit=(uint16_t)((uint32_t)effPct*OUTER_COUNT/100);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t h=(uint8_t)(i*255/OUTER_COUNT);
            setTargetPixel(idx,i<lit?secHSV(outerSectionIdx(idx),h,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        break;
    }
    case PRINT_LAYER_FILL: {
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t waveL = sin8fast((uint8_t)(t / 34U + i * 18U));
            uint8_t waveR = sin8fast((uint8_t)(t / 39U + i * 18U + 90U));
            uint8_t vL = (uint8_t)(70U + ((uint16_t)waveL * 55U / 255U));
            uint8_t vR = (uint8_t)(70U + ((uint16_t)waveR * 55U / 255U));
            setTargetPixel(sectionVisualLed(1, i), secV(1,
                (uint8_t)((uint16_t)effR * vL / 255U),
                (uint8_t)((uint16_t)effG * vL / 255U),
                (uint8_t)((uint16_t)effB * vL / 255U)));
            setTargetPixel(sectionVisualLed(3, i), secV(3,
                (uint8_t)((uint16_t)effR * vR / 255U),
                (uint8_t)((uint16_t)effG * vR / 255U),
                (uint8_t)((uint16_t)effB * vR / 255U)));
        }

        uint16_t active = (lit >= frontLen) ? (frontLen - 1U) : lit;
        uint16_t nozzle = (uint16_t)((t / 75U) % frontLen);
        for (uint16_t i = 0; i < frontLen; i++) {
            uint16_t idx = sectionVisualLed(2, i);
            bool completed = (i < lit);
            bool currentLayer = (i == active && effPct < 100U);
            uint8_t layerStripe = (i % 2U) ? 18U : 0U;
            uint8_t v = completed ? (uint8_t)(118U + layerStripe) : 18U;
            if (currentLayer) v = 210U;
            if (completed) {
                uint16_t d = (nozzle > i) ? (nozzle - i) : (i - nozzle);
                if (d <= 1U) {
                    uint16_t boosted = (uint16_t)v + 70U - (d * 26U);
                    v = (uint8_t)((boosted > 255U) ? 255U : boosted);
                }
            }
            if (completed || currentLayer) {
                uint8_t heat = currentLayer ? 34U : 0U;
                uint16_t rMix = (uint16_t)((uint16_t)effR * v / 255U) + heat;
                uint16_t gMix = (uint16_t)((uint16_t)effG * v / 255U) + (heat / 3U);
                setTargetPixel(idx, secV(2,
                    (uint8_t)((rMix > 255U) ? 255U : rMix),
                    (uint8_t)((gMix > 255U) ? 255U : gMix),
                    (uint8_t)((uint16_t)effB * v / 255U)));
            } else {
                setTargetPixel(idx, secV(2,
                    (uint8_t)((uint16_t)effR * v / 255U),
                    (uint8_t)((uint16_t)effG * v / 255U),
                    (uint8_t)((uint16_t)effB * v / 255U)));
            }
        }
        break;
    }
    case PRINT_HEARTBEAT_PROG: {
        // Double-pulse, brightness proportional to progress
        uint32_t ph=(t%1200);
        uint8_t v=0;
        if(ph<80)v=(uint8_t)(ph*3);
        else if(ph<160)v=(uint8_t)((160-ph)*3);
        else if(ph<280)v=(uint8_t)((ph-160)*2);
        else if(ph<400)v=(uint8_t)((400-ph)*2);
        v=(uint8_t)((uint16_t)v*effPct/100);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,(uint16_t)effR*v/255,(uint16_t)effG*v/255,(uint16_t)effB*v/255));
        break;
    }
    case PRINT_DNA: {
        // Two interlocking helices along outer strip
        uint8_t hA=(uint8_t)(t/20),hB=hA+128;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            float phase=(float)i*6.283f/8.0f+(float)t*0.003f;
            float sA=sinf(phase),sB=sinf(phase+3.1416f);
            uint8_t vA=(sA>0)?(uint8_t)(sA*OUT_MAX):0;
            uint8_t vB=(sB>0)?(uint8_t)(sB*OUT_MAX):0;
            RgbColor cA=hsvToRgb(hA,255,vA),cB=hsvToRgb(hB,255,vB);
            setTargetPixel(idx,rgbToRgbwLimited(RgbColor(cA.R+cB.R,cA.G+cB.G,cA.B+cB.B),255,W_MAX_OUT));
        }
        break;
    }
    case PRINT_PIXEL_RAIN: {
        // Filament-color drops fall down left and right strips
        static const uint32_t RAIN_STEP=50;
        static uint32_t lastRain=0;
        EXT_RAM_BSS_ATTR static uint8_t rainL[LEFT_COUNT]={},rainR[RIGHT_COUNT]={};
        if((uint32_t)(t-lastRain)>=RAIN_STEP){
            lastRain=t-((t-lastRain)%RAIN_STEP);
            for(int i=(int)LEFT_COUNT-1;i>0;i--){rainL[i]=rainL[i-1];rainR[i]=rainR[i-1];}
            rainL[0]=(random(0,3)==0)?255:0;
            rainR[0]=(random(0,3)==0)?255:0;
        }
        for(uint16_t i=0;i<LEFT_COUNT;i++){
            uint8_t vL=(uint8_t)((uint32_t)rainL[i]*effR/255),gL=(uint8_t)((uint32_t)rainL[i]*effG/255),bL=(uint8_t)((uint32_t)rainL[i]*effB/255);
            uint8_t vR=(uint8_t)((uint32_t)rainR[i]*effR/255),gR=(uint8_t)((uint32_t)rainR[i]*effG/255),bR=(uint8_t)((uint32_t)rainR[i]*effB/255);
            setTargetPixel(sectionVisualLed(1, i),  rgbToRgbwLimited(RgbColor(vL,gL,bL),255,W_MAX_OUT));
            setTargetPixel(sectionVisualLed(3, i), rgbToRgbwLimited(RgbColor(vR,gR,bR),255,W_MAX_OUT));
        }
        for (uint16_t i = 0; i < frontLen; i++) {
            uint16_t idx = sectionVisualLed(2, i);
            if (i >= lit) {
                setTargetPixel(idx, secV(2,
                    (uint8_t)((uint16_t)effR * 10U / 255U),
                    (uint8_t)((uint16_t)effG * 10U / 255U),
                    (uint8_t)((uint16_t)effB * 10U / 255U)));
                continue;
            }
            uint8_t wave = sin8fast((uint8_t)(t / 31U + i * 19U));
            uint8_t shimmer = (((i * 11U + t / 90U) % 17U) == 0U) ? 32U : 0U;
            uint16_t v = 34U + ((uint16_t)wave * 28U / 255U) + shimmer;
            if (i + 1U == lit && effPct < 100U) v += 48U;
            if (v > 130U) v = 130U;
            setTargetPixel(idx, secV(2,
                (uint8_t)((uint16_t)effR * v / 255U),
                (uint8_t)((uint16_t)effG * v / 255U),
                (uint8_t)((uint16_t)effB * v / 255U)));
        }
        break;
    }
    case PRINT_CLOCKWISE: {
        // Single bright dot orbits outer strip clockwise
        uint16_t pos=(uint32_t)(t/40)%OUTER_COUNT;
        uint16_t trail=4;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<=pos)?(pos-i):(OUTER_COUNT+pos-i);
            uint8_t v=(dist<trail)?(uint8_t)(OUT_MAX-(dist*OUT_MAX/trail)):0;
            setTargetPixel(idx,v>0?secV(outerSectionIdx(idx),(uint16_t)effR*v/255,(uint16_t)effG*v/255,(uint16_t)effB*v/255):RgbwColor(0,0,0,0));
        }
        break;
    }
    case PRINT_THERMOMETER: default: {
        uint8_t bedPct = clampPctFloat(effBedTempC, 20.0f, 90.0f, effPct);
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 20.0f, 70.0f, bedPct / 2);
        uint8_t toolPct = clampPctFloat(effToolTempC, 30.0f, 260.0f, 55);
        RgbColor bedC = tempColorFromPct(bedPct);
        RgbColor chamberC = tempColorFromPct(chamberPct);
        RgbColor toolC = tempColorFromPct(toolPct);

        fillSectionMeter(1, bedPct, bedC, true);
        fillSectionMeter(3, chamberPct, chamberC, false);

        for (uint16_t i = 0; i < frontLen; i++) {
            uint16_t idx = sectionVisualLed(2, i);
            uint8_t tempFill = (uint8_t)((uint32_t)(i + 1U) * 100U / frontLen);
            if (tempFill > toolPct) {
                setTargetPixel(idx, secV(2, (uint8_t)(toolC.R / 12U), (uint8_t)(toolC.G / 12U), (uint8_t)(toolC.B / 12U)));
                continue;
            }
            uint8_t baseV = (i < lit) ? 245 : 115;
            uint8_t pulse = (i < lit) ? (uint8_t)(sin8fast((uint8_t)(t / 24U + i * 17U)) / 8U) : 0;
            uint16_t vSum = (uint16_t)baseV + (uint16_t)pulse;
            uint8_t v = (uint8_t)((vSum > 255U) ? 255U : vSum);
            setTargetPixel(idx, secV(2,
                (uint8_t)((uint16_t)toolC.R * v / 255U),
                (uint8_t)((uint16_t)toolC.G * v / 255U),
                (uint8_t)((uint16_t)toolC.B * v / 255U)));
        }
        break;
    }
    case PRINT_EXTRUDER_SPARK:   { renderOuterExtraPattern(5, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_LAYER_SCAN:       { renderOuterExtraPattern(17, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_HEAT_RIPPLE:      { renderOuterExtraPattern(4, t, 18, 255, 0, 0, 0, false, effPct); break; }
    case PRINT_FILAMENT_COMETS:  { renderOuterExtraPattern(6, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_PROGRESS_THEATER: { renderOuterExtraPattern(9, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_NOZZLE_TRACE:     { renderOuterExtraPattern(1, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_BUILD_PLATE:      { renderOuterExtraPattern(12, t, 25, 220, 0, 0, 0, false, effPct); break; }
    case PRINT_MICRO_STEPS:      { renderOuterExtraPattern(11, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_FLOW_WAVE:        { renderOuterExtraPattern(8, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_TOOLHEAD_ORBIT:   { renderOuterExtraPattern(2, t, 0, 255, (uint8_t)effR, (uint8_t)effG, (uint8_t)effB, true, effPct); break; }
    case PRINT_THERMAL_BALANCE: {
        uint8_t bedPct = clampPctFloat(effBedTempC, 20.0f, 90.0f, effPct);
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 25.0f, 65.0f, bedPct / 2);
        RgbColor bedC = tempColorFromPct(bedPct);
        RgbColor chamberC = tempColorFromPct(chamberPct);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t wave = sin8fast((uint8_t)(t / 24U + i * 18U));
            uint8_t v = (uint8_t)(32U + (uint16_t)wave * bedPct / 255U);
            setTargetPixel(sectionVisualLed(1, i), secV(1, (uint8_t)((uint16_t)bedC.R * v / 255U), (uint8_t)((uint16_t)bedC.G * v / 255U), (uint8_t)((uint16_t)bedC.B * v / 255U)));
            wave = sin8fast((uint8_t)(t / 31U + (RIGHT_COUNT - 1 - i) * 18U + 80U));
            v = (uint8_t)(32U + (uint16_t)wave * chamberPct / 255U);
            setTargetPixel(sectionVisualLed(3, i), secV(3, (uint8_t)((uint16_t)chamberC.R * v / 255U), (uint8_t)((uint16_t)chamberC.G * v / 255U), (uint8_t)((uint16_t)chamberC.B * v / 255U)));
        }
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t mix = (uint8_t)(i * 255U / (FRONT_COUNT - 1U));
            RgbColor c(
                (uint8_t)(((uint16_t)bedC.R * (255U - mix) + (uint16_t)chamberC.R * mix) / 255U),
                (uint8_t)(((uint16_t)bedC.G * (255U - mix) + (uint16_t)chamberC.G * mix) / 255U),
                (uint8_t)(((uint16_t)bedC.B * (255U - mix) + (uint16_t)chamberC.B * mix) / 255U));
            uint8_t wave = sin8fast((uint8_t)(t / 18U + i * 13U));
            uint8_t v = (uint8_t)(36U + (uint16_t)wave * 150U / 255U);
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)c.R * v / 255U), (uint8_t)((uint16_t)c.G * v / 255U), (uint8_t)((uint16_t)c.B * v / 255U)));
        }
        setInsideAmbientRgb(tempColorFromPct((uint8_t)((bedPct + chamberPct) / 2)), 115, 30);
        break;
    }
    case PRINT_MATERIAL_CORE: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint8_t pulse = (uint8_t)(80 + sin8fast((uint8_t)(t / 12)) / 2);
        fillSec(1, secV(1, (uint8_t)((uint16_t)fil.R * 42U / 255U), (uint8_t)((uint16_t)fil.G * 42U / 255U), (uint8_t)((uint16_t)fil.B * 42U / 255U)));
        fillSec(3, secV(3, (uint8_t)((uint16_t)fil.R * 42U / 255U), (uint8_t)((uint16_t)fil.G * 42U / 255U), (uint8_t)((uint16_t)fil.B * 42U / 255U)));
        uint16_t core = (uint16_t)((t / 55UL) % FRONT_COUNT);
        for (uint8_t tr = 0; tr < 8; tr++) {
            uint16_t posA = (core + tr) % FRONT_COUNT;
            uint16_t posB = (FRONT_COUNT - 1U) - ((core + tr) % FRONT_COUNT);
            uint8_t v = (uint8_t)((uint16_t)pulse * (8U - tr) / 8U);
            setTargetPixel(sectionVisualLed(2, posA), secV(2, (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
            setTargetPixel(sectionVisualLed(2, posB), secV(2, (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        setInsideAmbientRgb(fil, pulse, 20);
        break;
    }
    case PRINT_HEAT_SOAK: {
        float heatHigh = (effTargetTempC > 30.0f) ? effTargetTempC : 30.0f;
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 25.0f, heatHigh, 0);
        RgbColor heatC = tempColorFromPct(chamberPct);
        uint16_t waveHead = (uint16_t)((t / 80UL) % (OUTER_COUNT + 8U));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d = (waveHead > i) ? (waveHead - i) : (i - waveHead);
            uint8_t base = (uint8_t)(18U + chamberPct / 3U);
            uint16_t boosted = (d < 9U) ? ((uint16_t)base + (uint16_t)(9U - d) * 18U) : base;
            uint8_t v = (boosted > 220U) ? 220 : (uint8_t)boosted;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)heatC.R * v / 255U), (uint8_t)((uint16_t)heatC.G * v / 255U), (uint8_t)((uint16_t)heatC.B * v / 255U)));
        }
        setInsideAmbientRgb(tempColorFromPct(chamberPct), (uint8_t)(60 + chamberPct), (uint8_t)(chamberPct / 2));
        break;
    }
    case PRINT_STABILITY_MONITOR: {
        static float lastTool = NAN, lastChamber = NAN;
        float toolDelta = (!isnan(lastTool) && !isnan(effToolTempC)) ? fabsf(effToolTempC - lastTool) : 0.0f;
        float chamberDelta = (!isnan(lastChamber) && !isnan(effChamberTempC)) ? fabsf(effChamberTempC - lastChamber) : 0.0f;
        if ((t % 1000UL) < 40UL) { lastTool = effToolTempC; lastChamber = effChamberTempC; }
        uint8_t jitter = (uint8_t)min((uint32_t)100, (uint32_t)((toolDelta * 18.0f) + (chamberDelta * 35.0f)));
        if (printPreview) jitter = (uint8_t)(18U + ((uint16_t)sin8fast((uint8_t)(t / 14U)) * 42U / 255U));
        RgbColor calm(0, 170, 220);
        RgbColor warnC(255, 95, 0);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t spark = extraSpark(i, t, 37);
            bool warn = (jitter > 35) && (spark > 210);
            uint8_t wave = sin8fast((uint8_t)(t / 30U + i * 7U));
            uint8_t v = warn ? spark : (uint8_t)(20U + (uint16_t)wave * 58U / 255U);
            RgbColor c = warn ? warnC : calm;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)c.R * v / 255U), (uint8_t)((uint16_t)c.G * v / 255U), (uint8_t)((uint16_t)c.B * v / 255U)));
        }
        break;
    }
    case PRINT_LAYER_ENGINE: {
        uint8_t phase = (uint8_t)(t / 70);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t v = (((i + phase) % 5) < 2) ? 210 : 24;
            setTargetPixel(sectionVisualLed(1, i), secV(1, (uint8_t)((uint16_t)effR * v / 255), (uint8_t)((uint16_t)effG * v / 255), (uint8_t)((uint16_t)effB * v / 255)));
            setTargetPixel(sectionVisualLed(3, i), secV(3, (uint8_t)((uint16_t)effR * v / 255), (uint8_t)((uint16_t)effG * v / 255), (uint8_t)((uint16_t)effB * v / 255)));
        }
        uint16_t layer = (uint16_t)((t / 95UL) % FRONT_COUNT);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t d = (layer > i) ? layer - i : i - layer;
            uint8_t v = (d < 4U) ? (uint8_t)(220U - d * 42U) : (uint8_t)((i < (uint16_t)((uint32_t)effPct * FRONT_COUNT / 100U)) ? 28U : 4U);
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)effR * v / 255U), (uint8_t)((uint16_t)effG * v / 255U), (uint8_t)((uint16_t)effB * v / 255U)));
        }
        break;
    }
    case PRINT_TIME_TUNNEL: {
        uint8_t speedDrop = (effPct / 2 > 70) ? 70 : (uint8_t)(effPct / 2);
        uint8_t speed = (uint8_t)(90 - speedDrop);
        uint16_t stepMs = (speed < 15) ? 15 : speed;
        uint16_t head = (uint16_t)((t / stepMs) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t d = (head >= i) ? head - i : OUTER_COUNT + head - i;
            if (d > OUTER_COUNT / 2U) d = OUTER_COUNT - d;
            uint8_t base = (uint8_t)(8U + effPct / 5U);
            setTargetPixel(OUTER_START + i, secV(outerSectionIdx(OUTER_START + i), (uint8_t)((uint16_t)effR * base / 255U), (uint8_t)((uint16_t)effG * base / 255U), (uint8_t)((uint16_t)effB * base / 255U)));
            if (d < 8) {
                uint16_t idx = OUTER_START + i;
                uint8_t v = (uint8_t)(220 - d * 24);
                setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)effR * v / 255), (uint8_t)((uint16_t)effG * v / 255), (uint8_t)((uint16_t)effB * v / 255)));
            }
        }
        break;
    }
    case PRINT_CHAMBER_AURA: {
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 25.0f, 65.0f, 25);
        RgbColor aura = tempColorFromPct(chamberPct);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t wave = sin8fast((uint8_t)(t / 34U + i * 11U));
            uint8_t v = (uint8_t)(24U + (uint16_t)wave * (80U + chamberPct) / 255U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)aura.R * v / 255U), (uint8_t)((uint16_t)aura.G * v / 255U), (uint8_t)((uint16_t)aura.B * v / 255U)));
        }
        uint16_t centerGlow = sectionVisualLed(2, FRONT_COUNT / 2U);
        setTargetPixel(centerGlow, secV(2, aura.R, aura.G, aura.B));
        setInsideAmbientRgb(tempColorFromPct(chamberPct), (uint8_t)(85 + sin8fast((uint8_t)(t / 20)) / 3), 35);
        break;
    }
    case PRINT_FILAMENT_FLOW: {
        uint16_t headL = (uint16_t)((t / 45U) % LEFT_COUNT);
        uint16_t headR = (uint16_t)((t / 45U) % RIGHT_COUNT);
        for (uint8_t trail = 0; trail < 5; trail++) {
            uint8_t v = (uint8_t)(220 - trail * 38);
            setTargetPixel(sectionVisualLed(1, (headL + LEFT_COUNT - trail) % LEFT_COUNT), secV(1, (uint8_t)((uint16_t)effR * v / 255), (uint8_t)((uint16_t)effG * v / 255), (uint8_t)((uint16_t)effB * v / 255)));
            setTargetPixel(sectionVisualLed(3, (headR + trail) % RIGHT_COUNT), secV(3, (uint8_t)((uint16_t)effR * v / 255), (uint8_t)((uint16_t)effG * v / 255), (uint8_t)((uint16_t)effB * v / 255)));
        }
        uint16_t flow = (uint16_t)((t / 52UL) % FRONT_COUNT);
        for (uint8_t tr = 0; tr < 7; tr++) {
            uint8_t v = (uint8_t)(210U - tr * 25U);
            uint16_t a = sectionVisualLed(2, ((flow + tr) % FRONT_COUNT));
            uint16_t b = sectionVisualLed(2, ((FRONT_COUNT - 1U + FRONT_COUNT - ((flow + tr) % FRONT_COUNT)) % FRONT_COUNT));
            setTargetPixel(a, secV(2, (uint8_t)((uint16_t)effR * v / 255U), (uint8_t)((uint16_t)effG * v / 255U), (uint8_t)((uint16_t)effB * v / 255U)));
            setTargetPixel(b, secV(2, (uint8_t)((uint16_t)effR * v / 255U), (uint8_t)((uint16_t)effG * v / 255U), (uint8_t)((uint16_t)effB * v / 255U)));
        }
        break;
    }
    case PRINT_PROCESS_STACK: {
        uint8_t thermal = clampPctFloat(effChamberTempC, 25.0f, 65.0f, clampPctFloat(effBedTempC, 20.0f, 90.0f, 0));
        telemetryMeter(1, 100, RgbColor((uint8_t)effR, (uint8_t)effG, (uint8_t)effB), true, 2);
        telemetryMeter(2, effPct, RgbColor(0, 220, 255), false, 17);
        telemetryMeter(3, thermal, tempColorFromPct(thermal), false, 7);
        uint16_t packet = (uint16_t)((t / 85UL) % OUTER_COUNT);
        setTargetPixel(OUTER_START + packet, secV(outerSectionIdx(OUTER_START + packet), 255, 255, 255));
        setInsideAmbientRgb(tempColorFromPct(thermal), 80, 45);
        break;
    }
    case PRINT_HEALTH_BEACON: {
        bool staleTool = rtPrinter.activeToolTempUpdatedMs && (millis() - rtPrinter.activeToolTempUpdatedMs > 20000UL);
        bool warn = !effOnline || effFailsafe || (!printPreview && staleTool);
        uint8_t beat = ((t % 1200UL) < 120UL || (t % 1200UL > 220UL && t % 1200UL < 340UL)) ? 230 : 26;
        RgbColor c = warn ? RgbColor(255, 70, 0) : RgbColor(0, 210, 135);
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, (uint8_t)((uint16_t)c.R * beat / 255U), (uint8_t)((uint16_t)c.G * beat / 255U), (uint8_t)((uint16_t)c.B * beat / 255U)));
        uint16_t beacon = (uint16_t)((t / (warn ? 35UL : 90UL)) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 6; tr++) {
            uint16_t p = (beacon + OUTER_COUNT - tr) % OUTER_COUNT;
            uint8_t v = (uint8_t)(220U - tr * 32U);
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)((uint16_t)c.R * v / 255U), (uint8_t)((uint16_t)c.G * v / 255U), (uint8_t)((uint16_t)c.B * v / 255U)));
        }
        break;
    }
    case PRINT_FINISH_PRESSURE: {
        uint8_t pressure = (effPct < 80) ? 0 : (uint8_t)((effPct - 80) * 255 / 20);
        uint8_t breath = (uint8_t)((uint16_t)sin8fast((uint8_t)(t / 8)) * pressure / 255);
        uint8_t sideV = (uint8_t)(80 + breath);
        fillSec(1, secV(1, (uint8_t)(effR * sideV / 255), (uint8_t)(effG * sideV / 255), (uint8_t)(effB * sideV / 255)));
        fillSec(3, secV(3, (uint8_t)(effR * sideV / 255), (uint8_t)(effG * sideV / 255), (uint8_t)(effB * sideV / 255)));
        uint16_t squeeze = (uint16_t)((uint32_t)pressure * (FRONT_COUNT / 2U) / 255U);
        uint16_t center = FRONT_COUNT / 2U;
        for (uint16_t i = 0; i <= squeeze && i < center; i++) {
            uint8_t v = (uint8_t)(90U + (uint16_t)pressure * (center - i) / center);
            setTargetPixel(sectionVisualLed(2, center - i), secV(2, (uint8_t)((uint16_t)effR * v / 255U), (uint8_t)((uint16_t)effG * v / 255U), (uint8_t)((uint16_t)effB * v / 255U)));
            if (center + i < FRONT_COUNT) setTargetPixel(sectionVisualLed(2, center + i), secV(2, (uint8_t)((uint16_t)effR * v / 255U), (uint8_t)((uint16_t)effG * v / 255U), (uint8_t)((uint16_t)effB * v / 255U)));
        }
        break;
    }
    case PRINT_DUAL_TEMP_METER: {
        uint8_t toolPct = clampPctFloat(effToolTempC, 30.0f, 260.0f, effPct);
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 25.0f, 65.0f, 0);
        telemetryMeter(1, chamberPct, tempColorFromPct(chamberPct), true, 5);
        telemetryMeter(3, toolPct, tempColorFromPct(toolPct), false, 9);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool leftHalf = i < (FRONT_COUNT / 2U);
            uint8_t pct = leftHalf ? chamberPct : toolPct;
            uint16_t local = leftHalf ? i : (i - FRONT_COUNT / 2U);
            uint16_t half = FRONT_COUNT / 2U;
            uint8_t litLocal = (uint8_t)((uint32_t)pct * half / 100U);
            RgbColor c = tempColorFromPct(pct);
            uint8_t v = (local < litLocal) ? (uint8_t)(80U + sin8fast((uint8_t)(t / 17U + local * 21U)) / 2U) : 5U;
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)c.R * v / 255U), (uint8_t)((uint16_t)c.G * v / 255U), (uint8_t)((uint16_t)c.B * v / 255U)));
        }
        setInsideAmbientRgb(tempColorFromPct((uint8_t)((toolPct + chamberPct) / 2)), 105, 30);
        break;
    }
    case PRINT_LAYER_PULSE: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        fillSectionMeter(2, effPct, fil);
        uint16_t layer = (uint16_t)((t / 130UL) % FRONT_COUNT);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t d = (layer > i) ? (layer - i) : (i - layer);
            if (d < 5U) {
                uint8_t v = (uint8_t)(220U - d * 35U);
                setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
            }
        }
        uint8_t sidePulse = sin8fast((uint8_t)(t / 18U + effPct));
        fillSec(1, secV(1, (uint8_t)((uint16_t)fil.R * sidePulse / 640U), (uint8_t)((uint16_t)fil.G * sidePulse / 640U), (uint8_t)((uint16_t)fil.B * sidePulse / 640U)));
        fillSec(3, secV(3, (uint8_t)((uint16_t)fil.R * sidePulse / 640U), (uint8_t)((uint16_t)fil.G * sidePulse / 640U), (uint8_t)((uint16_t)fil.B * sidePulse / 640U)));
        break;
    }
    case PRINT_TOOLPATH_ECHO: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint16_t head = (uint16_t)((t / 42UL) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d = (head >= i) ? (head - i) : (OUTER_COUNT + head - i);
            uint8_t base = (uint8_t)(6U + effPct / 5U);
            uint8_t v = (d < 9U) ? (uint8_t)(220U - d * 22U) : base;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        uint16_t echo = (uint16_t)((uint32_t)effPct * FRONT_COUNT / 100U);
        if (echo >= FRONT_COUNT) echo = FRONT_COUNT - 1U;
        setTargetPixel(sectionVisualLed(2, echo), secV(2, 255, 255, 255));
        break;
    }
    case PRINT_THERMAL_RIBBON: {
        uint8_t bedPct = clampPctFloat(effBedTempC, 20.0f, 90.0f, effPct);
        uint8_t toolPct = clampPctFloat(effToolTempC, 30.0f, 260.0f, bedPct);
        RgbColor bedC = tempColorFromPct(bedPct);
        RgbColor toolC = tempColorFromPct(toolPct);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t w = sin8fast((uint8_t)(t / 22U + i * 21U));
            uint8_t v = (uint8_t)(45U + w / 2U);
            setTargetPixel(sectionVisualLed(1, i), secV(1, (uint8_t)((uint16_t)bedC.R * v / 255U), (uint8_t)((uint16_t)bedC.G * v / 255U), (uint8_t)((uint16_t)bedC.B * v / 255U)));
            setTargetPixel(sectionVisualLed(3, i), secV(3, (uint8_t)((uint16_t)toolC.R * v / 255U), (uint8_t)((uint16_t)toolC.G * v / 255U), (uint8_t)((uint16_t)toolC.B * v / 255U)));
        }
        fillSectionMeter(2, effPct, RgbColor((uint8_t)effR, (uint8_t)effG, (uint8_t)effB));
        break;
    }
    case PRINT_INFILL_GRID: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint8_t grid = (uint8_t)(t / 110U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool cross = (((i + grid) % 7U) == 0U) || (((i * 3U + grid) % 11U) == 0U);
            uint8_t v = cross ? 185 : (uint8_t)(18U + effPct / 4U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        break;
    }
    case PRINT_FILAMENT_BEADS: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        fillSec(1, secV(1, fil.R / 9, fil.G / 9, fil.B / 9));
        fillSec(2, secV(2, fil.R / 11, fil.G / 11, fil.B / 11));
        fillSec(3, secV(3, fil.R / 9, fil.G / 9, fil.B / 9));
        uint16_t step = (uint16_t)(t / 68UL);
        for (uint8_t bead = 0; bead < 7; bead++) {
            uint16_t p = (step + bead * 6U) % OUTER_COUNT;
            uint8_t v = (uint8_t)(115U + ((bead & 1U) ? 55U : 95U));
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        break;
    }
    case PRINT_TIME_REMAINING_FLOW: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint16_t period = (uint16_t)(115U - (effPct > 90U ? 90U : effPct));
        if (period < 24U) period = 24U;
        uint16_t head = (uint16_t)((t / period) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool doneSide = i < (uint16_t)((uint32_t)effPct * OUTER_COUNT / 100U);
            uint16_t d = (head >= i) ? head - i : OUTER_COUNT + head - i;
            uint8_t v = doneSide ? 64 : 10;
            if (d < 6U) v = (uint8_t)(230U - d * 28U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        break;
    }
    case PRINT_STEPPER_TICKS: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint8_t phase = (uint8_t)(t / 36U);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            uint8_t lWave = sin8fast((uint8_t)(phase + i * 35U));
            uint8_t rWave = sin8fast((uint8_t)(phase + 64U + (RIGHT_COUNT - 1U - i) * 35U));
            uint8_t vL = (uint8_t)(18U + (lWave > 180U ? (lWave - 180U) * 3U : 0U));
            uint8_t vR = (uint8_t)(18U + (rWave > 180U ? (rWave - 180U) * 3U : 0U));
            setTargetPixel(sectionVisualLed(1, i), secV(1, (uint8_t)((uint16_t)fil.R * vL / 255U), (uint8_t)((uint16_t)fil.G * vL / 255U), (uint8_t)((uint16_t)fil.B * vL / 255U)));
            setTargetPixel(sectionVisualLed(3, i), secV(3, (uint8_t)((uint16_t)fil.R * vR / 255U), (uint8_t)((uint16_t)fil.G * vR / 255U), (uint8_t)((uint16_t)fil.B * vR / 255U)));
        }
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t gate = (((i + phase / 3U) % 5U) == 0U) ? 150 : 14;
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)fil.R * gate / 255U), (uint8_t)((uint16_t)fil.G * gate / 255U), (uint8_t)((uint16_t)fil.B * gate / 255U)));
        }
        break;
    }
    case PRINT_CALM_BUILD: {
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint8_t terraces = (uint8_t)(3U + effPct / 14U);
        fillSec(1, secV(1, fil.R / 7, fil.G / 7, fil.B / 7));
        fillSec(3, secV(3, fil.R / 7, fil.G / 7, fil.B / 7));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t band = (uint8_t)((i * terraces) / FRONT_COUNT);
            uint8_t shimmer = sin8fast((uint8_t)(t / 55U + band * 37U));
            uint8_t v = (uint8_t)(28U + band * 16U + shimmer / 8U);
            if (v > 180U) v = 180U;
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        break;
    }
    case PRINT_QUALITY_GUARD: {
        bool warn = effFailsafe || !effOnline;
        uint8_t chamberPct = clampPctFloat(effChamberTempC, 25.0f, 65.0f, 0);
        if (chamberPct > 80U) warn = true;
        RgbColor ok(0, 210, 135), alert(255, 70, 0);
        RgbColor c = warn ? alert : ok;
        uint8_t breathe = (uint8_t)(42U + sin8fast((uint8_t)(t / (warn ? 10U : 34U))) / (warn ? 2U : 5U));
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, (uint8_t)((uint16_t)c.R * breathe / 255U), (uint8_t)((uint16_t)c.G * breathe / 255U), (uint8_t)((uint16_t)c.B * breathe / 255U)));
        uint16_t sweep = (uint16_t)((t / (warn ? 55UL : 140UL)) % FRONT_COUNT);
        setTargetPixel(sectionVisualLed(2, sweep), secV(2, 255, 255, 255));
        break;
    }
    case PRINT_NOZZLE_HEAT_TRACE: {
        uint8_t toolPct = clampPctFloat(effToolTempC, 30.0f, 260.0f, effPct);
        RgbColor heat = tempColorFromPct(toolPct);
        RgbColor fil((uint8_t)effR, (uint8_t)effG, (uint8_t)effB);
        uint16_t nozzle = (uint16_t)((sin8fast((uint8_t)(t / 26U)) * (FRONT_COUNT - 1U)) / 255U);
        fillSec(1, secV(1, heat.R / 4, heat.G / 4, heat.B / 4));
        fillSec(3, secV(3, fil.R / 6, fil.G / 6, fil.B / 6));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t d = (nozzle > i) ? nozzle - i : i - nozzle;
            uint8_t v = (d < 5U) ? (uint8_t)(230U - d * 38U) : (uint8_t)(10U + toolPct / 9U);
            RgbColor c = (i < nozzle) ? fil : heat;
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)c.R * v / 255U), (uint8_t)((uint16_t)c.G * v / 255U), (uint8_t)((uint16_t)c.B * v / 255U)));
        }
        uint16_t hotL = (uint16_t)((uint32_t)toolPct * LEFT_COUNT / 100U);
        uint16_t hotR = (uint16_t)((uint32_t)toolPct * RIGHT_COUNT / 100U);
        for (uint16_t i = 0; i < hotL && i < LEFT_COUNT; i++) setTargetPixel(LEFT_END - i, secV(1, heat.R, heat.G, heat.B));
        for (uint16_t i = 0; i < hotR && i < RIGHT_COUNT; i++) setTargetPixel(sectionVisualLed(3, i), secV(3, heat.R, heat.G, heat.B));
        break;
    }
    }
    if (!ledAllowsInsideRgbNow()) renderInsideNormalToTarget();
}

// -- PAUSE animations ---------------------------------------------------------

static void renderPauseTarget() {
    clearTarget();
    uint32_t t = millis();
    switch (rtLed.pauseAnim) {
    case PAUSE_AMBER: {
        uint16_t ph = (t/2) % 1024;
        uint8_t breath = (ph<512) ? ph/4 : (1023-ph)/4;
        uint8_t v = (uint8_t)(50 + breath);
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec, v, (uint8_t)(v*65/100), (uint8_t)(v*10/100)));
        break;
    }
    case PAUSE_BLINK_LR: {
        bool lOn = (t/500)%2 == 0;
        uint8_t v = 200;
        if (lOn) fillSec(1, secV(1, v, (uint8_t)(v*65/100), 0));
        else      fillSec(3, secV(3, v, (uint8_t)(v*65/100), 0));
        uint16_t ph2 = (t/4) % 1024;
        uint8_t bv = (ph2<512) ? ph2/5 : (1023-ph2)/5;
        fillSec(2, secV(2, (uint8_t)(30+bv), (uint8_t)((30+bv)*65/100), 0));
        break;
    }
    case PAUSE_FREEZE: {
        uint16_t ph = (t/8) % 1024;
        uint8_t bv = (ph<512) ? ph/4 : (1023-ph)/4;
        uint8_t v = (uint8_t)(40 + bv/2);
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec, (uint8_t)(v/3), (uint8_t)(v*2/3), v));
        break;
    }
    case PAUSE_RADAR: {
        // millis-based: one step per 40ms
        static const uint32_t RADAR_STEP_MS = 40;
        EXT_RAM_BSS_ATTR static uint8_t rtail[OUTER_COUNT] = {};
        static uint32_t lastRadarStep = 0;
        static int16_t rpos = 0;
        if (t - lastRadarStep >= RADAR_STEP_MS) {
            lastRadarStep = t - ((t - lastRadarStep) % RADAR_STEP_MS);
            for (uint16_t i=0;i<OUTER_COUNT;i++) rtail[i]=(uint8_t)((uint16_t)rtail[i]*200/255);
            rtail[rpos]=255;
            if (++rpos>=(int16_t)OUTER_COUNT) rpos=0;
        }
        for (uint16_t i=0;i<OUTER_COUNT;i++) {
            uint16_t idx=OUTER_START+i;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)rtail[i]*180/255), (uint8_t)((uint16_t)rtail[i]*130/255), 0));
        }
        break;
    }
    case PAUSE_HEARTBEAT: {
        // Beat pattern: on(80) off(80) on(80) pause(700)
        uint32_t cycle = t % 940;
        bool on = (cycle < 80) || (cycle >= 160 && cycle < 240);
        uint8_t v = on ? 200 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec, v, (uint8_t)(v*2/10), 0));
        break;
    }
    case PAUSE_PROGRESS_BAR: {
        // millis-based: bouncing position moves every 60ms
        static const uint32_t PB_STEP_MS = 60;
        static uint32_t lastPbStep = 0;
        static int16_t ppos = 0;
        static int8_t  pdir = 1;
        if (t - lastPbStep >= PB_STEP_MS) {
            lastPbStep = t - ((t - lastPbStep) % PB_STEP_MS);
            ppos += pdir; if (ppos >= (int16_t)(FRONT_COUNT - 5) || ppos<=0) pdir=-pdir;
        }
        uint8_t sv = sectionBrightToV(rtLed.sectionBright[2]);
        for (uint16_t i=0;i<FRONT_COUNT;i++) {
            bool in = ((int16_t)i >= ppos && (int16_t)i < ppos+5);
            uint8_t v = in ? sv : (uint8_t)(sv/8);
            setTargetPixel(sectionVisualLed(2, i), secV(2, (uint8_t)((uint16_t)180*v/255), (uint8_t)((uint16_t)120*v/255), 0));
        }
        fillSec(1, secV(1, 40, 25, 0)); fillSec(3, secV(3, 40, 25, 0));
        break;
    }
    case PAUSE_CROSSFADE: {
        // millis-based: full 256-hue cycle = ~4s (was t/20/8 = 41s!)
        uint8_t h = (uint8_t)(t / 16);
        for (uint8_t sec=1;sec<=3;sec++) {
            uint8_t sh = h + sec * 20;
            fillSec(sec, secHSV(sec, sh, 200, 160));
        }
        break;
    }
    case PAUSE_PHASE: {
        for (uint8_t sec=1;sec<=3;sec++) {
            uint32_t ph = (t + (uint32_t)(sec-1)*330) % 1024;
            uint8_t bv = (ph<512) ? ph/4 : (1023-ph)/4;
            fillSec(sec, secV(sec, (uint8_t)(20+bv), (uint8_t)((20+bv)*65/100), 0));
        }
        break;
    }
    case PAUSE_YELLOW_WHITE: {
        // millis-based: t/6 per tick, full cycle = 512*6 = 3.07s (was 10s)
        uint32_t ywT = t / 6;
        uint16_t ph = ywT % 512;
        uint8_t bv = (ph<256) ? ph/2 : (511-ph)/2;
        // lerp yellow->white by breath
        for (uint8_t sec=1;sec<=3;sec++) {
            uint8_t v = sectionBrightToV(rtLed.sectionBright[sec]);
            uint8_t r = (uint8_t)((uint16_t)v * min((uint32_t)255, (uint32_t)(200u + bv/2)) / 255);
            uint8_t g = (uint8_t)((uint16_t)v * min((uint32_t)255, (uint32_t)(160u + bv/2)) / 255);
            uint8_t b2 = (uint8_t)((uint16_t)v * bv / 255);
            fillSec(sec, rgbToRgbwLimited(RgbColor(r,g,b2), 255, W_MAX_OUT));
        }
        break;
    }
    case PAUSE_ICU: {
        // millis-based: eye moves every 160ms
        static const uint32_t ICU_STEP_MS = 160;
        static uint32_t lastIcuStep = 0;
        static int16_t eyeL = 2, eyeR = 7;
        static int8_t edL = 1, edR = -1;
        if (t - lastIcuStep >= ICU_STEP_MS) {
            lastIcuStep = t - ((t - lastIcuStep) % ICU_STEP_MS);
            eyeL += edL; if (eyeL>=10||eyeL<=0) edL=-edL;
            eyeR += edR; if (eyeR>=10||eyeR<=0) edR=-edR;
        }
        fillSec(1, secV(1, 15, 10, 0));
        fillSec(3, secV(3, 15, 10, 0));
        setTargetPixel(sectionVisualLed(1, eyeL),  secV(1, 220, 220, 0));
        setTargetPixel(sectionVisualLed(3, eyeR), secV(3, 220, 220, 0));
        fillSec(2, secV(2, 30, 20, 0));
        break;
    }
    case PAUSE_STROBE_AMBER: {
        bool on = (t%200 < 50);
        uint8_t v = on ? 220 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec, v, (uint8_t)(v*65/100), 0));
        break;
    }
    case PAUSE_ZIGZAG: {
        bool lHigh = (t/400)%2 == 0;
        fillSec(1, secV(1, lHigh?200:30, lHigh?130:20, 0));
        fillSec(3, secV(3, lHigh?30:200, lHigh?20:130, 0));
        fillSec(2, secV(2, 20, 13, 0));
        break;
    }
    case PAUSE_NEON_SIGN: default: {
        uint32_t phase = (t/500)%4;
        for (uint8_t sec=1;sec<=3;sec++) {
            bool on = (phase == sec-1) || (phase==3);
            uint8_t h = (sec==1)?0:(sec==2)?85:170;
            fillSec(sec, on ? secHSV(sec,h,255,200) : secV(sec,5,5,5));
        }
        break;
    }
    case PAUSE_SANDCLOCK: {
        // Hourglass: two halves drain to center, then refill
        uint32_t ph=t%4000;
        uint16_t half=LEFT_COUNT;
        uint16_t fill=(ph<2000)?(uint16_t)(ph*half/2000):(uint16_t)((4000-ph)*half/2000);
        for(uint16_t i=0;i<LEFT_COUNT;i++){
            bool onL=((int16_t)i >= ((int16_t)LEFT_COUNT - 1 - (int16_t)fill));
            bool onR=(i<=fill);
            setTargetPixel(sectionVisualLed(1, i),  onL?secHSV(1,30,255,OUT_MAX):RgbwColor(0,0,0,0));
            setTargetPixel(sectionVisualLed(3, i), onR?secHSV(3,30,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        fillSec(2,secV(2,0,0,0));
        break;
    }
    case PAUSE_AMBER_WAVE: {
        // Slow amber sine wave rolling through outer strip
        uint8_t offset=(uint8_t)(t/25);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t v=(uint8_t)(128+127*sinf((float)(i*16+offset)*3.1416f/128.0f));
            setTargetPixel(idx,rgbToRgbwLimited(RgbColor(v,(uint8_t)(v*2/5),0),255,W_MAX_OUT));
        }
        break;
    }
    case PAUSE_BOUNCE_WAIT: {
        // Single ball bouncing across front strip
        static float bpos2=0,bvel2=0.4f;
        static uint32_t lastBW=0;
        if((uint32_t)(t-lastBW)>=20){lastBW=t-(t-lastBW)%20;
            bpos2+=bvel2;if(bpos2>=19||bpos2<=0)bvel2=-bvel2;}
        fillSec(2,secV(2,8,8,8));
        fillSec(1,secHSV(1,30,255,sectionBrightToV(rtLed.sectionBright[1])));
        fillSec(3,secHSV(3,30,255,sectionBrightToV(rtLed.sectionBright[3])));
        setTargetPixel(sectionVisualLed(2, (uint8_t)bpos2),secHSV(2,200,255,OUT_MAX));
        break;
    }
    case PAUSE_COMET_SLOW: {
        // Single slow comet orbiting outer strip
        uint16_t pos=(uint32_t)(t/80)%OUTER_COUNT;
        uint16_t trail=6;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<=pos)?(pos-i):(OUTER_COUNT+pos-i);
            uint8_t v=(dist<trail)?(uint8_t)(OUT_MAX*2/3-(dist*OUT_MAX/trail/2)):0;
            setTargetPixel(idx,v>0?secHSV(outerSectionIdx(idx),30,255,v):RgbwColor(0,0,0,0));
        }
        break;
    }
    case PAUSE_SPINNER: {
        // Loading spinner: short arc rotating around outer strip
        uint16_t head=(uint32_t)(t/40)%OUTER_COUNT;
        uint16_t arcLen=8;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<=head)?(head-i):(OUTER_COUNT+head-i);
            uint8_t v=(dist<arcLen)?(uint8_t)(OUT_MAX*(arcLen-dist)/arcLen):0;
            setTargetPixel(idx,v?secHSV(outerSectionIdx(idx),200,200,v):RgbwColor(0,0,0,0));
        }
        break;
    }
    case PAUSE_MORSE_WAIT: {
        // Flashes WAIT in Morse: .-- .- ..  -
        // W=.-- A=.- I=.. T=-  pattern: 0110011001100011001101110 (ticks of 150ms)
        static const uint8_t MORSE[]={1,0,1,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,1,0,0,0};
        static const uint8_t MLEN=24;
        uint8_t tick=(uint8_t)((t/150)%MLEN);
        uint8_t v=MORSE[tick]?OUT_MAX:0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v,v,v));
        break;
    }
    case PAUSE_BREATHE_BLUE: {
        // Calm blue breathe all sections
        uint32_t ph=t%4000;
        uint16_t half=2000;
        float s=(ph<half)?((float)ph/half):(1.0f-(float)(ph-half)/half);
        uint8_t v=(uint8_t)(30+195*s*s);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,160,255,v));
        break;
    }
    case PAUSE_SOFT_HOLD:      { renderOuterExtraPattern(16, t, 28, 220, 0, 0, 0, false); break; }
    case PAUSE_AMBER_THEATER:  { renderOuterExtraPattern(0,  t, 32, 255, 0, 0, 0, false); break; }
    case PAUSE_BREATHING_DOTS: { renderOuterExtraPattern(2,  t / 2, 35, 230, 0, 0, 0, false); break; }
    case PAUSE_WAITING_RIPPLE: { renderOuterExtraPattern(4,  t / 2, 30, 240, 0, 0, 0, false); break; }
    case PAUSE_PARKING_LIGHTS: { renderOuterExtraPattern(7,  t / 2, 28, 255, 0, 0, 0, false); break; }
    case PAUSE_DIM_SPARKS:     { renderOuterExtraPattern(5,  t / 2, 36, 190, 0, 0, 0, false); break; }
    case PAUSE_SLOW_SCAN:      { renderOuterExtraPattern(17, t / 2, 32, 255, 0, 0, 0, false); break; }
    case PAUSE_FROZEN_GOLD:    { renderOuterExtraPattern(13, t / 2, 38, 150, 0, 0, 0, false); break; }
    case PAUSE_CLOCK_TICK:     { renderOuterExtraPattern(11, t / 2, 34, 255, 0, 0, 0, false); break; }
    case PAUSE_CALM_ORBIT:     { renderOuterExtraPattern(1,  t / 2, 30, 200, 0, 0, 0, false); break; }
    case PAUSE_HOLDING_PATTERN: {
        fillSectionMeter(2, rtPrinter.progress, RgbColor(255, 145, 0));
        fillSec(1, secV(1, 70, 45, 0)); fillSec(3, secV(3, 70, 45, 0));
        break;
    }
    case PAUSE_BREATHING_AMBER: {
        uint8_t v = (uint8_t)(65 + sin8fast((uint8_t)(t / 18)) / 2);
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, v, (uint8_t)(v * 2 / 3), 0));
        break;
    }
    case PAUSE_RESUME_GATE: {
        uint8_t open = sin8fast((uint8_t)(t / 20));
        uint16_t spread = (uint16_t)((uint32_t)open * (OUTER_COUNT / 2) / 255U);
        uint16_t center = OUTER_COUNT / 2;
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d = (i > center) ? i - center : center - i;
            uint16_t falloff = d * 8U;
            if (falloff > 170U) falloff = 170U;
            uint8_t v = (d <= spread) ? (uint8_t)(210U - falloff) : 12;
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 2 / 3), 0));
        }
        setInsideAmbientRgb(RgbColor(255, 145, 0), 70, 35);
        break;
    }
    case PAUSE_TEMP_KEEPALIVE: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 30.0f, 260.0f, 0);
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 25.0f, 65.0f, 0);
        fillSectionMeter(1, toolPct, tempColorFromPct(toolPct), true);
        fillSectionMeter(3, chamberPct, tempColorFromPct(chamberPct), false);
        fillSectionMeter(2, rtPrinter.progress, RgbColor(255, 145, 0));
        break;
    }
    case PAUSE_ATTENTION_SOFT: {
        uint8_t v = ((t % 4200UL) < 240UL) ? 210 : 26;
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, v, (uint8_t)(v * 2 / 3), 0));
        break;
    }
    case PAUSE_OPERATOR_WAIT: {
        fillSec(1, secV(1, 30, 18, 0)); fillSec(3, secV(3, 30, 18, 0));
        bool on = (t % 1200UL) < 650UL;
        fillSec(2, on ? secV(2, 210, 125, 0) : secV(2, 55, 30, 0));
        break;
    }
    case PAUSE_FROZEN_LAYER: {
        fillSectionMeter(2, rtPrinter.progress, RgbColor(130, 220, 255));
        for (uint8_t sec = 1; sec <= 3; sec++) {
            uint8_t v = (uint8_t)(45 + sin8fast((uint8_t)(t / 24 + sec * 32)) / 3);
            fillSec(sec, secV(sec, 0, (uint8_t)(v / 2), v));
        }
        setInsideAmbientRgb(RgbColor(0, 160, 255), 65, 45);
        break;
    }
    case PAUSE_FILAMENT_HOLD: {
        RgbColor fil = rtPrinter.filamentColor;
        fillSectionMeter(2, rtPrinter.progress, fil);
        uint16_t head = (uint16_t)((t / 120UL) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 6; tr++) {
            uint16_t pos = (head + OUTER_COUNT - tr) % OUTER_COUNT;
            uint16_t idx = OUTER_START + pos;
            uint8_t v = (uint8_t)(170 - tr * 22);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)fil.R * v / 255), (uint8_t)((uint16_t)fil.G * v / 255), (uint8_t)((uint16_t)fil.B * v / 255)));
        }
        break;
    }
    case PAUSE_DO_NOT_TOUCH: {
        uint8_t beat = ((t % 1600UL) < 120UL || ((t % 1600UL) > 240UL && (t % 1600UL) < 360UL)) ? 220 : 24;
        fillSec(1, secV(1, beat, 65, 0));
        fillSec(3, secV(3, beat, 65, 0));
        fillSectionMeter(2, rtPrinter.progress, RgbColor(255, 120, 0));
        setInsideAmbientRgb(RgbColor(255, 95, 0), (uint8_t)(42 + beat / 5), 12);
        break;
    }
    case PAUSE_HEAT_HOLD_SPLIT: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 30.0f, 260.0f, 0);
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 25.0f, 65.0f, 0);
        fillSectionMeter(1, chamberPct, tempColorFromPct(chamberPct), true);
        fillSectionMeter(2, rtPrinter.progress, RgbColor(255, 145, 0));
        fillSectionMeter(3, toolPct, tempColorFromPct(toolPct), false);
        setInsideAmbientRgb(tempColorFromPct((uint8_t)((toolPct + chamberPct) / 2)), 72, 26);
        break;
    }
    case PAUSE_CALM_DOWN: {
        uint8_t exhale = (uint8_t)((t % 5000UL) * 255UL / 5000UL);
        uint8_t level = (uint8_t)(150U - (uint16_t)exhale * 105U / 255U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d = (i > OUTER_COUNT / 2U) ? i - OUTER_COUNT / 2U : OUTER_COUNT / 2U - i;
            uint8_t v = (uint8_t)(level > d * 3U ? level - d * 3U : 12U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 3U / 4U), 0));
        }
        break;
    }
    case PAUSE_STILL_WATER: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t ripple = sin8fast((uint8_t)(t / 70U + i * 8U));
            uint8_t v = (uint8_t)(18U + ripple / 5U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), 0, (uint8_t)(v * 3U / 4U), v));
        }
        fillSectionMeter(2, rtPrinter.progress, RgbColor(90, 160, 255));
        break;
    }
    case PAUSE_SOFT_LANTERN: {
        uint8_t flame = (uint8_t)(58U + sin8fast((uint8_t)(t / 36U)) / 4U);
        fillSec(1, secV(1, (uint8_t)(flame / 2U), (uint8_t)(flame / 4U), 0));
        fillSec(2, secV(2, flame, (uint8_t)(flame * 3U / 5U), 8));
        fillSec(3, secV(3, (uint8_t)(flame / 2U), (uint8_t)(flame / 4U), 0));
        break;
    }
    case PAUSE_HOLD_ORB: {
        fillSec(1, secV(1, 18, 12, 0));
        fillSec(2, secV(2, 20, 14, 0));
        fillSec(3, secV(3, 18, 12, 0));
        uint8_t lock = (uint8_t)(70U + sin8fast((uint8_t)(t / 45U)) / 5U);
        uint16_t anchors[] = { sectionVisualLed(1, 0), sectionVisualLed(1, LEFT_COUNT - 1U), sectionVisualLed(2, FRONT_COUNT / 2U), sectionVisualLed(3, 0), sectionVisualLed(3, RIGHT_COUNT - 1U) };
        for (uint8_t a = 0; a < 5; a++) {
            setTargetPixel(anchors[a], secV(outerSectionIdx(anchors[a]), lock, (uint8_t)(lock * 3U / 4U), 0));
        }
        break;
    }
    case PAUSE_SUSPENDED_LAYER: {
        uint16_t frozen = (uint16_t)((uint32_t)rtPrinter.progress * FRONT_COUNT / 100U);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool edge = (i == frozen) || (frozen > 0 && i == frozen - 1U);
            uint8_t v = edge ? 190 : ((i < frozen) ? 55 : 8);
            setTargetPixel(sectionVisualLed(2, i), secV(2, v, (uint8_t)(v * 2U / 3U), 0));
        }
        uint8_t drift = (uint8_t)(t / 220U);
        for (uint16_t i = 0; i < LEFT_COUNT; i++) {
            bool grid = (((i + drift) % 4U) == 0U);
            uint8_t v = grid ? 85 : 14;
            setTargetPixel(sectionVisualLed(1, i), secV(1, v, (uint8_t)(v * 2U / 3U), 0));
            setTargetPixel(sectionVisualLed(3, i), secV(3, v, (uint8_t)(v * 2U / 3U), 0));
        }
        break;
    }
    case PAUSE_GENTLE_REMINDER: {
        fillSec(1, secV(1, 24, 16, 0));
        fillSec(2, secV(2, 18, 12, 0));
        fillSec(3, secV(3, 24, 16, 0));
        uint8_t tick = (uint8_t)((t / 1000UL) % 10U);
        for (uint8_t i = 0; i <= tick && i < 10U; i++) {
            uint16_t p = sectionVisualLed(2, (uint16_t)i * (FRONT_COUNT - 1U) / 9U);
            setTargetPixel(p, secV(2, 155, 100, 0));
        }
        break;
    }
    case PAUSE_BREATH_GATE: {
        uint8_t valve = (uint8_t)((t / 90UL) % FRONT_COUNT);
        fillSec(1, secV(1, 34, 20, 0));
        fillSec(3, secV(3, 34, 20, 0));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t d = (valve > i) ? valve - i : i - valve;
            uint8_t v = (d < 3U) ? (uint8_t)(185U - d * 45U) : ((i % 4U == 0U) ? 52U : 10U);
            setTargetPixel(sectionVisualLed(2, i), secV(2, v, (uint8_t)(v * 2U / 3U), 0));
        }
        break;
    }
    case PAUSE_WAITING_ROOM: {
        fillSec(1, secV(1, 42, 28, 0));
        fillSec(2, secV(2, 20, 32, 36));
        fillSec(3, secV(3, 42, 28, 0));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint8_t spark = extraSpark(i, t / 3U, 73);
            if (spark > 242U) setTargetPixel(OUTER_START + i, secV(outerSectionIdx(OUTER_START + i), spark, (uint8_t)(spark * 4U / 5U), (uint8_t)(spark / 3U)));
        }
        break;
    }
    case PAUSE_TOOL_PARK: {
        fillSectionMeter(2, rtPrinter.progress, RgbColor(255, 130, 25));
        uint8_t blink = ((t % 1800UL) < 140UL) ? 210 : 36;
        fillSec(1, secV(1, blink, (uint8_t)(blink / 2U), 0));
        fillSec(3, secV(3, 32, 20, 0));
        setTargetPixel(RIGHT_START, secV(3, 255, 185, 0));
        setTargetPixel(RIGHT_END, secV(3, 255, 185, 0));
        break;
    }
    case PAUSE_RESUME_RAMP: {
        uint8_t ramp = (uint8_t)((t / 55UL) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool armed = i <= ramp;
            uint8_t v = armed ? 120 : 12;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(v * 4U / 5U), v, 0));
        }
        fillSectionMeter(2, rtPrinter.progress, RgbColor(0, 220, 120));
        break;
    }
    }
    if (!ledAllowsInsideRgbNow()) renderInsideNormalToTarget();
}

// -- ERROR animations ---------------------------------------------------------

static void renderErrorTarget() {
    clearTarget();
    renderInsideNormalToTarget();
    uint32_t t = millis();
    switch (rtLed.errorAnim) {
    case ERROR_BLINK: {
        bool ph = (t/300)%2;
        if (ph) {
            fillSec(1, secV(1,0,0,200)); fillSec(3, secV(3,0,0,200));
        } else {
            fillSec(2, secV(2,200,0,0));
        }
        break;
    }
    case ERROR_SOS: {
        // ...---... pattern: short=150ms on/150ms off, long=450ms on/150ms off, pause=800ms
        // BUG-3 fix: pStart=0 caused immediate skip of all slots on first call.
        // Initialize to current t so first slot starts fresh.
        static const uint16_t pat[] = {150,150,150,150,150,150, 450,150,450,150,450,150, 150,150,150,150,150,150, 800,0};
        static uint8_t pIdx = 0;
        static uint32_t pStart = 0;
        static bool pInitialized = false;
        if (!pInitialized) { pStart = t; pInitialized = true; }
        if (t - pStart >= pat[pIdx]) { pStart = t; pIdx = (pIdx + 1) % 20; if(pIdx >= 19) pIdx = 0; }
        bool on = (pIdx%2==0) && (pIdx<18);
        uint8_t v = on ? 220 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,0,0));
        break;
    }
    case ERROR_FIRE_ALARM: {
        bool lr = (t/80)%2 == 0 && (t%250 < 80);
        if (lr) { fillSec(1, secV(1,220,0,0)); fillSec(3, secV(3,220,0,0)); }
        uint16_t ph = (t/3)%1024;
        uint8_t bv = (ph<512)?ph/4:(1023-ph)/4;
        fillSec(2, secV(2, min((uint32_t)255, (uint32_t)(100u+bv)), 40, 0));
        break;
    }
    case ERROR_CRITICAL: {
        bool on = (t%250 < 225);
        uint8_t v = on ? 230 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,0,0));
        break;
    }
    case ERROR_POLICE: {
        bool lB = (t%200 < 100);
        fillSec(1, lB ? secV(1,0,0,220) : secV(1,5,5,5));
        fillSec(3, lB ? secV(3,5,5,5) : secV(3,220,0,0));
        fillSec(2, secV(2,10,5,5));
        break;
    }
    case ERROR_BREATHE_RED: {
        uint16_t ph = (t/5)%1024;
        uint8_t bv = (ph<512)?ph/4:(1023-ph)/4;
        uint8_t v = (uint8_t)(40+bv/2);
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,0,0));
        break;
    }
    case ERROR_HEARTBEAT: {
        uint32_t cycle = t%1580;
        bool on = cycle<80 || (cycle>=160&&cycle<240);
        uint8_t v = on ? 230 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,0,0));
        break;
    }
    case ERROR_STROBE: {
        bool on = (t/50)%2==0;
        uint8_t v = on ? 240 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,0,0));
        break;
    }
    case ERROR_WAVE_RED: {
        for (uint16_t i=0;i<OUTER_COUNT;i++) {
            uint16_t idx=OUTER_START+i;
            uint8_t ph=(uint8_t)(t/20 + i*12);
            uint8_t v=(uint8_t)(60+sin8fast(ph)/2);
            setTargetPixel(idx, secV(outerSectionIdx(idx),v,0,0));
        }
        break;
    }
    case ERROR_XENON: {
        bool on = (t%1000 < 25);
        uint8_t v = on ? 255 : 0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec, secV(sec,v,v,v));
        break;
    }
    case ERROR_SIREN: {
        uint32_t pos = (t/15)%OUTER_COUNT;
        EXT_RAM_BSS_ATTR static uint8_t stail[OUTER_COUNT]={};
        for(uint16_t i=0;i<OUTER_COUNT;i++) stail[i]=(uint8_t)((uint16_t)stail[i]*180/255);
        stail[pos]=255;
        for(uint16_t i=0;i<OUTER_COUNT;i++) {
            uint16_t idx=OUTER_START+i;
            setTargetPixel(idx, secV(outerSectionIdx(idx),stail[i],0,0));
        }
        break;
    }
    case ERROR_THUNDER: {
        // BUG-F fix: brightness randomised once at flash start, not per frame
        static uint32_t nextFlash = 0;
        static uint32_t flashEnd  = 0;
        static uint8_t  flashSec  = 1;
        static uint8_t  flashV    = 200;
        if (t > nextFlash && t > flashEnd) {
            flashEnd  = t + (uint32_t)random(30, 120);
            nextFlash = flashEnd + (uint32_t)random(200, 1500);
            flashSec  = (uint8_t)(1 + random(3));
            flashV    = (uint8_t)random(150, 255);  // fixed for duration of flash
        }
        if (t < flashEnd) {
            bool isBlue = (flashSec == 2);
            fillSec(flashSec, isBlue ? secV(flashSec, flashV/3, flashV/3, flashV)
                                     : secV(flashSec, flashV, flashV, flashV));
        }
        break;
    }
    case ERROR_COUNTDOWN: default: {
        uint32_t elapsed=t%8000;
        uint32_t period=(uint32_t)max((uint32_t)50, (uint32_t)(800u-(elapsed/10)));
        bool on2=(t%period)<(period/2);
        uint8_t v=on2?220:0;
        for (uint8_t sec=1;sec<=3;sec++) fillSec(sec,secV(sec,v,0,0));
        break;
    }
    case ERROR_GLITCH: {
        // Random pixel corruption - chaotic static
        static const uint32_t GL_STEP=30;
        static uint32_t lastGl=0;
        EXT_RAM_BSS_ATTR static uint8_t glV[OUTER_COUNT]={};
        if((uint32_t)(t-lastGl)>=GL_STEP){lastGl=t-((t-lastGl)%GL_STEP);
            for(uint16_t i=0;i<OUTER_COUNT;i++)glV[i]=(random(0,4)<1)?(uint8_t)random(0,255):0;}
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint16_t idx=OUTER_START+i;uint8_t v=glV[i];setTargetPixel(idx,secV(outerSectionIdx(idx),v,v/4,v/8));}
        break;
    }
    case ERROR_ALARM_CHASE: {
        // Red/white high-speed chase
        uint16_t pos=(uint32_t)(t/20)%OUTER_COUNT;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            bool isHead=(i==pos)||((i+1)%OUTER_COUNT==pos);
            setTargetPixel(idx,isHead?secV(outerSectionIdx(idx),255,255,255):secV(outerSectionIdx(idx),200,0,0));
        }
        break;
    }
    case ERROR_DANGER_STRIPE: {
        // Hazard tape: marching red/black stripes
        uint8_t offset=(uint8_t)(t/60);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            bool red=((i+offset)/4)%2==0;
            setTargetPixel(idx,red?secV(outerSectionIdx(idx),220,120,0):RgbwColor(0,0,0,0));
        }
        break;
    }
    case ERROR_PULSE_ALERT: {
        // Aggressive red: fast rise, slow decay
        uint32_t ph=t%800;
        uint8_t v=(ph<100)?(uint8_t)(ph*255/100):(uint8_t)(255-((ph-100)*255/700));
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v,0,0));
        break;
    }
    case ERROR_REDOUT: {
        // Deep red fill that pulses like a dying heartbeat
        uint32_t ph=t%2000;
        uint8_t v=(ph<300)?(uint8_t)(ph*200/300):(uint8_t)(200-(ph-300)*200/1700);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v,0,0));
        break;
    }
    case ERROR_EMERGENCY: {
        // Rapid full-white / full-red alternating
        bool alt=(t/100)%2==0;
        uint8_t r=alt?255:200, g=alt?255:0, b=alt?255:0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,r,g,b));
        break;
    }
    case ERROR_MELTDOWN: {
        // Red fire on all sections simultaneously
        static const uint32_t MELT_STEP=50;
        static uint32_t lastMelt=0;
        EXT_RAM_BSS_ATTR static uint8_t meltH[OUTER_COUNT]={};
        if((uint32_t)(t-lastMelt)>=MELT_STEP){
            lastMelt=t-((t-lastMelt)%MELT_STEP);
            for(uint16_t i=OUTER_COUNT-1;i>0;i--)meltH[i]=(meltH[i-1]+meltH[i]+meltH[i])/3;
            meltH[0]=(uint8_t)max(0,(int)meltH[0]-(int)random(30,70));
            meltH[0]=(uint8_t)min(255,(int)meltH[0]+(int)random(60,160));
        }
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i; uint8_t v=meltH[OUTER_COUNT-1-i];
            uint8_t r=(v<85)?v*3:255, g=(v<85)?0:(v<170)?(v-85)*3:255;
            setTargetPixel(idx,rgbToRgbwLimited(RgbColor(r,g,0),255,W_MAX_OUT));
        }
        break;
    }
    case ERROR_CRASH: {
        // Sparks from center outward, then dark, repeating
        uint32_t ph=t%1200;
        uint16_t center=OUTER_COUNT/2;
        uint16_t spread=(ph<600)?(uint16_t)(ph*center/600):0;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<center)?(center-i):(i-center);
            bool on=(spread>0&&dist<=spread&&dist>=spread-2);
            setTargetPixel(idx,on?secV(outerSectionIdx(idx),255,200,0):RgbwColor(0,0,0,0));
        }
        break;
    }
    case ERROR_RED_THEATER:   { renderOuterExtraPattern(0,  t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_FAULT_RIPPLE:  { renderOuterExtraPattern(4,  t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_HOT_ZONE:      { renderOuterExtraPattern(3,  t, 8, 255, 0, 0, 0, false); break; }
    case ERROR_PANIC_COMETS:  { renderOuterExtraPattern(6,  t / 2, 0, 255, 0, 0, 0, false); break; }
    case ERROR_LOCKDOWN:      { renderOuterExtraPattern(14, t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_WARNING_TICKS: { renderOuterExtraPattern(11, t, 18, 255, 0, 0, 0, false); break; }
    case ERROR_BREACH_SCAN:   { renderOuterExtraPattern(17, t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_FAULT_SPARKS:  { renderOuterExtraPattern(15, t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_RED_JUGGLE:    { renderOuterExtraPattern(2,  t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_EVACUATE:      { renderOuterExtraPattern(18, t, 0, 255, 0, 0, 0, false); break; }
    case ERROR_ROOT_CAUSE_HINT: {
        bool netBad = !rtPrinter.moonrakerOnline;
        bool thermalBad = rtVent.failsafeActive || rtVent.currentTempC > (rtVent.targetTempC + 8.0f);
        fillSec(1, netBad ? secV(1, 0, 70, 255) : secV(1, 0, 120, 70));
        fillSec(3, thermalBad ? secV(3, 255, 70, 0) : secV(3, 0, 120, 70));
        uint8_t beat = ((t % 900UL) < 110UL || ((t % 900UL) > 210UL && (t % 900UL) < 310UL)) ? 230 : 32;
        fillSec(2, secV(2, beat, thermalBad ? 35 : 0, netBad ? 80 : 0));
        setInsideAmbientRgb(thermalBad ? RgbColor(255, 70, 0) : (netBad ? RgbColor(0, 80, 255) : RgbColor(255, 0, 0)), (uint8_t)(55 + beat / 3), 0);
        break;
    }
    case ERROR_STACK_LIGHT: {
        uint8_t pulse = ((t / 180UL) % 2) ? 210 : 55;
        fillSec(1, secV(1, 255, 0, 0));
        fillSec(2, secV(2, 255, pulse / 2, 0));
        fillSec(3, secV(3, 255, 0, 0));
        setInsideAmbientRgb(RgbColor(255, 45, 0), (uint8_t)(55 + pulse / 4), 0);
        break;
    }
    case ERROR_HEARTBEAT_SMART: {
        uint32_t cycle = t % 1400UL;
        uint8_t beat = (cycle < 80UL || (cycle > 160UL && cycle < 240UL)) ? 245 : 18;
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, beat, 0, 0));
        setInsideAmbientRgb(RgbColor(255, 0, 0), (uint8_t)(35 + beat / 3), 0);
        break;
    }
    case ERROR_LOCATION_SPLIT: {
        uint8_t scan = (uint8_t)(t / 35);
        fillSec(1, rtPrinter.moonrakerOnline ? secV(1, 0, 120, 80) : secV(1, 0, 80, 255));
        fillSec(3, rtVent.failsafeActive ? secV(3, 255, 70, 0) : secV(3, 80, 0, 0));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool on = (((i + scan) % 6) < 2);
            setTargetPixel(sectionVisualLed(2, i), on ? secV(2, 255, 0, 0) : secV(2, 30, 0, 0));
        }
        setInsideAmbientRgb(RgbColor(255, 20, 0), 70, 0);
        break;
    }
    case ERROR_BLACKOUT_FLASH: {
        bool flash = (t % 1800UL) < 65UL || ((t + 120UL) % 1800UL) < 45UL;
        uint8_t v = flash ? 255 : 0;
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secV(sec, v, flash ? v : 0, flash ? v : 0));
        setInsideAmbientRgb(flash ? RgbColor(255, 255, 255) : RgbColor(35, 0, 0), flash ? 180 : 18, 0);
        break;
    }
    case ERROR_RECOVERY_WAIT: {
        bool online = rtPrinter.moonrakerOnline && !rtVent.failsafeActive;
        uint8_t wave = sin8fast((uint8_t)(t / 18));
        RgbColor c = online ? RgbColor(0, 190, 120) : RgbColor(255, 95, 0);
        fillSec(1, secV(1, c.R / 2, c.G / 2, c.B / 2));
        fillSectionMeter(2, online ? 70 : 25, c);
        fillSec(3, secV(3, c.R / 2, c.G / 2, c.B / 2));
        setInsideAmbientRgb(c, (uint8_t)(35 + wave / 4), online ? 10 : 0);
        break;
    }
    case ERROR_SIREN_SCAN_SMART: {
        renderOuterExtraPattern(17, t / 2, rtVent.failsafeActive ? 12 : 0, 255, 0, 0, 0, false);
        setInsideAmbientRgb(RgbColor(255, 0, 0), (uint8_t)(35 + sin8fast((uint8_t)(t / 6)) / 2), 0);
        break;
    }
    case ERROR_DIAGNOSTIC_BITS: {
        uint8_t bits = 0;
        if (!rtPrinter.moonrakerOnline) bits |= 1;
        if (rtVent.failsafeActive) bits |= 2;
        if (rtVent.currentTempC > rtVent.targetTempC + 8.0f) bits |= 4;
        fillSec(1, (bits & 1) ? secV(1, 0, 70, 255) : secV(1, 0, 80, 50));
        fillSec(3, (bits & 2) ? secV(3, 255, 70, 0) : secV(3, 0, 80, 50));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool on = (bits == 0) ? (((i + t / 160UL) % 8) == 0) : (bits & (1 << (i % 3)));
            setTargetPixel(sectionVisualLed(2, i), on ? secV(2, 255, 0, 0) : secV(2, 15, 0, 0));
        }
        setInsideAmbientRgb((bits & 2) ? RgbColor(255, 70, 0) : RgbColor(255, 0, 0), 65, 0);
        break;
    }
    case ERROR_SERVICE_BEACON: {
        uint8_t pulse = ((t % 1000UL) < 140UL) ? 230 : 35;
        fillSec(1, secV(1, pulse, 95, 0));
        fillSec(2, secV(2, pulse, 0, 0));
        fillSec(3, secV(3, pulse, 95, 0));
        setInsideAmbientRgb(RgbColor(255, 90, 0), (uint8_t)(45 + pulse / 4), 0);
        break;
    }
    case ERROR_SAFE_SHUTDOWN: {
        uint8_t fade = (uint8_t)(255 - ((t / 18UL) & 0xFF));
        fillSec(1, secV(1, fade, 0, 0));
        fillSec(2, secV(2, (uint8_t)(fade / 2), 0, 0));
        fillSec(3, secV(3, fade, 0, 0));
        setInsideAmbientRgb(RgbColor(120, 0, 0), (uint8_t)(20 + fade / 5), 0);
        break;
    }
    case ERROR_CALM_ALERT: {
        uint8_t v = (uint8_t)(52U + sin8fast((uint8_t)(t / 26U)) / 2U);
        fillSec(1, secV(1, v, 0, 0));
        fillSec(2, secV(2, v, (uint8_t)(v / 6U), 0));
        fillSec(3, secV(3, v, 0, 0));
        break;
    }
    case ERROR_FAULT_LOCATOR: {
        fillSec(1, secV(1, 34, 0, 0));
        fillSec(2, secV(2, 24, 0, 0));
        fillSec(3, secV(3, 34, 0, 0));
        uint16_t scan = (uint16_t)((t / 58UL) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 5; tr++) {
            uint16_t p = (scan + OUTER_COUNT - tr) % OUTER_COUNT;
            uint8_t v = (uint8_t)(235U - tr * 35U);
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), v, (uint8_t)(v / 10U), 0));
        }
        break;
    }
    case ERROR_THERMAL_CUT: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 30.0f, 260.0f, 75);
        RgbColor heat = tempColorFromPct(toolPct);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t wave = sin8fast((uint8_t)(t / 12U + i * 16U));
            uint8_t v = (uint8_t)(45U + (uint16_t)wave * toolPct / 180U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)heat.R * v / 255U), (uint8_t)((uint16_t)heat.G * v / 255U), (uint8_t)((uint16_t)heat.B * v / 255U)));
        }
        break;
    }
    case ERROR_NETWORK_LOST: {
        uint8_t ph = (uint8_t)(t / 140U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool packet = (((i + ph) % 9U) < 3U);
            bool gap = (((i + ph) % 17U) == 0U);
            setTargetPixel(idx, gap ? secV(outerSectionIdx(idx), 0, 0, 0) : (packet ? secV(outerSectionIdx(idx), 0, 70, 210) : secV(outerSectionIdx(idx), 115, 0, 0)));
        }
        break;
    }
    case ERROR_SERVICE_CODE: {
        uint8_t page = (uint8_t)((t / 900UL) % 4U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool frame = (i < 3U) || (i >= OUTER_COUNT - 3U) || ((i + page) % 14U == 0U);
            uint8_t v = frame ? 185 : 14;
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v / 6U), 0));
        }
        fillSec(2, secV(2, (uint8_t)(70U + page * 35U), 0, 0));
        break;
    }
    case ERROR_CONTAINMENT: {
        fillSec(1, secV(1, 120, 0, 0));
        fillSec(3, secV(3, 120, 0, 0));
        uint16_t gate = (uint16_t)((t / 75UL) % (FRONT_COUNT / 2U + 1U));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint16_t d = (i < FRONT_COUNT / 2U) ? i : (FRONT_COUNT - 1U - i);
            uint8_t v = (d <= gate) ? 210 : 18;
            setTargetPixel(sectionVisualLed(2, i), secV(2, v, 0, 0));
        }
        break;
    }
    case ERROR_SAFE_BREATH: {
        uint8_t fade = (uint8_t)(160U - ((t % 4200UL) * 120UL / 4200UL));
        fillSec(1, secV(1, fade, 0, 0));
        fillSec(3, secV(3, fade, 0, 0));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t v = (uint8_t)(fade > i * 5U ? fade - i * 5U : 10U);
            setTargetPixel(sectionVisualLed(2, i), secV(2, v, 0, 0));
        }
        break;
    }
    case ERROR_ESCALATION: {
        uint8_t ramp = (uint8_t)((t % 2200UL) * 255UL / 2200UL);
        uint8_t wave = sin8fast((uint8_t)(t / 9U));
        uint8_t v = (uint8_t)(35U + (uint16_t)ramp * wave / 255U);
        fillSec(1, secV(1, v, 0, 0));
        fillSec(2, secV(2, v, (uint8_t)(v / 4U), 0));
        fillSec(3, secV(3, v, 0, 0));
        if (ramp > 220U) {
            uint16_t flash = (uint16_t)((t / 35UL) % OUTER_COUNT);
            setTargetPixel(OUTER_START + flash, secV(outerSectionIdx(OUTER_START + flash), 255, 255, 255));
        }
        break;
    }
    case ERROR_REPAIR_BEACON: {
        fillSec(1, secV(1, 48, 0, 0));
        fillSec(2, secV(2, 36, 0, 0));
        fillSec(3, secV(3, 48, 0, 0));
        uint8_t flash = ((t % 1400UL) < 180UL) ? 230 : 80;
        uint16_t mid = FRONT_COUNT / 2U;
        for (int8_t d = -2; d <= 2; d++) {
            int16_t p = (int16_t)mid + d;
            if (p >= 0 && p < (int16_t)FRONT_COUNT) setTargetPixel(sectionVisualLed(2, (uint16_t)p), secV(2, flash, flash, flash));
        }
        setTargetPixel(sectionVisualLed(1, LEFT_COUNT / 2U), secV(1, flash, flash, flash));
        setTargetPixel(sectionVisualLed(3, RIGHT_COUNT / 2U), secV(3, flash, flash, flash));
        break;
    }
    case ERROR_COOLING_ALARM: {
        uint8_t ph = (uint8_t)(t / 110U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool cool = (((i + ph) % 8U) < 4U);
            setTargetPixel(idx, cool ? secV(outerSectionIdx(idx), 0, 80, 220) : secV(outerSectionIdx(idx), 210, 0, 0));
        }
        break;
    }
    }
}

// -- FINISH animations ---------------------------------------------------------

static void renderFinishTarget() {
    clearTarget();
    // P1: In preview, use independent startMs so animation always starts from t=0
    uint32_t t = ledAnimPreviewActive()
               ? (millis() - gLedAnimPreview.startMs)
               : (millis() - rtLed.finishAnimStart);
    switch (rtLed.finishAnim) {
    case FINISH_SWEEP: {
        uint16_t pos=(t/30)%OUTER_COUNT;
        uint16_t idx=OUTER_START+pos;
        uint8_t v=sectionBrightToV(rtLed.sectionBright[outerSectionIdx(idx)]);
        setTargetPixel(idx, rgbToRgbwLimited(RgbColor(0,v,0),255,W_MAX_OUT));
        break;
    }
    case FINISH_RAINBOW: {
        uint8_t sh=(uint8_t)(t/6);
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint16_t idx=OUTER_START+i;setTargetPixel(idx,secHSV(outerSectionIdx(idx),sh+(uint8_t)(i*255/OUTER_COUNT),255,OUT_MAX));}
        break;
    }
    case FINISH_PULSE: {
        uint16_t ph=(t/4)%1024;uint8_t bv=(ph<512)?ph/4:(1023-ph)/4;uint8_t v=(uint8_t)(60+bv/3);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v,v,v));
        break;
    }
    case FINISH_FILAMENT: {
        // Use demo filament colour in preview for consistent look
        const bool finPreview = ledAnimPreviewActive() && (gLedAnimPreview.cat == 4);
        uint32_t fR = finPreview ? gLedAnimPreview.demoFilR : (uint32_t)rtPrinter.filamentColor.R;
        uint32_t fG = finPreview ? gLedAnimPreview.demoFilG : (uint32_t)rtPrinter.filamentColor.G;
        uint32_t fB = finPreview ? gLedAnimPreview.demoFilB : (uint32_t)rtPrinter.filamentColor.B;
        // BLACK_FILAMENT: same rainbow substitution as renderPrintTarget
        if (!finPreview && (fR + fG + fB) < 30) {
            uint8_t rainbowHue = (uint8_t)(millis() / 20);
            RgbColor rc = hsvToRgb(rainbowHue, 255, 220);
            fR = rc.R; fG = rc.G; fB = rc.B;
        }
        for(uint8_t sec=1;sec<=3;sec++){
            uint8_t sv=sectionBrightToV(rtLed.sectionBright[sec]);
            fillSec(sec,rgbToRgbwLimited(RgbColor((uint16_t)fR*sv/255,(uint16_t)fG*sv/255,(uint16_t)fB*sv/255),OUT_MAX,W_MAX_OUT));
        }
        break;
    }
    case FINISH_FIREWORKS: {
        EXT_RAM_BSS_ATTR static uint8_t fw[OUTER_COUNT]={};
        EXT_RAM_BSS_ATTR static uint8_t fwH[OUTER_COUNT]={};
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            if(fw[i]>0) fw[i]=(uint8_t)((uint16_t)fw[i]*200/255);
            uint16_t idx=OUTER_START+i;
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),fwH[i],255,(uint8_t)((uint16_t)fw[i]*OUT_MAX/255)));
        }
        if(random(8)==0){uint16_t p=(uint16_t)random(OUTER_COUNT);fw[p]=255;fwH[p]=(uint8_t)random(256);}
        break;
    }
    case FINISH_CURTAIN: {
        // Fill from center outward on Center, top on L/R
        uint16_t prog=min((uint32_t)FRONT_COUNT,(uint32_t)t/50);
        uint8_t sv2=sectionBrightToV(rtLed.sectionBright[2]);
        uint8_t goldR=(uint8_t)((uint16_t)255*sv2/255),goldG=(uint8_t)((uint16_t)180*sv2/255),goldB=(uint8_t)((uint16_t)30*sv2/255);
        RgbwColor gold=rgbToRgbwLimited(RgbColor(goldR,goldG,goldB),255,W_MAX_OUT);
        for(uint16_t i=0;i<FRONT_COUNT;i++){int16_t dist=(int16_t)i-(int16_t)(FRONT_COUNT/2);if(dist<0)dist=-dist;if((uint16_t)dist<=prog/2)setTargetPixel(sectionVisualLed(2, i),gold);}
        uint16_t lrprog=min((uint32_t)LEFT_COUNT,(uint32_t)t/80);
        for(uint16_t i=0;i<lrprog;i++){
            setTargetPixel(sectionVisualLed(1, i),       secV(1,(uint8_t)goldR,(uint8_t)goldG,(uint8_t)goldB));
            setTargetPixel(sectionVisualLed(3, i), secV(3,(uint8_t)goldR,(uint8_t)goldG,(uint8_t)goldB));
        }
        break;
    }
    case FINISH_CONFETTI: {
        EXT_RAM_BSS_ATTR static uint8_t cf[OUTER_COUNT]={};EXT_RAM_BSS_ATTR static uint8_t cfH[OUTER_COUNT]={};
        for(uint16_t i=0;i<OUTER_COUNT;i++){if(cf[i]>0)cf[i]=(uint8_t)((uint16_t)cf[i]*210/255);}
        uint8_t spawn=(uint8_t)min((uint32_t)5,(uint32_t)(t/2000+1));
        for(uint8_t s=0;s<spawn;s++){if(random(10)==0){uint16_t p=(uint16_t)random(OUTER_COUNT);cf[p]=255;cfH[p]=(uint8_t)random(256);}}
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint16_t idx=OUTER_START+i;setTargetPixel(idx,secHSV(outerSectionIdx(idx),cfH[i],255,(uint8_t)((uint16_t)cf[i]*OUT_MAX/255)));}
        break;
    }
    case FINISH_GOLD_RAIN: {
        fillSec(1,secV(1,220,160,20));fillSec(3,secV(3,220,160,20));
        uint32_t sp=(t/40)%5;
        for(uint16_t i=0;i<FRONT_COUNT;i++){bool on2=(((uint32_t)i+sp)%5==0);setTargetPixel(sectionVisualLed(2, i),on2?secV(2,220,160,20):secV(2,60,40,5));}
        break;
    }
    case FINISH_STROBE_PARTY: {
        // BUG-2 fix: t%80==0 almost never hits at variable FPS - use t/80 instead
        uint8_t spH = (uint8_t)((t / 80) * 30);
        bool spOn = (t % 80 < 60);
        uint8_t v = spOn ? OUT_MAX : 0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,spH+(sec*20),255,v));
        break;
    }
    case FINISH_BOUNCING_BALLS: {
        // BUG-5 fix: physics now dt-based (per 20ms tick) - FPS-independent
        static const uint32_t BB_STEP_MS = 20;
        static uint32_t lastBbStep = 0;
        EXT_RAM_BSS_ATTR static float bpos[4] = {0, 5, 10, 15};
        EXT_RAM_BSS_ATTR static float bvel[4] = {0.8f, 0.6f, 1.0f, 0.7f};
        EXT_RAM_BSS_ATTR static uint8_t bhue[4] = {0, 60, 120, 200};
        if (t - lastBbStep >= BB_STEP_MS) {
            lastBbStep = t - ((t - lastBbStep) % BB_STEP_MS);
            for(int b = 0; b < 4; b++) {
                bpos[b] += bvel[b]; bvel[b] -= 0.04f;
                if(bpos[b] <= 0) { bpos[b] = 0; bvel[b] = 0.7f + b * 0.08f; }
                if(bpos[b] > 19) { bpos[b] = 19; bvel[b] = -bvel[b]; }
            }
        }
        fillSec(2, secV(2, 10, 10, 10));
        for(int b = 0; b < 4; b++) {
            uint8_t p = (uint8_t)bpos[b];
            setTargetPixel(sectionVisualLed(2, p), secHSV(2, bhue[b], 255, OUT_MAX));
        }
        fillSec(1, secHSV(1, bhue[0], 255, sectionBrightToV(rtLed.sectionBright[1])));
        fillSec(3, secHSV(3, bhue[2], 255, sectionBrightToV(rtLed.sectionBright[3])));
        break;
    }
    case FINISH_RAINBOW_EXPLODE: {
        uint32_t cycle=(t%2000);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            // Distance from centre of outer strip - scales with OUTER_COUNT
            uint16_t half=OUTER_COUNT/2;
            uint16_t dist=(uint16_t)(i<half ? half-i : i-half);
            uint8_t h=(uint8_t)(cycle/8+(uint8_t)(dist*12));
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),h,255,OUT_MAX));
        }
        break;
    }
    case FINISH_DISCO: {
        EXT_RAM_BSS_ATTR static uint8_t dh[4]={};
        if(t%150<20){
            for(uint8_t sec=1;sec<=3;sec++){dh[sec]=(uint8_t)random(256);fillSec(sec,secHSV(sec,dh[sec],255,OUT_MAX));}
        } else {
            for(uint8_t sec=1;sec<=3;sec++) fillSec(sec,secHSV(sec,dh[sec],255,OUT_MAX));
        }
        break;
    }
    case FINISH_HEART: {
        uint32_t cycle2=t%780;
        bool on2=(cycle2<80)||(cycle2>=160&&cycle2<240);
        uint8_t v2=on2?230:20;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v2,(uint8_t)(v2/5),(uint8_t)(v2/8)));
        break;
    }
    case FINISH_SPARKLE: {
        // Random white sparkles fading on all LEDs
        static const uint32_t SP_STEP=40;
        static uint32_t lastSp=0;
        EXT_RAM_BSS_ATTR static uint8_t spV[OUTER_COUNT]={};
        if((uint32_t)(t-lastSp)>=SP_STEP){lastSp=t-((t-lastSp)%SP_STEP);
            for(int i=0;i<(int)OUTER_COUNT;i++){if(spV[i]>20)spV[i]-=20;else spV[i]=0;if(random(0,8)==0)spV[i]=255;}
        }
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint8_t v=spV[OUTER_START+i];setTargetPixel(OUTER_START+i,rgbToRgbwLimited(RgbColor(v,v,v),255,W_MAX_OUT));} // INSIDE-FIX: only outer
        break;
    }
    case FINISH_CHAMPAGNE: {
        // Gold bubbles rise left/right, burst at top
        static const uint32_t CHAMP_STEP=60;
        static uint32_t lastChamp=0;
        EXT_RAM_BSS_ATTR static float cL[4]={0,3,6,9},cR[4]={1,4,7,10};
        if((uint32_t)(t-lastChamp)>=CHAMP_STEP){lastChamp=t-((t-lastChamp)%CHAMP_STEP);
            for(int b=0;b<4;b++){cL[b]-=0.5f;if(cL[b]<0)cL[b]=10.0f;cR[b]-=0.5f;if(cR[b]<0)cR[b]=10.0f;}
        }
        for(uint16_t i=0;i<LEFT_COUNT;i++){setTargetPixel(sectionVisualLed(1, i),RgbwColor(0,0,0,0));setTargetPixel(sectionVisualLed(3, i),RgbwColor(0,0,0,0));}
        for(int b=0;b<4;b++){
            uint8_t pL=(uint8_t)((LEFT_COUNT - 1)-(uint8_t)cL[b]),pR=(uint8_t)((RIGHT_COUNT - 1)-(uint8_t)cR[b]);
            setTargetPixel(sectionVisualLed(1, pL),  rgbToRgbwLimited(RgbColor(255,200,0),255,W_MAX_OUT));
            setTargetPixel(sectionVisualLed(3, pR), rgbToRgbwLimited(RgbColor(255,200,0),255,W_MAX_OUT));
        }
        fillSec(2,secV(2,10,8,0));
        break;
    }
    case FINISH_WIPE_OUT: {
        // Color wipe from both ends meeting in center
        uint32_t ph2=t%3000;
        uint16_t pos=(ph2<1500)?(uint16_t)(ph2*OUTER_COUNT/2/1500):(uint16_t)(OUTER_COUNT/2-(ph2-1500)*OUTER_COUNT/2/1500);
        uint8_t hw=(uint8_t)(t/20);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<OUTER_COUNT/2)?i:(OUTER_COUNT-1-i);
            bool lit=(dist<pos);
            setTargetPixel(idx,lit?secHSV(outerSectionIdx(idx),hw,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        break;
    }
    case FINISH_FILL_PROG: {
        // Fills all outer LEDs one by one then blinks gold
        uint32_t ph3=t%3000;
        uint16_t lit=(ph3<2000)?(uint16_t)(ph3*OUTER_COUNT/2000):OUTER_COUNT;
        bool blink=(ph3>=2000&&(ph3/100)%2==0);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            bool on=(i<lit)&&!blink;
            setTargetPixel(idx,on?secHSV(outerSectionIdx(idx),45,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        break;
    }
    case FINISH_WATERFALL: {
        // Color cascades from top of left/right down, then along front
        uint32_t ph4=t%4000;
        uint8_t step=(uint8_t)(ph4/60);
        uint8_t hw2=(uint8_t)(t/15);
        for(uint16_t i=0;i<LEFT_COUNT;i++){
            bool on=(step>i);
            setTargetPixel(sectionVisualLed(1, i),  on?secHSV(1,hw2,255,OUT_MAX):RgbwColor(0,0,0,0));
            setTargetPixel(sectionVisualLed(3, i), on?secHSV(3,hw2+64,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        for(uint16_t i=0;i<FRONT_COUNT;i++){
            bool on=(step>(uint8_t)(LEFT_COUNT+i));
            setTargetPixel(sectionVisualLed(2, i), on?secHSV(2,hw2+128,255,OUT_MAX):RgbwColor(0,0,0,0));
        }
        break;
    }
    case FINISH_STARBURST: {
        // Bright flash expanding outward from center
        uint32_t ph5=t%1500;
        uint16_t center=OUTER_COUNT/2;
        uint16_t spread=(ph5<500)?(uint16_t)(ph5*center/500):(uint16_t)(center-(ph5-500)*center/1000);
        uint8_t hs=(uint8_t)(t/10);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint16_t dist=(i<center)?(center-i):(i-center);
            bool on=(dist<=spread&&spread>0);
            uint8_t v=on?(uint8_t)(OUT_MAX-(dist*OUT_MAX/(center+1))):0;
            setTargetPixel(idx,v?secHSV(outerSectionIdx(idx),hs,255,v):RgbwColor(0,0,0,0));
        }
        break;
    }
    case FINISH_COLOR_SPIRAL: default: {
        uint8_t sh=(uint8_t)(t/6);
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t sec=outerSectionIdx(idx);
            uint8_t sp=(sec==1)?2:(sec==2)?1:3;
            setTargetPixel(idx,secHSV(sec,sh*sp+(uint8_t)(i*8),255,OUT_MAX));
        }
        break;
    }
    case FINISH_VICTORY_LAP:   { renderOuterExtraPattern(1, t, 92, 255, 0, 0, 0, false); break; }
    case FINISH_GOLD_THEATER:  { renderOuterExtraPattern(0, t, 36, 255, 0, 0, 0, false); break; }
    case FINISH_RIBBON_DANCE:  { renderOuterExtraPattern(3, t, (uint8_t)(t / 18), 230, 0, 0, 0, false); break; }
    case FINISH_TROPHY_GLOW:   { renderOuterExtraPattern(16, t, 34, 220, 0, 0, 0, false); break; }
    case FINISH_STAR_GLITTER:  { renderOuterExtraPattern(5, t, 42, 210, 0, 0, 0, false); break; }
    case FINISH_DUAL_COMETS:   { renderOuterExtraPattern(6, t, (uint8_t)(t / 25), 255, 0, 0, 0, false); break; }
    case FINISH_APPLAUSE:      { renderOuterExtraPattern(11, t, 45, 255, 0, 0, 0, false); break; }
    case FINISH_PRISM_BLOOM:   { renderOuterExtraPattern(12, t, (uint8_t)(t / 14), 255, 0, 0, 0, false); break; }
    case FINISH_PIXEL_TOAST:   { renderOuterExtraPattern(18, t, 38, 230, 0, 0, 0, false); break; }
    case FINISH_CROWN_CHASE:   { renderOuterExtraPattern(7, t, 32, 255, 0, 0, 0, false); break; }
    case FINISH_COOLDOWN_PROGRESS: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 35.0f, 240.0f, 20);
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 25.0f, 65.0f, 15);
        fillSectionMeter(1, chamberPct, tempColorFromPct(chamberPct), true);
        fillSectionMeter(3, toolPct, tempColorFromPct(toolPct), false);
        fillSec(2, secV(2, 0, 185, 95));
        setInsideAmbientRgb(tempColorFromPct((uint8_t)((toolPct + chamberPct) / 2)), 90, 28);
        break;
    }
    case FINISH_PRINT_SIGNATURE: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = hsvToRgb((uint8_t)(t / 12), 255, 220);
        renderOuterExtraPattern(12, t, (uint8_t)(t / 18), 210, fil.R, fil.G, fil.B, true);
        fillSectionMeter(2, 100, fil);
        setInsideAmbientRgb(fil, (uint8_t)(70 + sin8fast((uint8_t)(t / 14)) / 3), 25);
        break;
    }
    case FINISH_APPLAUSE_SMART: {
        uint32_t beat = t % 900UL;
        uint8_t clap = (beat < 85UL || (beat > 170UL && beat < 255UL)) ? 235 : 26;
        fillSec(1, secV(1, clap, (uint8_t)(clap * 7 / 10), 0));
        fillSec(3, secV(3, clap, (uint8_t)(clap * 7 / 10), 0));
        fillSectionMeter(2, 100, RgbColor(255, 210, 60));
        setInsideAmbientRgb(RgbColor(255, 170, 35), (uint8_t)(45 + clap / 4), 40);
        break;
    }
    case FINISH_TAKE_ME: {
        uint8_t pulse = sin8fast((uint8_t)(t / 10));
        fillSec(1, secV(1, 0, (uint8_t)(100 + pulse / 2), 50));
        fillSec(3, secV(3, 0, (uint8_t)(100 + pulse / 2), 50));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            bool on = (((i + (t / 120UL)) % 5) < 2);
            setTargetPixel(sectionVisualLed(2, i), on ? secV(2, 0, 235, 110) : secV(2, 0, 45, 25));
        }
        setInsideAmbientRgb(RgbColor(0, 220, 120), (uint8_t)(50 + pulse / 4), 35);
        break;
    }
    case FINISH_COOL_TO_TOUCH: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 30.0f, 220.0f, 10);
        RgbColor hot = tempColorFromPct(toolPct);
        RgbColor cool(0, 190, 255);
        RgbColor c = (toolPct > 30) ? hot : cool;
        fillSectionMeter(1, toolPct, c, true);
        fillSectionMeter(3, toolPct, c, false);
        fillSec(2, (toolPct > 30) ? secV(2, 210, 85, 0) : secV(2, 0, 180, 255));
        setInsideAmbientRgb(c, (uint8_t)(45 + toolPct / 3), (toolPct < 25) ? 35 : 10);
        break;
    }
    case FINISH_LAST_LAYER_GLOW: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(240, 240, 255);
        fillSectionMeter(2, 100, fil);
        uint8_t shimmer = sin8fast((uint8_t)(t / 9));
        fillSec(1, secV(1, (uint8_t)((uint16_t)fil.R * shimmer / 255), (uint8_t)((uint16_t)fil.G * shimmer / 255), (uint8_t)((uint16_t)fil.B * shimmer / 255)));
        fillSec(3, secV(3, (uint8_t)((uint16_t)fil.R * shimmer / 255), (uint8_t)((uint16_t)fil.G * shimmer / 255), (uint8_t)((uint16_t)fil.B * shimmer / 255)));
        setInsideAmbientRgb(fil, (uint8_t)(65 + shimmer / 4), 30);
        break;
    }
    case FINISH_GALLERY_MODE: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(255, 210, 160);
        uint8_t v = (uint8_t)(65 + sin8fast((uint8_t)(t / 32)) / 5);
        fillSec(1, secV(1, fil.R / 3, fil.G / 3, fil.B / 3));
        fillSec(2, secV(2, v, v, v));
        fillSec(3, secV(3, fil.R / 3, fil.G / 3, fil.B / 3));
        setInsideAmbientRgb(fil, (uint8_t)(45 + v / 3), 45);
        break;
    }
    case FINISH_FILAMENT_FIREWORKS: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = hsvToRgb((uint8_t)(t / 8), 255, 230);
        renderOuterExtraPattern(15, t, (uint8_t)(t / 10), 255, fil.R, fil.G, fil.B, true);
        setInsideAmbientRgb(fil, (uint8_t)(55 + sin8fast((uint8_t)(t / 7)) / 3), 20);
        break;
    }
    case FINISH_INSPECTION_LIGHT: {
        uint8_t sweep = (uint8_t)((t / 55UL) % FRONT_COUNT);
        fillSec(1, secV(1, 90, 90, 85));
        fillSec(3, secV(3, 90, 90, 85));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t v = (i == sweep || i + 1 == sweep || (sweep == 0 && i == FRONT_COUNT - 1)) ? 230 : 70;
            setTargetPixel(sectionVisualLed(2, i), secV(2, v, v, (uint8_t)(v * 9 / 10)));
        }
        setInsideAmbientRgb(RgbColor(255, 245, 220), 70, 70);
        break;
    }
    case FINISH_QUIET_PRIDE: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(255, 200, 80);
        uint8_t breath = sin8fast((uint8_t)(t / 34));
        fillSec(1, secV(1, fil.R / 3, fil.G / 3, fil.B / 3));
        fillSectionMeter(2, 100, RgbColor(255, 220, 130));
        fillSec(3, secV(3, fil.R / 3, fil.G / 3, fil.B / 3));
        setInsideAmbientRgb(fil, (uint8_t)(45 + breath / 5), 50);
        break;
    }
    case FINISH_CALM_DONE: {
        uint8_t breath = (uint8_t)(60U + sin8fast((uint8_t)(t / 44U)) / 5U);
        fillSec(1, secV(1, breath, (uint8_t)(breath * 3U / 4U), 15));
        fillSec(2, secV(2, (uint8_t)(breath * 4U / 5U), breath, 35));
        fillSec(3, secV(3, breath, (uint8_t)(breath * 3U / 4U), 15));
        break;
    }
    case FINISH_SILK_UNVEIL: {
        uint16_t fold = (uint16_t)((t / 85UL) % (FRONT_COUNT + LEFT_COUNT));
        fillSec(1, secV(1, 28, 18, 4));
        fillSec(3, secV(3, 28, 18, 4));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool seam = ((i + fold) % 9U) < 2U;
            uint8_t v = seam ? 180 : 20;
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 4U / 5U), (uint8_t)(v / 5U)));
        }
        break;
    }
    case FINISH_GOLDEN_HOUR: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t glow = sin8fast((uint8_t)(t / 46U + i * 7U));
            uint8_t v = (uint8_t)(55U + glow / 3U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 2U / 3U), (uint8_t)(v / 8U)));
        }
        break;
    }
    case FINISH_STARFALL: {
        fillSec(1, secV(1, 8, 8, 12));
        fillSec(2, secV(2, 10, 10, 16));
        fillSec(3, secV(3, 8, 8, 12));
        static const uint8_t stars[] = {1, 5, 12, 18, 25, 31, 37, 40};
        uint8_t drift = (uint8_t)(t / 210U);
        for (uint8_t s = 0; s < sizeof(stars); s++) {
            uint16_t p = (uint16_t)((stars[s] + drift) % OUTER_COUNT);
            uint8_t v = (uint8_t)(95U + sin8fast((uint8_t)(t / 38U + s * 31U)) / 2U);
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), v, (uint8_t)(v * 4U / 5U), (uint8_t)(v / 3U)));
        }
        break;
    }
    case FINISH_SIGNATURE_SWEEP: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = RgbColor(255, 210, 80);
        fillSec(1, secV(1, fil.R / 5, fil.G / 5, fil.B / 5));
        fillSec(3, secV(3, fil.R / 5, fil.G / 5, fil.B / 5));
        uint16_t sweep = (uint16_t)((t / 48UL) % FRONT_COUNT);
        for (uint8_t tr = 0; tr < 8; tr++) {
            uint16_t p = (sweep + FRONT_COUNT - tr) % FRONT_COUNT;
            uint8_t v = (uint8_t)(220U - tr * 24U);
            setTargetPixel(sectionVisualLed(2, p), secV(2, (uint8_t)((uint16_t)fil.R * v / 255U), (uint8_t)((uint16_t)fil.G * v / 255U), (uint8_t)((uint16_t)fil.B * v / 255U)));
        }
        break;
    }
    case FINISH_INSPECT_READY: {
        uint8_t phase = (uint8_t)((t / 1400UL) % 3U);
        fillSec(1, phase == 0 ? secV(1, 180, 180, 165) : secV(1, 45, 45, 40));
        fillSec(2, phase == 1 ? secV(2, 210, 210, 190) : secV(2, 55, 55, 48));
        fillSec(3, phase == 2 ? secV(3, 180, 180, 165) : secV(3, 45, 45, 40));
        uint16_t tick = (uint16_t)((t / 180UL) % FRONT_COUNT);
        if (phase == 1) {
            setTargetPixel(sectionVisualLed(2, tick), secV(2, 255, 255, 240));
        }
        break;
    }
    case FINISH_PRINT_ECHO: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = hsvToRgb((uint8_t)(t / 16U), 180, 200);
        uint8_t pct = (uint8_t)((t / 65UL) % 101U);
        fillSectionMeter(2, pct, fil);
        uint8_t side = (uint8_t)(40U + sin8fast((uint8_t)(t / 28U)) / 5U);
        fillSec(1, secV(1, (uint8_t)((uint16_t)fil.R * side / 255U), (uint8_t)((uint16_t)fil.G * side / 255U), (uint8_t)((uint16_t)fil.B * side / 255U)));
        fillSec(3, secV(3, (uint8_t)((uint16_t)fil.R * side / 255U), (uint8_t)((uint16_t)fil.G * side / 255U), (uint8_t)((uint16_t)fil.B * side / 255U)));
        break;
    }
    case FINISH_SOFT_APPLAUSE: {
        uint8_t wave = (uint8_t)((t / 95UL) % OUTER_COUNT);
        fillSec(1, secV(1, 24, 18, 4));
        fillSec(2, secV(2, 28, 22, 6));
        fillSec(3, secV(3, 24, 18, 4));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t d = (wave > i) ? wave - i : i - wave;
            if (d > OUTER_COUNT / 2U) d = OUTER_COUNT - d;
            if (d < 5U) {
                uint8_t v = (uint8_t)(155U - d * 22U);
                setTargetPixel(OUTER_START + i, secV(outerSectionIdx(OUTER_START + i), v, (uint8_t)(v * 4U / 5U), (uint8_t)(v / 6U)));
            }
        }
        break;
    }
    case FINISH_COOLDOWN_AURA: {
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 30.0f, 220.0f, 25);
        RgbColor warm = tempColorFromPct(toolPct);
        RgbColor cool(0, 180, 255);
        uint8_t mix = (toolPct > 100U) ? 255 : (uint8_t)(toolPct * 255U / 100U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t r = (uint8_t)(((uint16_t)warm.R * mix + (uint16_t)cool.R * (255U - mix)) / 255U);
            uint8_t g = (uint8_t)(((uint16_t)warm.G * mix + (uint16_t)cool.G * (255U - mix)) / 255U);
            uint8_t b = (uint8_t)(((uint16_t)warm.B * mix + (uint16_t)cool.B * (255U - mix)) / 255U);
            uint8_t v = (uint8_t)(45U + sin8fast((uint8_t)(t / 32U + i * 9U)) / 5U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)r * v / 255U), (uint8_t)((uint16_t)g * v / 255U), (uint8_t)((uint16_t)b * v / 255U)));
        }
        break;
    }
    case FINISH_SHOWCASE_LOOP: {
        uint8_t phase = (uint8_t)(t / 18U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t h = (uint8_t)(phase + i * 5U);
            uint8_t wave = sin8fast((uint8_t)(phase + i * 13U));
            uint8_t v = (uint8_t)(55U + wave / 2U);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 220, v));
        }
        break;
    }
    }
    if (!ledAllowsInsideRgbNow()) renderInsideNormalToTarget();
}

// -- OTHER animations ---------------------------------------------------------

static void renderOtherTarget() {
    clearTarget();
    uint32_t t = millis();
    switch (rtLed.otherAnim) {
    case OTHER_MATRIX: {
        // BUG-4 fix: fade was counter-based (mspeed++). Now millis step at 60ms.
        static const uint32_t MATRIX_STEP_MS = 60;
        static uint32_t lastMatrixStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t mstate[OUTER_COUNT] = {};
        if (t - lastMatrixStep >= MATRIX_STEP_MS) {
            lastMatrixStep = t - ((t - lastMatrixStep) % MATRIX_STEP_MS);
            for(uint16_t i = 0; i < OUTER_COUNT; i++) {
                if(mstate[i] > 0) mstate[i] = (uint8_t)((uint16_t)mstate[i] * 200 / 255);
                else if(random(60) == 0) mstate[i] = 255;
            }
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secV(outerSectionIdx(idx), 0, mstate[i], 0));
        }
        break;
    }
    case OTHER_CANDLE: {
        // BUG-9 fix: candle flicker step-guarded at 50ms - was per-frame (FPS-dependent)
        static const uint32_t CANDLE_STEP_MS = 50;
        static uint32_t lastCandleStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t ch[OUTER_COUNT] = {};
        if (t - lastCandleStep >= CANDLE_STEP_MS) {
            lastCandleStep = t - ((t - lastCandleStep) % CANDLE_STEP_MS);
            for(uint16_t i = 0; i < OUTER_COUNT; i++) {
                ch[i] = (uint8_t)(min((int32_t)255, (int32_t)ch[i] + (int32_t)random(-15, 20)));
                if(ch[i] < 80) ch[i] = 80;
                if(ch[i] > 230) ch[i] = 230;
            }
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t v = ch[i];
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v*60/100), (uint8_t)(v*10/100)));
        }
        break;
    }
    case OTHER_STATIC_RAINBOW: {
        for(uint16_t i=0;i<OUTER_COUNT;i++){uint16_t idx=OUTER_START+i;setTargetPixel(idx,secHSV(outerSectionIdx(idx),(uint8_t)(i*255/OUTER_COUNT),255,OUT_MAX));}
        break;
    }
    case OTHER_NEON_CLUB: {
        static uint8_t ncH=0;static uint32_t ncLast=0;
        if(t-ncLast>60){ncLast=t;ncH=(uint8_t)(ncH+30);}
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,ncH+sec*40,255,OUT_MAX));
        break;
    }
    case OTHER_SYNTHWAVE: {
        fillSec(1,secV(1,0,220,220));  // cyan
        fillSec(3,secV(3,220,0,220));  // magenta
        uint8_t sv=sectionBrightToV(rtLed.sectionBright[2]);
        for(uint16_t i=0;i<FRONT_COUNT;i++){
            uint8_t blend=(uint8_t)(i*255/(FRONT_COUNT - 1));
            uint8_t r=(uint8_t)((uint16_t)220*blend/255*sv/255);
            uint8_t g=(uint8_t)((uint16_t)220*(255-blend)/255*sv/255);
            uint8_t b=(uint8_t)((uint16_t)220*sv/255);
            setTargetPixel(sectionVisualLed(2, i),rgbToRgbwLimited(RgbColor(r,g,b),255,W_MAX_OUT));
        }
        break;
    }
    case OTHER_JELLYFISH: {
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t p1=(uint8_t)(t/30+i*17);uint8_t p2=(uint8_t)(t/20+i*11+60);
            uint8_t mix=(sin8fast(p1)+sin8fast(p2))/2;
            uint8_t h=(uint8_t)(160+mix/3);  // blue-purple range
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),h,200,(uint8_t)(80+mix/2)));
        }
        break;
    }
    case OTHER_SNOW: {
        // WARN-5 fix: decay step-guarded at 20ms
        static const uint32_t OSNOW_STEP_MS = 20;
        static uint32_t lastOSnowStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t snState[OUTER_COUNT] = {};
        if (t - lastOSnowStep >= OSNOW_STEP_MS) {
            lastOSnowStep = t - ((t - lastOSnowStep) % OSNOW_STEP_MS);
            for(uint16_t i = 0; i < OUTER_COUNT; i++) {
                if(snState[i] > 0) snState[i] = (uint8_t)((uint16_t)snState[i] * 220 / 255);
                else if(random(100) < 2) snState[i] = 255;
            }
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t sv = snState[i]; uint8_t bg = 20;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(bg+sv*235/255), (uint8_t)(bg+sv*235/255), (uint8_t)min((uint32_t)255, (uint32_t)bg+sv)));
        }
        break;
    }
    case OTHER_SUNSET: {
        // millis-based: t/400 per unit -> ~100s full cycle (was t/1600 = 409s!)
        uint8_t phase=(uint8_t)(t/400);
        // deep red -> orange -> gold
        uint8_t r=200,g=0,b=0;
        if(phase<85){g=(uint8_t)(phase*2);}
        else if(phase<170){g=(uint8_t)(170+(phase-85));b=0;r=min((uint32_t)255, (uint32_t)(200u+(phase-85)/2));}
        else{r=255;g=200;b=(uint8_t)((phase-170)*2);}
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,r,g,b));
        break;
    }
    case OTHER_VOLCANO: {
        // WARN-1 fix: state update step-guarded; sparks use separate step
        static const uint32_t VOL_STEP_MS = 30;
        static uint32_t lastVolStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t vh[OUTER_COUNT] = {};
        EXT_RAM_BSS_ATTR static uint8_t vSpark[OUTER_COUNT] = {};
        if (t - lastVolStep >= VOL_STEP_MS) {
            lastVolStep = t - ((t - lastVolStep) % VOL_STEP_MS);
            for(uint16_t i = 0; i < OUTER_COUNT; i++) {
                vh[i] = (uint8_t)(min((int32_t)255, (int32_t)vh[i] + (int32_t)random(-20, 30)));
                if(vh[i] < 50) vh[i] = 50;
                if(vh[i] > 240) vh[i] = 240;
                vSpark[i] = (random(40) == 0) ? 255 : vh[i];  // spark sampled once per step
            }
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t v = vSpark[i];
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v/4), 0));
        }
        break;
    }
    case OTHER_TECHNO: {
        bool on=(t%500<60);
        uint8_t v=on?240:0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,v,v,v));
        break;
    }
    case OTHER_DRAGON_BLOOD: {
        uint16_t ph=(t/6)%1024;uint8_t bv=(ph<512)?ph/4:(1023-ph)/4;
        uint8_t v=(uint8_t)(40+bv/2);
        bool flash=(random(200)==0);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,flash?secV(sec,0,0,0):secV(sec,v,0,0));
        break;
    }
    case OTHER_AURORA: {
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t p1=(uint8_t)(t/60+i*8);uint8_t p2=(uint8_t)(t/40+i*5+30);
            uint8_t mix=(sin8fast(p1)+sin8fast(p2))/2;
            uint8_t h=(uint8_t)(85+mix/2);  // green->teal->blue
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),h,220,(uint8_t)(60+mix/2)));
        }
        break;
    }
    case OTHER_CYBERPUNK: {
        bool lBlink=(t%600<200);bool rBlink=(t%600>=300&&t%600<500);
        fillSec(1,lBlink?secV(1,0,220,220):secV(1,0,20,20));
        fillSec(3,rBlink?secV(3,220,0,180):secV(3,20,0,15));
        fillSec(2,secV(2,0,180,160));
        break;
    }
    case OTHER_NEBULA: {
        // millis-based: hue from t/16
        uint8_t h=(uint8_t)(t/16);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,h,255,OUT_MAX));
        break;
    }
    case OTHER_SUBMARINE: {
        // BUG-6 fix: bubble movement dt-based at 20ms; bubbles on both sides (was left only)
        static const uint32_t SUB_STEP_MS = 20;
        static uint32_t lastSubStep = 0;
        EXT_RAM_BSS_ATTR static float bub[3]  = {5.0f, 12.0f, 20.0f};
        EXT_RAM_BSS_ATTR static float bubR[3] = {3.0f, 7.0f,  9.0f};   // right side bubbles
        static const float bspeed[3] = {0.08f, 0.12f, 0.06f};
        if (t - lastSubStep >= SUB_STEP_MS) {
            lastSubStep = t - ((t - lastSubStep) % SUB_STEP_MS);
            for(uint8_t b = 0; b < 3; b++) {
                // LEFT visual order: 0=top, 10=bottom; decreasing moves upward.
                bub[b]  -= bspeed[b]; if(bub[b]  < 0) bub[b]  = (float)LEFT_COUNT - 0.1f;
                // RIGHT visual order: 0=top, 10=bottom; increasing moves downward.
                bubR[b] += bspeed[b]; if(bubR[b] >= (float)RIGHT_COUNT) bubR[b] = 0.0f;
            }
        }
        fillSec(1, secV(1, 0, 40, 20));
        fillSec(3, secV(3, 0, 40, 20));
        fillSec(2, secV(2, 0, 50, 25));
        for(uint8_t b = 0; b < 3; b++) {
            uint8_t pL = (uint8_t)bub[b];
            uint8_t pR = (uint8_t)bubR[b];
            if(pL < LEFT_COUNT) setTargetPixel(sectionVisualLed(1, pL), secV(1, 150, 220, 200));
            if(pR < RIGHT_COUNT) setTargetPixel(sectionVisualLed(3, pR), secV(3, 150, 220, 200));
        }
        break;
    }
    case OTHER_PRIDE: {
        static const uint8_t prideH[]={0,15,30,85,160,210};
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t seg=(uint8_t)(i*6/OUTER_COUNT);
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),prideH[seg],255,OUT_MAX));
        }
        break;
    }
    case OTHER_PLASMA: {
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            uint8_t p1=sin8fast((uint8_t)(t/20+i*15));uint8_t p2=sin8fast((uint8_t)(t/13+i*9+60));
            setTargetPixel(idx,secHSV(outerSectionIdx(idx),(uint8_t)((p1+p2)/2),255,OUT_MAX));
        }
        break;
    }
    case OTHER_BOUNCING_BALLS: {
        // Full-outer ping-pong balls with eased travel and a soft trailing glow.
        static const uint8_t BALL_COUNT = 5;
        static const uint8_t TRAIL_LEN = 6;
        static const uint8_t bbH[BALL_COUNT] = {0, 51, 102, 153, 204};
        static const uint16_t bbPeriodMs[BALL_COUNT] = {3600, 4100, 4700, 5300, 6100};
        static const uint16_t bbPhaseMs[BALL_COUNT] = {0, 700, 1400, 2300, 3200};
        const uint16_t span = (OUTER_COUNT > 1) ? (OUTER_COUNT - 1) : 1;

        for (uint8_t b = 0; b < BALL_COUNT; b++) {
            uint16_t phase = (uint16_t)((t + bbPhaseMs[b]) % bbPeriodMs[b]);
            bool movingRight = (phase < (bbPeriodMs[b] / 2U));
            uint16_t halfPhase = movingRight ? phase : (uint16_t)(phase - (bbPeriodMs[b] / 2U));
            uint16_t u = (uint16_t)(((uint32_t)halfPhase * 255UL) / (bbPeriodMs[b] / 2U));
            uint16_t eased = (uint16_t)((uint32_t)u * u * (765U - 2U * u) / 65025UL); // smoothstep 0..255
            uint16_t pos = (uint16_t)(((uint32_t)eased * span + 127U) / 255U);
            if (!movingRight) pos = span - pos;

            for (uint8_t trail = 0; trail < TRAIL_LEN; trail++) {
                int16_t trailPos = movingRight ? ((int16_t)pos - trail) : ((int16_t)pos + trail);
                if (trailPos < 0 || trailPos >= (int16_t)OUTER_COUNT) continue;
                uint16_t idx = OUTER_START + (uint16_t)trailPos;
                uint8_t v = (uint8_t)(((uint16_t)OUT_MAX * (TRAIL_LEN - trail)) / TRAIL_LEN);
                setTargetPixel(idx, secHSV(outerSectionIdx(idx), bbH[b], 255, v));
            }
        }
        break;
    }
    case OTHER_COP_CAR: {
        bool lB=(t%400<200);
        fillSec(1,lB?secV(1,0,0,220):secV(1,5,5,5));
        fillSec(3,lB?secV(3,5,5,5):secV(3,220,0,0));
        fillSec(2,secV(2,10,5,5));
        break;
    }
    case OTHER_STROBE_PARTY: {
        // millis-based: hue from t/500 (changes every 500ms)
        uint8_t sph=(uint8_t)((t/500)*20);
        bool on=(t%100<50);
        uint8_t v=on?OUT_MAX:0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,sph+(uint8_t)(sec*40),255,v));
        break;
    }
    case OTHER_SUNRISE: {
        // millis-based: t/400 -> ~100s full cycle (was t/900 = 230s)
        uint8_t ph=(uint8_t)(t/400);
        uint8_t r=min((uint32_t)255, (uint32_t)(100u+ph));uint8_t g=(uint8_t)(ph/2);uint8_t b=0;
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,r,g,b));
        break;
    }
    case OTHER_OCEANIC_DEPTH: {
        EXT_RAM_BSS_ATTR static uint8_t odState[OUTER_COUNT]={};
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            if(odState[i]>0)odState[i]=(uint8_t)((uint16_t)odState[i]*200/255);
            else if(random(200)==0)odState[i]=255;
            uint16_t idx=OUTER_START+i;
            uint8_t sp=odState[i];
            setTargetPixel(idx,secV(outerSectionIdx(idx),(uint8_t)(5+(uint16_t)sp/8),(uint8_t)(20+(uint16_t)sp/4),(uint8_t)(30+(uint16_t)sp/2)));
        }
        break;
    }
    case OTHER_RADIATION: {
        uint32_t cyc=(t/20)%40;
        for(uint16_t i=0;i<OUTER_COUNT;i++){
            uint16_t idx=OUTER_START+i;
            // Distance from centre of outer strip - scales with OUTER_COUNT
            uint16_t half=OUTER_COUNT/2;
            uint16_t dist=(uint16_t)(i<half ? half-i : i-half);
            bool on2=(dist==(cyc%(OUTER_COUNT/2)));
            setTargetPixel(idx,on2?secV(outerSectionIdx(idx),80,220,0):secV(outerSectionIdx(idx),5,15,0));
        }
        break;
    }
    case OTHER_PASTEL: {
        // millis-based: t/20 per hue unit -> ~5s cycle (was t/64 = 16s)
        uint8_t h=(uint8_t)(t/20);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,h+(uint8_t)(sec*20),120,200));
        break;
    }
    case OTHER_ELECTRIC: {
        // BUG-8 fix: eltail fade was outside step guard (FPS-dependent). Now fully inside.
        static const uint32_t EL_STEP_MS = 20;
        static uint32_t lastElStep = 0;
        static uint16_t elpos = 0;
        EXT_RAM_BSS_ATTR static uint8_t eltail[OUTER_COUNT] = {};
        if (t - lastElStep >= EL_STEP_MS) {
            lastElStep = t - ((t - lastElStep) % EL_STEP_MS);
            elpos = (elpos + 2) % OUTER_COUNT;
            for(uint16_t i = 0; i < OUTER_COUNT; i++) eltail[i] = (uint8_t)((uint16_t)eltail[i] * 170 / 255);
            eltail[elpos] = 255;
            if(elpos + 1 < OUTER_COUNT) eltail[elpos + 1] = 200;
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)eltail[i]/3), (uint8_t)((uint16_t)eltail[i]/3), eltail[i]));
        }
        break;
    }
    case OTHER_RAINBOW_PULSE: {
        // millis-based: t/16 per unit
        uint32_t rpT = t / 16;
        uint8_t h=(uint8_t)(rpT/3);
        uint16_t ph=(uint16_t)((rpT*4)%1024);uint8_t bv=(ph<512)?(uint8_t)(ph/4):(uint8_t)((1023-ph)/4);
        uint8_t v=(uint8_t)(60+bv/2);
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,h+(uint8_t)(sec*30),255,v));
        break;
    }
    case OTHER_CARNIVAL: {
        uint32_t rot=(t/500)%3;
        static const uint8_t carH[]={0,30,170};  // red,orange,blue
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,carH[(sec-1+rot)%3],255,OUT_MAX));
        break;
    }
    case OTHER_NEON_SIGN: {
        EXT_RAM_BSS_ATTR static uint8_t nsPhase[3]={0,0,0};EXT_RAM_BSS_ATTR static uint32_t nsLast[3]={};
        for(uint8_t sec=1;sec<=3;sec++){
            uint32_t delay=(sec==1)?3000:(sec==2)?2000:4000;
            if(t-nsLast[sec-1]>delay){nsLast[sec-1]=t;nsPhase[sec-1]=(nsPhase[sec-1]+1)%3;}
            uint8_t ph=nsPhase[sec-1];
            uint8_t h=(sec==1)?0:(sec==2)?85:200;
            // Phase 0: flicker-on, 1: stable, 2: off
            uint8_t v=(ph==1)?OUT_MAX:(ph==0&&random(5)>0)?OUT_MAX:0;
            fillSec(sec,secHSV(sec,h,255,v));
        }
        break;
    }
    case OTHER_MOTION_DETECT: {
        static uint32_t lastFlash=0;static uint32_t flashDur=0;
        if(t-lastFlash>3000){lastFlash=t;flashDur=t+800;}
        bool on2=t<flashDur;
        if(on2)for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secV(sec,255,255,255));
        break;
    }
    case OTHER_RETRO_TV: {
        // BUG-E fix: was 40x random() per frame (2000/s at 50fps).
        // Now resampled every 50ms - still looks like static but far cheaper.
        static const uint32_t TV_STEP_MS = 50;
        static uint32_t lastTvStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t tvR[OUTER_COUNT] = {};
        EXT_RAM_BSS_ATTR static uint8_t tvG[OUTER_COUNT] = {};
        EXT_RAM_BSS_ATTR static uint8_t tvB[OUTER_COUNT] = {};
        if (t - lastTvStep >= TV_STEP_MS) {
            lastTvStep = t - ((t - lastTvStep) % TV_STEP_MS);
            for(uint16_t i = 0; i < OUTER_COUNT; i++) {
                tvR[i] = (uint8_t)random(256);
                tvG[i] = (uint8_t)random(256);
                tvB[i] = (uint8_t)random(256);
            }
        }
        for(uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            setTargetPixel(idx, secV(outerSectionIdx(idx), tvR[i], tvG[i], tvB[i]));
        }
        break;
    }
    case OTHER_CRYSTAL: {
        // millis-based: t/100 per hue unit
        uint8_t h=(uint8_t)(160+t/100);  // cold blue-violet range
        for(uint8_t sec=1;sec<=3;sec++)fillSec(sec,secHSV(sec,h,180,200));
        break;
    }
    case OTHER_FIRE_ICE: default: {
        // Left=fire (warm), Right=ice (cool dynamic), Center pulses between
        // BUG-7 fix: ice now has symmetrical heat-array dynamics like fire
        // BUG-5 fix (LOGIC-5): fiC removed - was declared but never used
        EXT_RAM_BSS_ATTR static uint8_t fiH[LEFT_COUNT] = {};  // fire heat (left)
        EXT_RAM_BSS_ATTR static uint8_t icH[RIGHT_COUNT] = {};  // ice heat (right)
        // Fire: propagate + cool + heat
        for(int i = (int)LEFT_COUNT - 1; i > 0; i--) fiH[i] = (uint8_t)((fiH[i-1] + fiH[i] + fiH[i]) / 3);
        fiH[0] = (uint8_t)max((int32_t)0,   (int32_t)fiH[0] - (int32_t)random(30, 80));
        fiH[0] = (uint8_t)min((int32_t)255, (int32_t)fiH[0] + (int32_t)random(60, 160));
        // Ice: same dynamics, different color mapping
        for(int i = (int)RIGHT_COUNT - 1; i > 0; i--) icH[i] = (uint8_t)((icH[i-1] + icH[i] + icH[i]) / 3);
        icH[0] = (uint8_t)max((int32_t)0,   (int32_t)icH[0] - (int32_t)random(30, 80));
        icH[0] = (uint8_t)min((int32_t)255, (int32_t)icH[0] + (int32_t)random(60, 160));
        uint8_t bL = sectionBrightToV(rtLed.sectionBright[1]);
        uint8_t bR = sectionBrightToV(rtLed.sectionBright[3]);
        for(uint16_t i = 0; i < LEFT_COUNT; i++) {
            // Fire color: black->red->orange->yellow
            // LEFT visual: i=0=top, i=10=bottom.
            uint8_t fv = fiH[(LEFT_COUNT - 1) - i];
            uint8_t fr = (fv < 85) ? fv * 3 : 255;
            uint8_t fg = (fv < 85) ? 0 : (fv < 170) ? (fv - 85) * 3 : 255;
            setTargetPixel(sectionVisualLed(1, i),
                rgbToRgbwLimited(RgbColor((uint16_t)fr*bL/255, (uint16_t)fg*bL/255, 0), 255, W_MAX_OUT));
            // RIGHT visual: i=0=top, i=10=bottom.
            uint8_t iv = icH[i];
            uint8_t ib = (iv < 85) ? iv * 3 : 255;
            uint8_t ig = (iv < 170) ? 0 : (iv - 170) * 3;
            uint8_t ir = (iv < 170) ? 0 : (iv - 170);
            setTargetPixel(sectionVisualLed(3, i),
                rgbToRgbwLimited(RgbColor((uint16_t)ir*bR/255, (uint16_t)ig*bR/255, (uint16_t)ib*bR/255), 255, W_MAX_OUT));
        }
        // Center: pulse between warm and cool
        uint16_t ph = (t / 4) % 1024;
        uint8_t bv = (ph < 512) ? ph / 4 : (1023 - ph) / 4;
        uint8_t sv = sectionBrightToV(rtLed.sectionBright[2]);
        uint8_t cv = (uint8_t)((uint16_t)(60 + bv / 2) * sv / 255);
        fillSec(2, secV(2, cv / 2, cv / 2, cv));
        break;
    }
    case OTHER_LASER_GRID:      { renderOuterExtraPattern(0,  t, 120, 255, 0, 0, 0, false); break; }
    case OTHER_GALAXY_SPIN:     { renderOuterExtraPattern(10, t, (uint8_t)(t / 18), 230, 0, 0, 0, false); break; }
    case OTHER_COMET_TWINS:     { renderOuterExtraPattern(6,  t, 175, 255, 0, 0, 0, false); break; }
    case OTHER_DEEP_SEA_PULSE:  { renderOuterExtraPattern(16, t, 145, 230, 0, 0, 0, false); break; }
    case OTHER_SOLAR_WIND:      { renderOuterExtraPattern(8,  t, 30, 240, 0, 0, 0, false); break; }
    case OTHER_PIXEL_CIRCUS:    { renderOuterExtraPattern(7,  t, (uint8_t)(t / 12), 255, 0, 0, 0, false); break; }
    case OTHER_MINT_BREEZE:     { renderOuterExtraPattern(13, t, 105, 140, 0, 0, 0, false); break; }
    case OTHER_RUBY_SCAN:       { renderOuterExtraPattern(17, t, 248, 255, 0, 0, 0, false); break; }
    case OTHER_ARCADE_CHASE:    { renderOuterExtraPattern(11, t, (uint8_t)(t / 20), 255, 0, 0, 0, false); break; }
    case OTHER_STARDUST:        { renderOuterExtraPattern(5,  t, 38, 180, 0, 0, 0, false); break; }
    case OTHER_ICE_CAVE:        { renderOuterExtraPattern(12, t, 150, 120, 0, 0, 0, false); break; }
    case OTHER_FIREWORK_TRAIL:  { renderOuterExtraPattern(15, t, (uint8_t)(t / 10), 255, 0, 0, 0, false); break; }
    case OTHER_CHROMA_RING:     { renderOuterExtraPattern(3,  t, (uint8_t)(t / 16), 255, 0, 0, 0, false); break; }
    case OTHER_GHOST_LIGHT:     { renderOuterExtraPattern(1,  t / 2, 170, 70, 0, 0, 0, false); break; }
    case OTHER_TOXIC_WAVE:      { renderOuterExtraPattern(18, t, 72, 255, 0, 0, 0, false); break; }
    case OTHER_COPPER_SPARK:    { renderOuterExtraPattern(19, t, 24, 220, 0, 0, 0, false); break; }
    case OTHER_BLUEPRINT:       { renderOuterExtraPattern(14, t, 155, 180, 0, 0, 0, false); break; }
    case OTHER_MAGMA_FLOW:      { renderOuterExtraPattern(4,  t, 8, 255, 0, 0, 0, false); break; }
    case OTHER_CANDY_STRIPE:    { renderOuterExtraPattern(2,  t, 225, 180, 0, 0, 0, false); break; }
    case OTHER_QUANTUM_DOTS:    { renderOuterExtraPattern(19, t, (uint8_t)(t / 7), 255, 0, 0, 0, false); break; }
    case OTHER_SHOWROOM_LOOP: {
        uint32_t cue = t % 9000UL;
        if (cue < 2500UL) renderOuterExtraPattern(13, t, 175, 210, 0, 0, 0, false);
        else if (cue < 5200UL) renderOuterExtraPattern(6, t, (uint8_t)(t / 22), 255, 0, 0, 0, false);
        else renderOuterExtraPattern(12, t, (uint8_t)(t / 14), 255, 255, 120, 0, true);
        setInsideAmbientRgb((cue < 2500UL) ? RgbColor(90, 0, 255) : (cue < 5200UL ? RgbColor(0, 220, 255) : RgbColor(255, 125, 0)), (uint8_t)(75 + sin8fast((uint8_t)(t / 10)) / 3), 10);
        break;
    }
    case OTHER_AUDIO_REACTIVE_FAKE: {
        uint8_t bass = (uint8_t)((sin8fast((uint8_t)(t / 7)) + sin8fast((uint8_t)(t / 17))) / 2);
        for (uint16_t i = 0; i < FRONT_COUNT; i++) {
            uint8_t bar = sin8fast((uint8_t)(t / 9 + i * 21));
            setTargetPixel(sectionVisualLed(2, i), secHSV(2, (uint8_t)(150 + i * 4), 255, (uint8_t)(30 + bar * 190 / 255)));
        }
        fillSec(1, secV(1, bass, 0, bass / 2));
        fillSec(3, secV(3, 0, bass / 2, bass));
        setInsideAmbientRgb(RgbColor(bass, 0, 255), (uint8_t)(50 + bass / 3), 0);
        break;
    }
    case OTHER_WEATHER_MOOD: {
        uint8_t day = (uint8_t)(t / 60000UL);
        RgbColor mood = hsvToRgb((uint8_t)(95 + sin8fast(day) / 3), 170, 210);
        renderOuterExtraPattern(8, t, (uint8_t)(95 + day), 180, mood.R, mood.G, mood.B, true);
        setInsideAmbientRgb(mood, (uint8_t)(45 + sin8fast((uint8_t)(t / 38)) / 4), 25);
        break;
    }
    case OTHER_CLOCK_AURORA: {
        renderOuterExtraPattern(13, t, (uint8_t)(t / 70), 170, 0, 0, 0, false);
        uint16_t tick = (uint16_t)((t / 1000UL) % OUTER_COUNT);
        setTargetPixel(OUTER_START + tick, secV(outerSectionIdx(OUTER_START + tick), 255, 255, 255));
        setInsideAmbientRgb(hsvToRgb((uint8_t)(120 + t / 90), 180, 220), 70, 18);
        break;
    }
    case OTHER_FILAMENT_GALLERY: {
        RgbColor fil = rtPrinter.filamentColor;
        if ((uint16_t)fil.R + fil.G + fil.B < 30) fil = hsvToRgb((uint8_t)(t / 30), 255, 230);
        fillSec(1, secV(1, fil.R, fil.G, fil.B));
        fillSectionMeter(2, rtPrinter.progress, fil);
        fillSec(3, secV(3, fil.R / 2, fil.G / 2, fil.B / 2));
        setInsideAmbientRgb(fil, (uint8_t)(60 + sin8fast((uint8_t)(t / 24)) / 4), 35);
        break;
    }
    case OTHER_MAINTENANCE_MODE: {
        uint8_t scan = (uint8_t)(t / 60);
        fillSec(1, secV(1, 255, 120, 0));
        fillSec(2, secV(2, 25, 25, 25));
        fillSec(3, secV(3, 255, 120, 0));
        for (uint16_t i = 0; i < FRONT_COUNT; i++) if (((i + scan) % 7) == 0) setTargetPixel(sectionVisualLed(2, i), secV(2, 255, 220, 80));
        setInsideAmbientRgb(RgbColor(255, 120, 0), 55, 18);
        break;
    }
    case OTHER_CALIBRATION_RULER: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            bool major = (i % 5) == 0;
            bool minor = (i % 2) == 0;
            setTargetPixel(OUTER_START + i, major ? secV(outerSectionIdx(OUTER_START + i), 255, 255, 255) : (minor ? secV(outerSectionIdx(OUTER_START + i), 0, 120, 255) : RgbwColor(0,0,0,0)));
        }
        uint16_t pos = (uint16_t)((t / 70UL) % OUTER_COUNT);
        setTargetPixel(OUTER_START + pos, secV(outerSectionIdx(OUTER_START + pos), 255, 80, 0));
        setInsideAmbientRgb(RgbColor(0, 120, 255), 45, 25);
        break;
    }
    case OTHER_HEATMAP_DEMO: {
        uint8_t chamberPct = clampPctFloat(rtVent.chamberTempC, 25.0f, 65.0f, 45);
        uint8_t toolPct = clampPctFloat(rtPrinter.activeToolTempC, 35.0f, 260.0f, 70);
        fillSectionMeter(1, chamberPct, tempColorFromPct(chamberPct), true);
        fillSectionMeter(2, (uint8_t)((chamberPct + toolPct) / 2), tempColorFromPct((uint8_t)((chamberPct + toolPct) / 2)));
        fillSectionMeter(3, toolPct, tempColorFromPct(toolPct));
        setInsideAmbientRgb(tempColorFromPct((uint8_t)((chamberPct + toolPct) / 2)), 95, 25);
        break;
    }
    case OTHER_PRODUCT_HERO: {
        uint32_t cue = t % 12000UL;
        uint8_t h = (cue < 4000UL) ? 190 : (cue < 8000UL ? 130 : 24);
        renderOuterExtraPattern((cue < 4000UL) ? 16 : (cue < 8000UL ? 3 : 7), t, h, 235, 0, 0, 0, false);
        setInsideAmbientRgb(hsvToRgb(h, 230, 255), (uint8_t)(80 + sin8fast((uint8_t)(t / 9)) / 3), 20);
        break;
    }
    case OTHER_NIGHT_LIGHT: {
        uint8_t breath = sin8fast((uint8_t)(t / 45));
        fillSec(1, secV(1, 18, 10, 3));
        fillSec(2, secV(2, 28, 15, 5));
        fillSec(3, secV(3, 18, 10, 3));
        setInsideAmbientRgb(RgbColor(255, 145, 55), (uint8_t)(28 + breath / 7), 65);
        break;
    }
    case OTHER_FOCUS_MODE: {
        uint8_t breath = sin8fast((uint8_t)(t / 32));
        fillSec(1, secV(1, 0, 45, 85));
        fillSec(2, secV(2, 35, 70, 95));
        fillSec(3, secV(3, 0, 45, 85));
        setInsideAmbientRgb(RgbColor(0, 150, 210), (uint8_t)(38 + breath / 6), 32);
        break;
    }
    case OTHER_PARTY_LOCK: {
        renderOuterExtraPattern(11, t / 2, (uint8_t)(t / 7), 255, 0, 0, 0, false);
        bool strobe = (t % 430UL) < 45UL;
        setInsideAmbientRgb(strobe ? RgbColor(255, 255, 255) : hsvToRgb((uint8_t)(t / 8), 255, 255), strobe ? 210 : 85, 0);
        break;
    }
    case OTHER_RETRO_TERMINAL: {
        fillSec(1, secV(1, 0, 40, 0));
        fillSec(2, secV(2, 0, 55, 0));
        fillSec(3, secV(3, 0, 40, 0));
        uint16_t scan = (uint16_t)((t / 55UL) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 5; tr++) {
            uint16_t p = (scan + OUTER_COUNT - tr) % OUTER_COUNT;
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), 0, (uint8_t)(220 - tr * 35), 20));
        }
        setInsideAmbientRgb(RgbColor(0, 180, 30), 55, 10);
        break;
    }
    case OTHER_PLASMA_CORE: {
        renderOuterExtraPattern(10, t, (uint8_t)(t / 12), 240, 0, 0, 0, false);
        RgbColor core = hsvToRgb((uint8_t)((sin8fast((uint8_t)(t / 15)) + t / 18) & 0xFF), 255, 255);
        setInsideAmbientRgb(core, (uint8_t)(70 + sin8fast((uint8_t)(t / 8)) / 3), 0);
        break;
    }
    case OTHER_STATUS_MIRROR: {
        RgbColor c = rtPrinter.moonrakerOnline ? RgbColor(0, 190, 120) : RgbColor(255, 90, 0);
        fillSectionMeter(2, rtPrinter.progress, c);
        fillSec(1, secV(1, c.R / 2, c.G / 2, c.B / 2));
        fillSec(3, secV(3, c.R / 2, c.G / 2, c.B / 2));
        setInsideAmbientRgb(c, (uint8_t)(45 + sin8fast((uint8_t)(t / 18)) / 4), 25);
        break;
    }
    case OTHER_BREATHE_WITH_TIME: {
        uint8_t breath = sin8fast((uint8_t)(t / 28));
        uint8_t h = (uint8_t)(t / 95);
        for (uint8_t sec = 1; sec <= 3; sec++) fillSec(sec, secHSV(sec, (uint8_t)(h + sec * 18), 155, (uint8_t)(45 + breath / 2)));
        setInsideAmbientRgb(hsvToRgb(h, 155, 255), (uint8_t)(45 + breath / 3), 25);
        break;
    }
    case OTHER_DEMO_ALL_SECTIONS: {
        fillSec(1, secV(1, 255, 60, 0));
        fillSec(2, secV(2, 0, 180, 255));
        fillSec(3, secV(3, 120, 0, 255));
        uint16_t pos = (uint16_t)((t / 70UL) % OUTER_COUNT);
        setTargetPixel(OUTER_START + pos, secV(outerSectionIdx(OUTER_START + pos), 255, 255, 255));
        setInsideAmbientRgb(RgbColor(255, 255, 255), 70, 45);
        break;
    }
    case OTHER_CINEMA_IDLE: {
        uint8_t wave = sin8fast((uint8_t)(t / 25));
        fillSec(1, secV(1, 18, 8, 0));
        fillSec(2, secV(2, (uint8_t)(45 + wave / 4), (uint8_t)(22 + wave / 8), 3));
        fillSec(3, secV(3, 0, 22, 45));
        uint16_t pos = (uint16_t)((t / 95UL) % OUTER_COUNT);
        for (uint8_t tr = 0; tr < 6; tr++) {
            uint16_t p = (pos + OUTER_COUNT - tr) % OUTER_COUNT;
            setTargetPixel(OUTER_START + p, secV(outerSectionIdx(OUTER_START + p), (uint8_t)(180 - tr * 24), (uint8_t)(95 - tr * 11), 20));
        }
        setInsideAmbientRgb(RgbColor(255, 110, 30), (uint8_t)(38 + wave / 5), 30);
        break;
    }
    case OTHER_LUXURY_AMBIENT: {
        uint8_t breath = sin8fast((uint8_t)(t / 40));
        fillSec(1, secV(1, 55, 38, 12));
        fillSec(2, secV(2, (uint8_t)(80 + breath / 8), (uint8_t)(56 + breath / 10), 18));
        fillSec(3, secV(3, 22, 38, 62));
        setInsideAmbientRgb(RgbColor(255, 170, 72), (uint8_t)(42 + breath / 5), 55);
        break;
    }
    case OTHER_SPECTRUM_SCANNER: {
        uint16_t pos = (uint16_t)((t / 45UL) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint8_t h = (uint8_t)(i * 255 / OUTER_COUNT + t / 20);
            uint16_t d = (pos > i) ? pos - i : i - pos;
            if (d > OUTER_COUNT / 2) d = OUTER_COUNT - d;
            uint8_t v = (d < 6) ? (uint8_t)(230 - d * 28) : 18;
            setTargetPixel(OUTER_START + i, secHSV(outerSectionIdx(OUTER_START + i), h, 255, v));
        }
        setInsideAmbientRgb(hsvToRgb((uint8_t)(t / 14), 255, 255), (uint8_t)(58 + sin8fast((uint8_t)(t / 12)) / 4), 8);
        break;
    }
    case OTHER_CALM_DOWN: {
        uint8_t breath = (uint8_t)(35U + sin8fast((uint8_t)(t / 52U)) / 5U);
        fillSec(1, secV(1, 0, (uint8_t)(breath * 3U / 4U), breath));
        fillSec(2, secV(2, (uint8_t)(breath / 4U), breath, (uint8_t)(breath * 2U / 3U)));
        fillSec(3, secV(3, 0, (uint8_t)(breath * 3U / 4U), breath));
        setInsideAmbientRgb(RgbColor(0, 190, 170), (uint8_t)(35 + breath / 3), 35);
        break;
    }
    case OTHER_MEDITATION: {
        uint8_t ring = sin8fast((uint8_t)(t / 58U));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t mirror = (i > OUTER_COUNT / 2U) ? (OUTER_COUNT - i) : i;
            uint8_t v = (uint8_t)(18U + ((uint16_t)ring * (mirror + 2U) / (OUTER_COUNT / 2U + 2U)) / 4U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(v / 3U), v, (uint8_t)(v * 3U / 4U)));
        }
        setInsideAmbientRgb(RgbColor(35, 180, 145), (uint8_t)(35 + ring / 6), 45);
        break;
    }
    case OTHER_BIOLUMINESCENCE: {
        fillSec(1, secV(1, 0, 12, 20));
        fillSec(2, secV(2, 0, 18, 28));
        fillSec(3, secV(3, 0, 12, 20));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint8_t glow = extraSpark(i, t / 3U, 223);
            if (glow > 205U) {
                uint16_t idx = OUTER_START + i;
                setTargetPixel(idx, secV(outerSectionIdx(idx), 0, glow, (uint8_t)(glow * 3U / 4U)));
            }
        }
        setInsideAmbientRgb(RgbColor(0, 190, 180), 55, 15);
        break;
    }
    case OTHER_LIQUID_GLASS: {
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t caustic = (uint8_t)((sin8fast((uint8_t)(t / 30U + i * 10U)) + sin8fast((uint8_t)(t / 47U + i * 17U))) / 2U);
            uint8_t v = (uint8_t)(30U + caustic / 2U);
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)(v * 2U / 5U), (uint8_t)(v * 4U / 5U), v));
        }
        setInsideAmbientRgb(RgbColor(100, 220, 255), (uint8_t)(55 + sin8fast((uint8_t)(t / 18)) / 5), 55);
        break;
    }
    case OTHER_EMBER_ROOM: {
        static const uint32_t EMBER_STEP_MS = 65;
        static uint32_t lastEmberStep = 0;
        EXT_RAM_BSS_ATTR static uint8_t ember[OUTER_COUNT] = {};
        if (t - lastEmberStep >= EMBER_STEP_MS) {
            lastEmberStep = t - ((t - lastEmberStep) % EMBER_STEP_MS);
            for (uint16_t i = 0; i < OUTER_COUNT; i++) {
                int v = (int)ember[i] + (int)random(-10, 18);
                if (v < 45) v = 45;
                if (v > 190) v = 190;
                ember[i] = (uint8_t)v;
            }
        }
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t v = ember[i];
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 2U / 5U), 0));
        }
        setInsideAmbientRgb(RgbColor(255, 95, 20), 58, 25);
        break;
    }
    case OTHER_NEON_RAIN: {
        uint16_t fall = (uint16_t)(t / 55UL);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t lane = (uint8_t)((i * 7U + fall) % 18U);
            uint8_t v = (lane < 3U) ? (uint8_t)(220U - lane * 45U) : 8;
            uint8_t h = (uint8_t)(i * 13U + t / 19U);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 255, v));
        }
        setInsideAmbientRgb(hsvToRgb((uint8_t)(t / 14), 255, 240), 70, 0);
        break;
    }
    case OTHER_SOLAR_ECLIPSE: {
        uint16_t corona = (uint16_t)((t / 90UL) % OUTER_COUNT);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint16_t d = (corona > i) ? corona - i : i - corona;
            if (d > OUTER_COUNT / 2U) d = OUTER_COUNT - d;
            uint8_t v = (d < 8U) ? (uint8_t)(210U - d * 22U) : 6;
            setTargetPixel(idx, secV(outerSectionIdx(idx), v, (uint8_t)(v * 2U / 5U), 0));
        }
        setInsideAmbientRgb(RgbColor(255, 130, 20), 48, 0);
        break;
    }
    case OTHER_CRYSTAL_PRISM: {
        uint8_t shift = (uint8_t)(t / 180U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            uint8_t facet = (uint8_t)((i * 5U + shift) % 12U);
            uint8_t h = (uint8_t)(facet * 21U + (i % 3U) * 9U);
            uint8_t v = (facet == 0U || facet == 5U || facet == 9U) ? 210 : (uint8_t)(36U + facet * 6U);
            setTargetPixel(idx, secHSV(outerSectionIdx(idx), h, 135, v));
        }
        setInsideAmbientRgb(hsvToRgb(shift, 160, 230), 62, 45);
        break;
    }
    case OTHER_ROYAL_AURORA: {
        uint8_t banner = (uint8_t)((t / 720UL) % 3U);
        RgbColor c1 = banner == 0 ? RgbColor(255, 170, 45) : (banner == 1 ? RgbColor(120, 45, 255) : RgbColor(0, 210, 180));
        RgbColor c2 = banner == 0 ? RgbColor(120, 45, 255) : (banner == 1 ? RgbColor(0, 210, 180) : RgbColor(255, 170, 45));
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool heraldic = ((i / 3U) % 2U) == 0U;
            RgbColor c = heraldic ? c1 : c2;
            uint8_t crest = (((i + t / 90UL) % 14U) == 0U) ? 220 : 72;
            setTargetPixel(idx, secV(outerSectionIdx(idx), (uint8_t)((uint16_t)c.R * crest / 255U), (uint8_t)((uint16_t)c.G * crest / 255U), (uint8_t)((uint16_t)c.B * crest / 255U)));
        }
        setInsideAmbientRgb(RgbColor(255, 145, 60), 70, 25);
        break;
    }
    case OTHER_DATA_STREAM: {
        uint8_t tick = (uint8_t)(t / 260U);
        for (uint16_t i = 0; i < OUTER_COUNT; i++) {
            uint16_t idx = OUTER_START + i;
            bool header = ((i + tick) % 14U) < 3U;
            bool checksum = ((i * 7U + tick) % 29U) == 0U;
            uint8_t v = checksum ? 230 : (header ? 135 : 10);
            setTargetPixel(idx, secV(outerSectionIdx(idx), 0, (uint8_t)(v * 4U / 5U), v));
        }
        setInsideAmbientRgb(RgbColor(0, 180, 255), 58, 8);
        break;
    }
    }
    if (!ledAllowsInsideRgbNow()) {
        renderInsideNormalToTarget();
    }
}

// -- forward declarations needed -----------------------------------------------
// renderOtherTarget is defined above

static bool ledColorRemixAllowed(uint8_t cat, uint8_t animIdx) {
    switch (cat) {
        case 0:
            switch ((IdleAnimation)animIdx) {
                case IDLE_TEMPERATURE_IDLE:
                case IDLE_LAST_PRINT_ECHO:
                case IDLE_MATERIAL_SHELF:
                case IDLE_STATUS_RING:
                case IDLE_PRINT_READY_SPLIT:
                    return false;
                default:
                    return true;
            }
        case 1:
            switch ((PrintAnimation)animIdx) {
                case PRINT_THERMAL:
                case PRINT_THERMOMETER:
                case PRINT_EXTRUDER_SPARK:
                case PRINT_HEAT_RIPPLE:
                case PRINT_BUILD_PLATE:
                case PRINT_THERMAL_BALANCE:
                case PRINT_MATERIAL_CORE:
                case PRINT_HEAT_SOAK:
                case PRINT_STABILITY_MONITOR:
                case PRINT_CHAMBER_AURA:
                case PRINT_FILAMENT_FLOW:
                case PRINT_PROCESS_STACK:
                case PRINT_HEALTH_BEACON:
                case PRINT_FINISH_PRESSURE:
                case PRINT_DUAL_TEMP_METER:
                case PRINT_THERMAL_RIBBON:
                case PRINT_INFILL_GRID:
                case PRINT_FILAMENT_BEADS:
                case PRINT_QUALITY_GUARD:
                case PRINT_NOZZLE_HEAT_TRACE:
                case PRINT_LAYER_FILL:
                case PRINT_PROGRESS:
                case PRINT_LASER_TIP:
                case PRINT_WAVE:
                case PRINT_STRIPES:
                case PRINT_PULSE_PROG:
                case PRINT_COMET:
                case PRINT_ACTIVE_SEC:
                case PRINT_RUNNING:
                case PRINT_BREATHE_FIL:
                case PRINT_WIPE_PROG:
                case PRINT_SHIMMER:
                case PRINT_BICOLOR:
                case PRINT_SNAKE:
                case PRINT_PIXEL_RAIN:
                case PRINT_CLOCKWISE:
                case PRINT_FILAMENT_COMETS:
                case PRINT_LAYER_SCAN:
                case PRINT_PROGRESS_THEATER:
                case PRINT_NOZZLE_TRACE:
                case PRINT_MICRO_STEPS:
                case PRINT_FLOW_WAVE:
                case PRINT_TOOLHEAD_ORBIT:
                case PRINT_LAYER_PULSE:
                case PRINT_TOOLPATH_ECHO:
                case PRINT_TIME_REMAINING_FLOW:
                case PRINT_STEPPER_TICKS:
                case PRINT_CALM_BUILD:
                    return false;
                default:
                    return true;
            }
        case 2:
            switch ((PauseAnimation)animIdx) {
                case PAUSE_TEMP_KEEPALIVE:
                case PAUSE_FILAMENT_HOLD:
                case PAUSE_HEAT_HOLD_SPLIT:
                    return false;
                default:
                    return true;
            }
        case 3:
            return true;
        case 4:
            switch ((FinishAnimation)animIdx) {
                case FINISH_FILAMENT:
                case FINISH_COOLDOWN_PROGRESS:
                case FINISH_PRINT_SIGNATURE:
                case FINISH_COOL_TO_TOUCH:
                case FINISH_LAST_LAYER_GLOW:
                case FINISH_FILAMENT_FIREWORKS:
                case FINISH_PRINT_ECHO:
                case FINISH_COOLDOWN_AURA:
                    return false;
                default:
                    return true;
            }
        case 5:
            switch ((OtherAnimation)animIdx) {
                case OTHER_WEATHER_MOOD:
                case OTHER_FILAMENT_GALLERY:
                case OTHER_MAINTENANCE_MODE:
                case OTHER_CALIBRATION_RULER:
                case OTHER_HEATMAP_DEMO:
                case OTHER_STATUS_MIRROR:
                    return false;
                default:
                    return true;
            }
        default:
            return false;
    }
}


static void applyColorRemix(LedCategory category, uint8_t animation,
                            int16_t degrees) {
    if (!degrees || !ledColorRemixAllowed(static_cast<uint8_t>(category), animation)) return;
    const int16_t shift = static_cast<int16_t>(degrees * 256L / 360L);
    for (uint16_t i = OUTER_START; i <= OUTER_END; ++i) {
        uint8_t hue = 0;
        uint8_t saturation = 0;
        uint8_t value = 0;
        rgbToHsv8(targetFrame[i].R, targetFrame[i].G, targetFrame[i].B,
                  hue, saturation, value);
        if (!value || saturation < 10U) continue;
        const RgbColor shifted = hsvToRgb(static_cast<uint8_t>(hue + shift),
                                          saturation, value);
        targetFrame[i].R = shifted.R;
        targetFrame[i].G = shifted.G;
        targetFrame[i].B = shifted.B;
    }
}

static void prepareRuntime(LedCategory category, uint8_t animation,
                           const LedAnimationContext& context) {
    const uint8_t categoryIndex = static_cast<uint8_t>(category);
    const bool rendererWasAway = previousRenderMs && context.nowMs - previousRenderMs > 200U;
    if (category != previousCategory || previousAnimation[categoryIndex] != animation) {
        clearTarget();
    }
    rtPrinter.progress = context.progress;
    rtPrinter.activeTool = context.activeTool;
    rtPrinter.activeToolTempC = context.activeToolTempC;
    rtPrinter.activeToolTempUpdatedMs = context.printerTelemetryAgeMs == UINT32_MAX
        ? 0U : context.nowMs - context.printerTelemetryAgeMs;
    rtPrinter.moonrakerOnline = context.printerOnline;
    rtPrinter.filamentColor = RgbColor(
        static_cast<uint8_t>(context.filamentRgb >> 16U),
        static_cast<uint8_t>(context.filamentRgb >> 8U),
        static_cast<uint8_t>(context.filamentRgb));
    rtVent.currentTempC = context.bedTempC;
    rtVent.chamberTempC = context.chamberTempC;
    rtVent.targetTempC = context.chamberTempC;
    rtVent.failsafeActive = context.ventFailsafe;
    gSnakeFinishBurstActive = context.finishing;

    if (context.preview && (!previousPreview || rendererWasAway ||
                            category != previousCategory ||
                            previousAnimation[categoryIndex] != animation)) {
        gLedAnimPreview.startMs = context.nowMs;
    }
    gLedAnimPreview.active = context.preview;
    gLedAnimPreview.cat = static_cast<uint8_t>(category);
    gLedPreviewPrintAnimOverride = category == LedCategory::Print ? animation : -1;
    previousPreview = context.preview;
    previousRenderMs = context.nowMs;

    if (category != previousCategory && category == LedCategory::Finish) {
        rtLed.finishAnimStart = context.nowMs;
    }
    previousCategory = category;
    previousAnimation[categoryIndex] = animation;

    for (uint8_t tool = 0; tool < 4U; ++tool) {
        const uint32_t rgb = context.filamentColorsRgb[tool];
        gCachedToolColors[tool] = RgbColor(static_cast<uint8_t>(rgb >> 16U),
            static_cast<uint8_t>(rgb >> 8U), static_cast<uint8_t>(rgb));
        gCachedToolMaterials[tool] = (context.filamentColorMask & (1U << tool)) ? "FILAMENT" : "";
    }

    switch (category) {
        case LedCategory::Idle: rtLed.idleAnim = static_cast<IdleAnimation>(animation); break;
        case LedCategory::Print: rtLed.printAnim = static_cast<PrintAnimation>(animation); break;
        case LedCategory::Pause: rtLed.pauseAnim = static_cast<PauseAnimation>(animation); break;
        case LedCategory::Error: rtLed.errorAnim = static_cast<ErrorAnimation>(animation); break;
        case LedCategory::Finish: rtLed.finishAnim = static_cast<FinishAnimation>(animation); break;
        case LedCategory::Other: rtLed.otherAnim = static_cast<OtherAnimation>(animation); break;
        case LedCategory::Count: break;
    }
}

static void renderSelected(LedCategory category) {
    switch (category) {
        case LedCategory::Idle: renderIdleTarget(); break;
        case LedCategory::Print: renderPrintTarget(); break;
        case LedCategory::Pause: renderPauseTarget(); break;
        case LedCategory::Error: renderErrorTarget(); break;
        case LedCategory::Finish: renderFinishTarget(); break;
        case LedCategory::Other: renderOtherTarget(); break;
        case LedCategory::Count: clearTarget(); break;
    }
}

}  // namespace legacy

void renderLegacyLedAnimation(LedCategory category, uint8_t animation,
                              const LedAnimationContext& context,
                              int16_t colorRemixDegrees,
                              ::coronet::RgbwColor* output, size_t outputCount) {
    if (!output || outputCount < legacy::LED_COUNT) return;
    legacy::prepareRuntime(category, normalizeLedAnimation(category, animation), context);
    legacy::renderSelected(category);
    legacy::applyColorRemix(category, animation, colorRemixDegrees);
    for (uint16_t i = 0; i < legacy::LED_COUNT; ++i) {
        output[i] = ::coronet::RgbwColor(legacy::targetFrame[i].R,
                                        legacy::targetFrame[i].G,
                                        legacy::targetFrame[i].B,
                                        legacy::targetFrame[i].W);
    }
}

}  // namespace coronet
