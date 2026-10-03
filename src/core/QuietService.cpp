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
    const bool active = observedTarget_ != QuietTarget::Off;
    updateState([active](SystemState& system) { system.quietActive = active; });
}

void QuietService::loop() {
    const AppSettings settings = settingsService().snapshot();
    if (settings.quietTarget != observedTarget_) {
        observedTarget_ = settings.quietTarget;
        activeSinceMs_ = millis();
        const bool active = observedTarget_ != QuietTarget::Off;
        updateState([active](SystemState& system) { system.quietActive = active; });
        Serial.printf("[quiet] %s, duration=%u min\n",
                      active ? "active" : "off",
                      static_cast<unsigned>(settings.quietDurationMinutes));
    }
    if (!stateSnapshot().quietActive) return;
    const uint32_t durationMs = static_cast<uint32_t>(settings.quietDurationMinutes) * 60000UL;
    if (millis() - activeSinceMs_ < durationMs) return;
    settingsService().update([](AppSettings& current) {
        current.quietTarget = QuietTarget::Off;
    });
    observedTarget_ = QuietTarget::Off;
    updateState([](SystemState& system) { system.quietActive = false; });
    Serial.println("[quiet] duration elapsed");
}

}
