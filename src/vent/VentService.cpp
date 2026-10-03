#include "VentService.h"

#include <math.h>

#include "../config/HardwareConfig.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

VentService gVentService;

uint8_t clampPercent(uint8_t value) {
    return value > 100U ? 100U : value;
}

bool printingLike(PrinterState state) {
    return state == PrinterState::Printing || state == PrinterState::Paused;
}

}

VentService& ventService() {
    return gVentService;
}

void VentService::begin() {
    servoReady_ = beginServo();
    fanReady_ = beginFan();
    diyHeaterReady_ = beginDiyHeater();
    const bool ventReady = servoReady_ && fanReady_;
    updateState([&](SystemState& system) {
        system.servoReady = servoReady_;
        system.fanReady = fanReady_;
        system.diyHeaterReady = diyHeaterReady_;
        system.ventReady = ventReady;
        strlcpy(system.ventStatusText,
                ventReady ? "ready" : "hardware_init_failed",
                sizeof(system.ventStatusText));
    });
    applyOutputs(0, 0);
    diyHeaterArmed_ = true;
    Serial.printf("[vent] ready=%u fan=%u servo=%u heater=%u GPIO fan=%u servo=%u heater=%u\n",
                  ventReady ? 1U : 0U, fanReady_ ? 1U : 0U, servoReady_ ? 1U : 0U,
                  diyHeaterReady_ ? 1U : 0U,
                  static_cast<unsigned>(hw::FanPwmPin), static_cast<unsigned>(hw::ServoPin),
                  static_cast<unsigned>(hw::DiyChamberHeaterPin));
}

void VentService::loop() {
    const uint32_t now = millis();
    if (now - lastControlMs_ < ControlIntervalMs) return;
    lastControlMs_ = now;
    applyNow();
}

void VentService::applyNow() {
    uint8_t targetFan = 0;
    uint8_t targetFlap = 0;
    bool failsafe = false;
    const char* status = "off";
    computeTargets(millis(), targetFan, targetFlap, failsafe, status);

    appliedFanPercent_ = smoothStep(appliedFanPercent_, targetFan, 4);
    appliedFlapPercent_ = smoothStep(appliedFlapPercent_, targetFlap, 2);
    const uint8_t physicalFanPercent =
        applyOutputs(appliedFanPercent_, appliedFlapPercent_);

    updateState([&](SystemState& system) {
        system.fanPercent = physicalFanPercent;
        system.flapPercent = servoReady_ ? appliedFlapPercent_ : 0U;
        system.ventFailsafe = failsafe;
        strlcpy(system.ventStatusText, status, sizeof(system.ventStatusText));
    });
}

void VentService::logStatus() const {
    const AppSettings& settings = settingsService().settings();
    const SystemState system = stateSnapshot();
    Serial.printf("[vent] ready=%u mode=%u target=%uC output fan=%u%% flap=%u%% heater=%u failsafe=%u status=%s servo=%uus reverse=%u\n",
                  system.ventReady ? 1U : 0U,
                  static_cast<unsigned>(settings.ventMode),
                  static_cast<unsigned>(settings.ventTargetTempC),
                  static_cast<unsigned>(system.fanPercent),
                  static_cast<unsigned>(system.flapPercent),
                  system.diyHeaterHigh ? 1U : 0U,
                  system.ventFailsafe ? 1U : 0U,
                  system.ventStatusText,
                  static_cast<unsigned>(lastServoPulseUs_ == UINT16_MAX ? 0 : lastServoPulseUs_),
                  settings.servoReverse ? 1U : 0U);
}

bool VentService::beginServo() {
    releaseServo();
    mcpwm_timer_config_t timerConfig = {};
    timerConfig.group_id = 0;
    timerConfig.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timerConfig.resolution_hz = 1000000;
    timerConfig.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timerConfig.period_ticks = 20000;
    if (mcpwm_new_timer(&timerConfig, &servoTimer_) != ESP_OK) return false;

    mcpwm_operator_config_t operatorConfig = {};
    operatorConfig.group_id = 0;
    if (mcpwm_new_operator(&operatorConfig, &servoOperator_) != ESP_OK ||
        mcpwm_operator_connect_timer(servoOperator_, servoTimer_) != ESP_OK) {
        releaseServo();
        return false;
    }

    mcpwm_comparator_config_t comparatorConfig = {};
    comparatorConfig.flags.update_cmp_on_tez = true;
    if (mcpwm_new_comparator(servoOperator_, &comparatorConfig, &servoComparator_) != ESP_OK) {
        releaseServo();
        return false;
    }

    mcpwm_generator_config_t generatorConfig = {};
    generatorConfig.gen_gpio_num = hw::ServoPin;
    if (mcpwm_new_generator(servoOperator_, &generatorConfig, &servoGenerator_) != ESP_OK) {
        releaseServo();
        return false;
    }
    if (mcpwm_generator_set_action_on_timer_event(
            servoGenerator_, MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)) != ESP_OK ||
        mcpwm_generator_set_action_on_compare_event(
            servoGenerator_, MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, servoComparator_, MCPWM_GEN_ACTION_LOW)) != ESP_OK ||
        mcpwm_comparator_set_compare_value(servoComparator_, 1500) != ESP_OK ||
        mcpwm_timer_enable(servoTimer_) != ESP_OK ||
        mcpwm_timer_start_stop(servoTimer_, MCPWM_TIMER_START_NO_STOP) != ESP_OK) {
        releaseServo();
        return false;
    }
    return true;
}

bool VentService::beginFan() {
    releaseFan();
    mcpwm_timer_config_t timerConfig = {};
    timerConfig.group_id = 0;
    timerConfig.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timerConfig.resolution_hz = FanResolutionHz;
    timerConfig.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timerConfig.period_ticks = FanPeriodTicks;
    if (mcpwm_new_timer(&timerConfig, &fanTimer_) != ESP_OK) return false;

    mcpwm_operator_config_t operatorConfig = {};
    operatorConfig.group_id = 0;
    if (mcpwm_new_operator(&operatorConfig, &fanOperator_) != ESP_OK ||
        mcpwm_operator_connect_timer(fanOperator_, fanTimer_) != ESP_OK) {
        releaseFan();
        return false;
    }

    mcpwm_comparator_config_t comparatorConfig = {};
    comparatorConfig.flags.update_cmp_on_tez = true;
    if (mcpwm_new_comparator(fanOperator_, &comparatorConfig, &fanComparator_) != ESP_OK) {
        releaseFan();
        return false;
    }

    mcpwm_generator_config_t generatorConfig = {};
    generatorConfig.gen_gpio_num = hw::FanPwmPin;
    if (mcpwm_new_generator(fanOperator_, &generatorConfig, &fanGenerator_) != ESP_OK) {
        releaseFan();
        return false;
    }
    if (mcpwm_generator_set_action_on_timer_event(
            fanGenerator_, MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)) != ESP_OK ||
        mcpwm_generator_set_action_on_compare_event(
            fanGenerator_, MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, fanComparator_, MCPWM_GEN_ACTION_LOW)) != ESP_OK ||
        mcpwm_comparator_set_compare_value(fanComparator_, 0) != ESP_OK ||
        mcpwm_timer_enable(fanTimer_) != ESP_OK ||
        mcpwm_timer_start_stop(fanTimer_, MCPWM_TIMER_START_NO_STOP) != ESP_OK) {
        releaseFan();
        return false;
    }
    return true;
}

void VentService::releaseServo() {
    if (servoTimer_) {
        mcpwm_timer_start_stop(servoTimer_, MCPWM_TIMER_STOP_EMPTY);
        mcpwm_timer_disable(servoTimer_);
    }
    if (servoGenerator_) mcpwm_del_generator(servoGenerator_);
    if (servoComparator_) mcpwm_del_comparator(servoComparator_);
    if (servoOperator_) mcpwm_del_operator(servoOperator_);
    if (servoTimer_) mcpwm_del_timer(servoTimer_);
    servoGenerator_ = nullptr;
    servoComparator_ = nullptr;
    servoOperator_ = nullptr;
    servoTimer_ = nullptr;
}

void VentService::releaseFan() {
    if (fanTimer_) {
        mcpwm_timer_start_stop(fanTimer_, MCPWM_TIMER_STOP_EMPTY);
        mcpwm_timer_disable(fanTimer_);
    }
    if (fanGenerator_) mcpwm_del_generator(fanGenerator_);
    if (fanComparator_) mcpwm_del_comparator(fanComparator_);
    if (fanOperator_) mcpwm_del_operator(fanOperator_);
    if (fanTimer_) mcpwm_del_timer(fanTimer_);
    fanGenerator_ = nullptr;
    fanComparator_ = nullptr;
    fanOperator_ = nullptr;
    fanTimer_ = nullptr;
}

bool VentService::beginDiyHeater() {
    pinMode(hw::DiyChamberHeaterPin, OUTPUT);
    digitalWrite(hw::DiyChamberHeaterPin, LOW);
    diyHeaterHigh_ = false;
    updateState([](SystemState& system) { system.diyHeaterHigh = false; });
    return true;
}

void VentService::computeTargets(uint32_t now, uint8_t& targetFan, uint8_t& targetFlap,
                                 bool& failsafe, const char*& status) {
    const AppSettings& settings = settingsService().settings();
    const SystemState system = stateSnapshot();
    if (system.maintenanceMode) {
        targetFan = 0;
        targetFlap = 0;
        status = "maintenance";
        return;
    }
    const bool liveTelemetry = system.printerConnected && system.printerTelemetryValid;
    const bool telemetryFresh = liveTelemetry && system.lastPrinterUpdateMs != 0U &&
                                now - system.lastPrinterUpdateMs <= SensorStaleMs;
    const bool printerStateKnown = system.printerState != PrinterState::Unknown;
    const bool activePrint = telemetryFresh && printingLike(system.printerState);
    if (activePrint) printingSeen_ = true;
    if (telemetryFresh && printerStateKnown && !activePrint) printingSeen_ = false;

    if (settings.ventMode == VentMode::Manual) {
        coolingActive_ = false;
        targetFan = clampPercent(settings.manualFanPercent);
        targetFlap = clampPercent(settings.manualFlapPercent);
        status = "manual";
        return;
    }

    if (settings.ventMode == VentMode::Automatic && !activePrint) {
        coolingActive_ = false;
        if ((!telemetryFresh || !printerStateKnown) && printingSeen_) {
            failsafe = true;
            targetFan = settings.failsafeFanPercent;
            targetFlap = settings.failsafeFlapPercent;
            status = telemetryFresh ? "failsafe_printer_state_unknown"
                                    : "failsafe_print_telemetry_lost";
        } else {
            targetFan = 0;
            targetFlap = 0;
            status = telemetryFresh ? "automatic_waiting" : "automatic_printer_offline";
        }
        return;
    }

    const bool stale = !telemetryFresh;
    if (stale || isnan(system.chamberTempC)) {
        if (activePrint || printingSeen_ || settings.ventMode == VentMode::CavityTarget) {
            failsafe = true;
            targetFan = settings.failsafeFanPercent;
            targetFlap = settings.failsafeFlapPercent;
            status = stale ? "failsafe_telemetry_stale" : "failsafe_no_chamber_temp";
        } else {
            targetFan = 0;
            targetFlap = 0;
            status = "waiting_for_printer_temp";
        }
        return;
    }

    const float delta = system.chamberTempC - static_cast<float>(settings.ventTargetTempC);
    constexpr float DeadbandC = 0.5f;
    if (delta <= 0.0f) {
        coolingActive_ = false;
        targetFan = 0;
        targetFlap = 0;
        status = "target_satisfied";
        return;
    }
    if (delta > DeadbandC) coolingActive_ = true;
    if (!coolingActive_) {
        targetFan = 0;
        targetFlap = 0;
        status = "target_deadband";
        return;
    }

    float normalized = (delta - DeadbandC) / 3.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    targetFlap = static_cast<uint8_t>(normalized * 100.0f + 0.5f);
    if (targetFlap > 0 && targetFlap < 8) targetFlap = 8;
    targetFan = normalized <= 0.01f
        ? 0
        : static_cast<uint8_t>(settings.fanMinPercent +
            normalized * (settings.fanMaxPercent - settings.fanMinPercent) + 0.5f);
    status = "cooling";
}

uint8_t VentService::applyOutputs(uint8_t fanPercent, uint8_t flapPercent) {
    fanPercent = clampPercent(fanPercent);
    flapPercent = clampPercent(flapPercent);
    const AppSettings& settings = settingsService().settings();

    const uint16_t pulseUs = servoPulseForPercent(flapPercent);
    const bool servoChanged = lastServoPulseUs_ == UINT16_MAX ||
        abs(static_cast<int>(pulseUs) - static_cast<int>(lastServoPulseUs_)) >= 30;
    if (servoReady_ && servoChanged) {
        if (mcpwm_comparator_set_compare_value(servoComparator_, pulseUs) == ESP_OK) {
            lastServoPulseUs_ = pulseUs;
        }
    }

    uint8_t effectiveFan = fanPercent;
    if (effectiveFan > 0 && effectiveFan < settings.fanMinPercent) effectiveFan = settings.fanMinPercent;
    if (effectiveFan > settings.fanMaxPercent) effectiveFan = settings.fanMaxPercent;
    const uint16_t ticks = static_cast<uint16_t>(effectiveFan * FanPeriodTicks / 100U);
    const bool fanChanged = lastFanTicks_ == UINT16_MAX ||
        abs(static_cast<int>(ticks) - static_cast<int>(lastFanTicks_)) >= 12 ||
        (ticks == 0 && lastFanTicks_ != 0) || (ticks != 0 && lastFanTicks_ == 0);
    if (fanReady_ && fanChanged) {
        if (mcpwm_comparator_set_compare_value(fanComparator_, ticks) == ESP_OK) {
            lastFanTicks_ = ticks;
        }
    }

    const bool maintenanceMode = stateSnapshot().maintenanceMode;
    const bool heaterHigh = diyHeaterArmed_ && settings.diyHeaterOutputHigh && !maintenanceMode;
    if (diyHeaterReady_ && heaterHigh != diyHeaterHigh_) {
        digitalWrite(hw::DiyChamberHeaterPin, heaterHigh ? HIGH : LOW);
        diyHeaterHigh_ = heaterHigh;
    }
    const bool outputHigh = diyHeaterReady_ && diyHeaterHigh_;
    updateState([outputHigh](SystemState& system) { system.diyHeaterHigh = outputHigh; });
    return fanReady_ ? effectiveFan : 0U;
}

uint16_t VentService::servoPulseForPercent(uint8_t flapPercent) const {
    const AppSettings& settings = settingsService().settings();
    uint8_t logical = clampPercent(flapPercent);
    if (settings.servoReverse) logical = 100U - logical;
    int32_t pulse = static_cast<int32_t>(settings.servoClosedUs) +
        (static_cast<int32_t>(settings.servoOpenUs) - static_cast<int32_t>(settings.servoClosedUs)) * logical / 100L;
    if (pulse < hw::ServoPulseMinUs) pulse = hw::ServoPulseMinUs;
    if (pulse > hw::ServoPulseMaxUs) pulse = hw::ServoPulseMaxUs;
    return static_cast<uint16_t>(pulse);
}

uint8_t VentService::smoothStep(uint8_t current, uint8_t target, uint8_t step) {
    if (current < target) return static_cast<uint8_t>(min<uint16_t>(target, current + step));
    if (current > target) return static_cast<uint8_t>(max<int16_t>(target, current - step));
    return current;
}

}
