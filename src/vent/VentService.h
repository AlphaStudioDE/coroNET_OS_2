#pragma once

#include <Arduino.h>
#include <driver/mcpwm_prelude.h>

namespace coronet {

class VentService {
public:
    void begin();
    void loop();
    void applyNow();
    void logStatus() const;

private:
    static constexpr uint32_t ControlIntervalMs = 200;
    static constexpr uint32_t SensorStaleMs = 10000;
    static constexpr uint32_t FanResolutionHz = 10000000;
    static constexpr uint32_t FanPeriodTicks = 400;

    bool beginServo();
    bool beginFan();
    void computeTargets(uint32_t now, uint8_t& targetFan, uint8_t& targetFlap,
                        bool& failsafe, const char*& status);
    void applyOutputs(uint8_t fanPercent, uint8_t flapPercent);
    uint16_t servoPulseForPercent(uint8_t flapPercent) const;
    static uint8_t smoothStep(uint8_t current, uint8_t target, uint8_t step);

    mcpwm_timer_handle_t servoTimer_ = nullptr;
    mcpwm_oper_handle_t servoOperator_ = nullptr;
    mcpwm_cmpr_handle_t servoComparator_ = nullptr;
    mcpwm_gen_handle_t servoGenerator_ = nullptr;
    mcpwm_timer_handle_t fanTimer_ = nullptr;
    mcpwm_oper_handle_t fanOperator_ = nullptr;
    mcpwm_cmpr_handle_t fanComparator_ = nullptr;
    mcpwm_gen_handle_t fanGenerator_ = nullptr;
    bool servoReady_ = false;
    bool fanReady_ = false;
    bool coolingActive_ = false;
    bool printingSeen_ = false;
    uint8_t appliedFanPercent_ = 0;
    uint8_t appliedFlapPercent_ = 0;
    uint16_t lastServoPulseUs_ = UINT16_MAX;
    uint16_t lastFanTicks_ = UINT16_MAX;
    uint32_t lastControlMs_ = 0;
};

VentService& ventService();

}
