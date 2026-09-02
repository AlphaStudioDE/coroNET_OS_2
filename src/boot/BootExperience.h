#pragma once

#include <Arduino.h>

namespace coronet {

enum class BootExperienceMode : uint8_t {
    Full,
    Quick,
};

class BootExperience {
public:
    static constexpr uint32_t FullDurationMs = 35000;
    static constexpr uint32_t QuickDurationMs = 3500;

    void begin(bool setupDone);
    void systemReady();
    void loop();

    bool active() const { return active_; }
    bool full() const { return mode_ == BootExperienceMode::Full; }
    bool performanceStarted() const { return performanceStarted_; }
    bool protectsFirstImpression() const {
        return active_ && full() && systemReady_;
    }
    uint32_t timelineMs() const;
    uint32_t preludeMs() const;

private:
    void startFullPerformance();
    void complete();

    BootExperienceMode mode_ = BootExperienceMode::Quick;
    volatile bool active_ = false;
    volatile bool systemReady_ = false;
    volatile bool performanceStarted_ = false;
    bool audioRequested_ = false;
    bool audioExpected_ = false;
    uint32_t beginMs_ = 0;
    uint32_t audioRequestMs_ = 0;
    volatile uint32_t performanceStartMs_ = 0;
};

BootExperience& bootExperience();

}
