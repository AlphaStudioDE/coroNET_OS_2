#include "PairingService.h"

#include <Arduino.h>
#include <esp_system.h>

#include "../settings/SettingsService.h"

namespace coronet {

namespace {

PairingService gPairingService;

bool isTerminal(PairingPhase phase) {
    return phase == PairingPhase::Completed || phase == PairingPhase::Cancelled ||
           phase == PairingPhase::Expired;
}

}

PairingService& pairingService() {
    return gPairingService;
}

void PairingService::loop() {
    const uint32_t now = millis();
    portENTER_CRITICAL(&mux_);
    if (state_.phase != PairingPhase::Inactive && !isTerminal(state_.phase) &&
        static_cast<int32_t>(now - state_.expiresAtMs) >= 0) {
        state_.phase = PairingPhase::Expired;
        state_.revision++;
    }
    portEXIT_CRITICAL(&mux_);
}

PairingSnapshot PairingService::beginPairing() {
    settingsService().resetApiPairing();

    PairingSnapshot next;
    next.phase = PairingPhase::WaitingForConfirmations;
    do {
        next.sessionId = esp_random();
    } while (next.sessionId == 0);
    next.code = 100000U + (esp_random() % 900000U);
    next.expiresAtMs = millis() + SessionDurationMs;

    portENTER_CRITICAL(&mux_);
    next.revision = state_.revision + 1U;
    state_ = next;
    portEXIT_CRITICAL(&mux_);
    return next;
}

bool PairingService::confirmFromPhone(uint32_t sessionId, uint32_t code) {
    bool accepted = false;
    portENTER_CRITICAL(&mux_);
    if (sessionMatchesLocked(sessionId) && code == state_.code) {
        state_.phoneConfirmed = true;
        state_.revision++;
        updateReadyStateLocked();
        accepted = true;
    }
    portEXIT_CRITICAL(&mux_);
    return accepted;
}

bool PairingService::confirmOnDevice(uint32_t sessionId) {
    bool accepted = false;
    portENTER_CRITICAL(&mux_);
    if (sessionMatchesLocked(sessionId)) {
        state_.deviceConfirmed = true;
        state_.revision++;
        updateReadyStateLocked();
        accepted = true;
    }
    portEXIT_CRITICAL(&mux_);
    return accepted;
}

bool PairingService::markResultSent(uint32_t sessionId) {
    bool accepted = false;
    portENTER_CRITICAL(&mux_);
    if (state_.sessionId == sessionId &&
        (state_.phase == PairingPhase::ReadyToDeliver || state_.phase == PairingPhase::AwaitingReceipt)) {
        if (state_.phase != PairingPhase::AwaitingReceipt) {
            state_.phase = PairingPhase::AwaitingReceipt;
            state_.revision++;
        }
        accepted = true;
    }
    portEXIT_CRITICAL(&mux_);
    return accepted;
}

bool PairingService::completeFromPhone(uint32_t sessionId) {
    bool accepted = false;
    portENTER_CRITICAL(&mux_);
    if (state_.sessionId == sessionId && state_.phoneConfirmed && state_.deviceConfirmed &&
        (state_.phase == PairingPhase::ReadyToDeliver || state_.phase == PairingPhase::AwaitingReceipt)) {
        state_.phase = PairingPhase::Completed;
        state_.revision++;
        accepted = true;
    }
    portEXIT_CRITICAL(&mux_);

    if (accepted) {
        AppSettings& settings = settingsService().mutableSettings();
        settings.apiPaired = true;
        settingsService().save();
        settingsService().flush();
    }
    return accepted;
}

bool PairingService::cancel(uint32_t sessionId) {
    bool accepted = false;
    portENTER_CRITICAL(&mux_);
    if (state_.phase != PairingPhase::Inactive && !isTerminal(state_.phase) &&
        (sessionId == 0 || state_.sessionId == sessionId)) {
        state_.phase = PairingPhase::Cancelled;
        state_.revision++;
        accepted = true;
    }
    portEXIT_CRITICAL(&mux_);
    return accepted;
}

void PairingService::dismiss() {
    portENTER_CRITICAL(&mux_);
    if (isTerminal(state_.phase)) {
        const uint32_t revision = state_.revision + 1U;
        state_ = PairingSnapshot{};
        state_.revision = revision;
    }
    portEXIT_CRITICAL(&mux_);
}

PairingSnapshot PairingService::snapshot() const {
    PairingSnapshot copy;
    portENTER_CRITICAL(&mux_);
    copy = state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

bool PairingService::active() const {
    const PairingSnapshot current = snapshot();
    return current.phase != PairingPhase::Inactive && !isTerminal(current.phase);
}

void PairingService::updateReadyStateLocked() {
    if (state_.phoneConfirmed && state_.deviceConfirmed) {
        state_.phase = PairingPhase::ReadyToDeliver;
    }
}

bool PairingService::sessionMatchesLocked(uint32_t sessionId) const {
    return sessionId != 0 && state_.sessionId == sessionId &&
           state_.phase == PairingPhase::WaitingForConfirmations;
}

}
