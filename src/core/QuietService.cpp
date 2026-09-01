#include "QuietService.h"

#include "SystemState.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {
QuietService gQuietService;
}

QuietService& quietService() {
    return gQuietService;
}

void QuietService::begin() {
    observedTarget_ = settingsService().settings().quietTarget;
    activeSinceMs_ = millis();
    state().quietActive = observedTarget_ != QuietTarget::Off;
}

void QuietService::loop() {
    AppSettings& settings = settingsService().mutableSettings();
    if (settings.quietTarget != observedTarget_) {
        observedTarget_ = settings.quietTarget;
        activeSinceMs_ = millis();
        state().quietActive = observedTarget_ != QuietTarget::Off;
        Serial.printf("[quiet] %s, duration=%u min\n",
                      state().quietActive ? "active" : "off",
                      static_cast<unsigned>(settings.quietDurationMinutes));
    }
    if (!state().quietActive) return;
    const uint32_t durationMs = static_cast<uint32_t>(settings.quietDurationMinutes) * 60000UL;
    if (millis() - activeSinceMs_ < durationMs) return;
    settings.quietTarget = QuietTarget::Off;
    observedTarget_ = QuietTarget::Off;
    state().quietActive = false;
    settingsService().save();
    Serial.println("[quiet] duration elapsed");
}

}
