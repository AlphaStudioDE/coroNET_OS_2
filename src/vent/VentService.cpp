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
    state().servoReady = servoReady_;
    state().fanReady = fanReady_;
    state().ventReady = servoReady_ && fanReady_;
    strlcpy(state().ventStatusText,
            state().ventReady ? "ready" : "hardware_init_failed",
            sizeof(state().ventStatusText));
    applyOutputs(0, 0);
    Serial.printf("[vent] ready=%u fan=%u servo=%u GPIO fan=%u servo=%u\n",
                  state().ventReady ? 1U : 0U, fanReady_ ? 1U : 0U, servoReady_ ? 1U : 0U,
                  static_cast<unsigned>(hw::FanPwmPin), static_cast<unsigned>(hw::ServoPin));
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
    applyOutputs(appliedFanPercent_, appliedFlapPercent_);

    SystemState& system = state();
    system.fanPercent = appliedFanPercent_;
    system.flapPercent = appliedFlapPercent_;
    system.ventFailsafe = failsafe;
    strlcpy(system.ventStatusText, status, sizeof(system.ventStatusText));
}

void VentService::logStatus() const {
    const AppSettings& settings = settingsService().settings();
    Serial.printf("[vent] ready=%u mode=%u target=%uC output fan=%u%% flap=%u%% failsafe=%u status=%s servo=%uus reverse=%u\n",
                  state().ventReady ? 1U : 0U,
                  static_cast<unsigned>(settings.ventMode),
                  static_cast<unsigned>(settings.ventTargetTempC),
                  static_cast<unsigned>(state().fanPercent),
                  static_cast<unsigned>(state().flapPercent),
                  state().ventFailsafe ? 1U : 0U,
                  state().ventStatusText,
                  static_cast<unsigned>(lastServoPulseUs_ == UINT16_MAX ? 0 : lastServoPulseUs_),
                  settings.servoReverse ? 1U : 0U);
}

bool VentService::beginServo() {
    mcpwm_timer_config_t timerConfig = {};
    timerConfig.group_id = 0;
    timerConfig.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timerConfig.resolution_hz = 1000000;
    timerConfig.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timerConfig.period_ticks = 20000;
    if (mcpwm_new_timer(&timerConfig, &servoTimer_) != ESP_OK) return false;

    mcpwm_operator_config_t operatorConfig = {};
    operatorConfig.group_id = 0;
    if (mcpwm_new_operator(&operatorConfig, &servoOperator_) != ESP_OK) return false;
    if (mcpwm_operator_connect_timer(servoOperator_, servoTimer_) != ESP_OK) return false;

    mcpwm_comparator_config_t comparatorConfig = {};
    comparatorConfig.flags.update_cmp_on_tez = true;
    if (mcpwm_new_comparator(servoOperator_, &comparatorConfig, &servoComparator_) != ESP_OK) return false;

    mcpwm_generator_config_t generatorConfig = {};
    generatorConfig.gen_gpio_num = hw::ServoPin;
    if (mcpwm_new_generator(servoOperator_, &generatorConfig, &servoGenerator_) != ESP_OK) return false;
    if (mcpwm_generator_set_action_on_timer_event(
            servoGenerator_, MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)) != ESP_OK) return false;
    if (mcpwm_generator_set_action_on_compare_event(
            servoGenerator_, MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, servoComparator_, MCPWM_GEN_ACTION_LOW)) != ESP_OK) return false;
    if (mcpwm_comparator_set_compare_value(servoComparator_, 1500) != ESP_OK) return false;
    if (mcpwm_timer_enable(servoTimer_) != ESP_OK) return false;
    return mcpwm_timer_start_stop(servoTimer_, MCPWM_TIMER_START_NO_STOP) == ESP_OK;
}

bool VentService::beginFan() {
    mcpwm_timer_config_t timerConfig = {};
    timerConfig.group_id = 0;
    timerConfig.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timerConfig.resolution_hz = FanResolutionHz;
    timerConfig.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timerConfig.period_ticks = FanPeriodTicks;
    if (mcpwm_new_timer(&timerConfig, &fanTimer_) != ESP_OK) return false;

    mcpwm_operator_config_t operatorConfig = {};
    operatorConfig.group_id = 0;
    if (mcpwm_new_operator(&operatorConfig, &fanOperator_) != ESP_OK) return false;
    if (mcpwm_operator_connect_timer(fanOperator_, fanTimer_) != ESP_OK) return false;

    mcpwm_comparator_config_t comparatorConfig = {};
    comparatorConfig.flags.update_cmp_on_tez = true;
    if (mcpwm_new_comparator(fanOperator_, &comparatorConfig, &fanComparator_) != ESP_OK) return false;

    mcpwm_generator_config_t generatorConfig = {};
    generatorConfig.gen_gpio_num = hw::FanPwmPin;
    if (mcpwm_new_generator(fanOperator_, &generatorConfig, &fanGenerator_) != ESP_OK) return false;
    if (mcpwm_generator_set_action_on_timer_event(
            fanGenerator_, MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)) != ESP_OK) return false;
    if (mcpwm_generator_set_action_on_compare_event(
            fanGenerator_, MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP, fanComparator_, MCPWM_GEN_ACTION_LOW)) != ESP_OK) return false;
    if (mcpwm_comparator_set_compare_value(fanComparator_, 0) != ESP_OK) return false;
    if (mcpwm_timer_enable(fanTimer_) != ESP_OK) return false;
    return mcpwm_timer_start_stop(fanTimer_, MCPWM_TIMER_START_NO_STOP) == ESP_OK;
}

void VentService::computeTargets(uint32_t now, uint8_t& targetFan, uint8_t& targetFlap,
                                 bool& failsafe, const char*& status) {
    const AppSettings& settings = settingsService().settings();
    const SystemState& system = state();
    if (system.maintenanceMode) {
        targetFan = 0;
        targetFlap = 0;
        status = "maintenance";
        return;
    }
    const bool liveTelemetry = system.printerConnected && system.printerTelemetryValid;
    const bool activePrint = liveTelemetry && printingLike(system.printerState);
    if (liveTelemetry && activePrint) printingSeen_ = true;
    if (liveTelemetry && !activePrint) printingSeen_ = false;

    if (settings.ventMode == VentMode::Manual) {
        coolingActive_ = false;
        targetFan = clampPercent(settings.manualFanPercent);
        targetFlap = clampPercent(settings.manualFlapPercent);
        status = "manual";
        return;
    }

    if (settings.ventMode == VentMode::Automatic && !activePrint) {
        coolingActive_ = false;
        if (!system.printerConnected && printingSeen_) {
            failsafe = true;
            targetFan = settings.failsafeFanPercent;
            targetFlap = settings.failsafeFlapPercent;
            status = "failsafe_print_telemetry_lost";
        } else {
            targetFan = 0;
            targetFlap = 0;
            status = liveTelemetry ? "automatic_waiting" : "automatic_printer_offline";
        }
        return;
    }

    const bool stale = !liveTelemetry || system.lastPrinterUpdateMs == 0 ||
                       now - system.lastPrinterUpdateMs > SensorStaleMs;
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

void VentService::applyOutputs(uint8_t fanPercent, uint8_t flapPercent) {
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
