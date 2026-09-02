#include "BootExperience.h"

#include "../audio/AudioService.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

BootExperience gBootExperience;
constexpr uint32_t AudioStartGraceMs = 1200;

uint8_t bootVolumePercent() {
    const AppSettings& settings = settingsService().settings();
    uint16_t sum = 0;
    for (uint8_t i = 0; i < enumCount(SoundScenario{}); ++i) {
        sum += settings.soundVolume[i];
    }
    return static_cast<uint8_t>(
        (sum + enumCount(SoundScenario{}) / 2U) / enumCount(SoundScenario{}));
}

}

BootExperience& bootExperience() {
    return gBootExperience;
}

void BootExperience::begin(bool setupDone) {
    mode_ = setupDone ? BootExperienceMode::Quick : BootExperienceMode::Full;
    active_ = true;
    systemReady_ = false;
    performanceStarted_ = false;
    audioRequested_ = false;
    audioExpected_ = false;
    beginMs_ = millis();
    audioRequestMs_ = 0;
    performanceStartMs_ = 0;
    Serial.printf("[boot-experience] mode=%s\n", full() ? "full" : "quick");
}

void BootExperience::systemReady() {
    if (!active_) return;
    systemReady_ = true;
    if (full()) {
        startFullPerformance();
    } else {
        performanceStartMs_ = millis();
        performanceStarted_ = true;
    }
}

void BootExperience::startFullPerformance() {
    if (audioRequested_ || performanceStarted_) return;
    audioRequested_ = true;
    audioRequestMs_ = millis();
    audioExpected_ = audioService().playFile(
        "/boot.wav", bootVolumePercent(), false, SoundScenario::Start, true);
    if (!audioExpected_) {
        performanceStartMs_ = audioRequestMs_;
        performanceStarted_ = true;
        Serial.println("[boot-experience] boot.wav unavailable; using visual timeline");
    }
}

void BootExperience::loop() {
    if (!active_) return;
    const uint32_t now = millis();

    if (!systemReady_) return;
    if (!full()) {
        if (now - performanceStartMs_ >= QuickDurationMs) complete();
        return;
    }

    if (!performanceStarted_) {
        if (audioService().bootAudioActive() &&
            audioService().playbackStartedMs() >= audioRequestMs_) {
            performanceStartMs_ = audioService().playbackStartedMs();
            performanceStarted_ = true;
            Serial.println("[boot-experience] full timeline locked to audio playback");
        } else if (now - audioRequestMs_ >= AudioStartGraceMs) {
            performanceStartMs_ = audioRequestMs_;
            performanceStarted_ = true;
            Serial.println("[boot-experience] audio start timeout; visual timeline released");
        }
        return;
    }

    if (now - performanceStartMs_ >= FullDurationMs && !audioService().bootAudioActive()) {
        complete();
    }
}

uint32_t BootExperience::timelineMs() const {
    if (!active_ || !performanceStarted_) return 0;
    return millis() - performanceStartMs_;
}

uint32_t BootExperience::preludeMs() const {
    if (!active_) return 0;
    return millis() - beginMs_;
}

void BootExperience::complete() {
    if (!active_) return;
    active_ = false;
    Serial.printf("[boot-experience] %s experience complete\n", full() ? "full" : "quick");
}

}
