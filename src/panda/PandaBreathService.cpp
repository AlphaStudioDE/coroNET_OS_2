#include "PandaBreathService.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "../settings/SettingsService.h"

namespace coronet {

namespace {

PandaBreathService gPandaBreathService;
constexpr uint32_t kConnectRetryMs = 5000;
constexpr uint32_t kWorkflowIntervalMs = 500;
constexpr uint32_t kCommandRefreshMs = 15000;

}

PandaBreathService& pandaBreathService() {
    return gPandaBreathService;
}

void PandaBreathService::begin() {
    initialized_ = true;
    socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    socket_.setReconnectInterval(kConnectRetryMs);
    socket_.enableHeartbeat(15000, 3000, 2);
    configureFromSettings();
    Serial.println("[panda] service ready; hardware connection is optional");
}

void PandaBreathService::loop() {
    if (!initialized_) return;
    if (state().maintenanceMode) {
        observedPrinterEventSequence_ = state().printerStateEventSequence;
        if (socketConfigured_) disconnect();
        return;
    }
    if (observedSettingsRevision_ != settingsService().revision()) configureFromSettings();

    const AppSettings& settings = settingsService().settings();
    if (!settings.pandaEnabled || !configuredHost_[0]) {
        observedPrinterEventSequence_ = state().printerStateEventSequence;
        if (socketConfigured_) disconnect();
        state().pandaConnected = false;
        setPhase(PandaWorkflowPhase::Idle, settings.pandaEnabled ? "Panda address required" : "Panda disabled");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        observedPrinterEventSequence_ = state().printerStateEventSequence;
        state().pandaConnected = false;
        setPhase(PandaWorkflowPhase::Idle, "Waiting for Wi-Fi");
        return;
    }

    connectIfNeeded();
    if (socketConfigured_) socket_.loop();

    const uint32_t now = millis();
    if (now - lastWorkflowMs_ >= kWorkflowIntervalMs) {
        lastWorkflowMs_ = now;
        updateWorkflow(now);
    }
    sendDesired(false);
}

void PandaBreathService::applyNow() {
    observedSettingsRevision_ = 0;
    commandDirty_ = true;
    lastWorkflowMs_ = 0;
}

void PandaBreathService::disconnect() {
    if (connected_) sendOff();
    socket_.disconnect();
    connected_ = false;
    socketConfigured_ = false;
    state().pandaConnected = false;
}

void PandaBreathService::logStatus() const {
    Serial.printf(
        "[panda] enabled=%u host=%s socket=%u phase=%u target=%uC current=%.1fC heating=%u status=%s\n",
        settingsService().settings().pandaEnabled ? 1U : 0U,
        configuredHost_[0] ? configuredHost_ : "-", connected_ ? 1U : 0U,
        static_cast<unsigned>(state().pandaPhase), static_cast<unsigned>(state().pandaTargetTempC),
        state().pandaCurrentTempC, state().pandaHeating ? 1U : 0U, state().pandaStatusText);
}

void PandaBreathService::configureFromSettings() {
    observedSettingsRevision_ = settingsService().revision();
    char normalized[65] = "";
    normalizeHost(settingsService().settings().pandaHost, normalized);
    if (strcmp(normalized, configuredHost_) != 0) {
        disconnect();
        strlcpy(configuredHost_, normalized, sizeof(configuredHost_));
    }
    commandDirty_ = true;
}

void PandaBreathService::connectIfNeeded() {
    if (socketConfigured_ || !configuredHost_[0]) return;
    const uint32_t now = millis();
    if (now - lastConnectAttemptMs_ < kConnectRetryMs) return;
    lastConnectAttemptMs_ = now;
    socket_.begin(configuredHost_, 80, "/ws");
    socketConfigured_ = true;
    strlcpy(state().pandaStatusText, "Connecting to Panda Breath", sizeof(state().pandaStatusText));
}

void PandaBreathService::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            connected_ = true;
            state().pandaConnected = true;
            commandDirty_ = true;
            Serial.printf("[panda] connected to %s\n", configuredHost_);
            break;
        case WStype_DISCONNECTED:
            connected_ = false;
            state().pandaConnected = false;
            strlcpy(state().pandaStatusText, "Panda disconnected", sizeof(state().pandaStatusText));
            break;
        case WStype_TEXT:
            handleMessage(payload, length);
            break;
        default:
            break;
    }
}

void PandaBreathService::handleMessage(const uint8_t* payload, size_t length) {
    JsonDocument document;
    if (deserializeJson(document, payload, length) != DeserializationError::Ok) return;
    JsonVariantConst root = document.as<JsonVariantConst>();
    if (root["current_temp"].is<float>()) state().pandaCurrentTempC = root["current_temp"].as<float>();
    else if (root["temp"].is<float>()) state().pandaCurrentTempC = root["temp"].as<float>();
    else if (root["temperature"].is<float>()) state().pandaCurrentTempC = root["temperature"].as<float>();

    if (root["target_temp"].is<int>()) state().pandaTargetTempC = root["target_temp"].as<uint8_t>();
    if (root["work_on"].is<bool>()) state().pandaHeating = root["work_on"].as<bool>();
    else if (root["isrunning"].is<int>()) state().pandaHeating = root["isrunning"].as<int>() != 0;
}

void PandaBreathService::updateWorkflow(uint32_t now) {
    const AppSettings& settings = settingsService().settings();
    const SystemState& system = state();
    const PrinterState printer = system.printerState;
    const bool printerError = printer == PrinterState::Error;
    const bool printCompleted = system.printerStateEventSequence != observedPrinterEventSequence_ &&
                                (system.printerEventFrom == PrinterState::Printing ||
                                 system.printerEventFrom == PrinterState::Paused) &&
                                system.printerEventTo == PrinterState::Complete;
    observedPrinterEventSequence_ = system.printerStateEventSequence;

    if (settings.pandaMode == PandaBreathMode::Off || printerError) {
        requestOff();
        setPhase(printerError ? PandaWorkflowPhase::Fault : PandaWorkflowPhase::Idle,
                 printerError ? "Panda stopped: printer error" : "Panda off");
    } else if (settings.pandaMode == PandaBreathMode::ForcedOn) {
        requestHeat(settings.pandaTargetTempC, PandaWorkflowPhase::Holding, "Forced chamber hold");
    } else if (settings.pandaMode == PandaBreathMode::FilamentDrying) {
        const DryProfile profile = dryProfile();
        requestDry(profile.temperatureC, profile.hours);
        setPhase(PandaWorkflowPhase::Drying, "Filament drying");
    } else if (settings.pandaMode == PandaBreathMode::Tempering) {
        if (state().pandaPhase != PandaWorkflowPhase::Tempering &&
            state().pandaPhase != PandaWorkflowPhase::Complete) {
            phaseStartedMs_ = now;
            setPhase(PandaWorkflowPhase::Tempering, "Controlled tempering");
        }
        const uint8_t target = temperingTarget(now);
        if (target == 0U) {
            requestOff();
            setPhase(PandaWorkflowPhase::Complete, "Tempering complete");
        } else {
            requestHeat(target, PandaWorkflowPhase::Tempering, "Controlled tempering");
        }
    } else if (settings.pandaMode == PandaBreathMode::Automatic) {
        if (printer == PrinterState::Printing || printer == PrinterState::Paused) {
            requestHeat(settings.pandaPrintTargetTempC, PandaWorkflowPhase::PrintHold,
                        printer == PrinterState::Paused ? "Print paused: chamber hold" : "Automatic print hold");
        } else if (printCompleted && settings.pandaTemperingAfterPrint) {
            phaseStartedMs_ = now;
            requestHeat(settings.pandaPrintTargetTempC, PandaWorkflowPhase::Tempering,
                        "Post-print tempering");
        } else if (state().pandaPhase == PandaWorkflowPhase::Tempering &&
                   settings.pandaTemperingAfterPrint) {
            const uint8_t target = temperingTarget(now);
            if (target) requestHeat(target, PandaWorkflowPhase::Tempering, "Post-print tempering");
            else {
                requestOff();
                setPhase(PandaWorkflowPhase::Complete, "Post-print tempering complete");
            }
        } else {
            requestOff();
            setPhase(PandaWorkflowPhase::WaitingForPrint, "Automatic: waiting for print");
        }
    } else {
        if (printer == PrinterState::Printing || printer == PrinterState::Paused) {
            requestHeat(settings.pandaPrintTargetTempC, PandaWorkflowPhase::PrintHold, "Print chamber hold");
        } else {
            requestHeat(settings.pandaTargetTempC, PandaWorkflowPhase::Preheating, "Preheating chamber");
            if (!isnan(state().pandaCurrentTempC) &&
                state().pandaCurrentTempC >= settings.pandaTargetTempC - 1.0f) {
                if (!holdStartedMs_) holdStartedMs_ = now;
                requestHeat(settings.pandaTargetTempC, PandaWorkflowPhase::Holding, "Preheat hold");
                const uint32_t holdMs = static_cast<uint32_t>(settings.pandaPreheatHoldMinutes) * 60000UL;
                if (now - holdStartedMs_ >= holdMs) setPhase(PandaWorkflowPhase::Complete, "Preheat hold complete");
            } else {
                holdStartedMs_ = 0;
            }
        }
    }
}

void PandaBreathService::setPhase(PandaWorkflowPhase phase, const char* text) {
    if (state().pandaPhase != phase) {
        state().pandaPhase = phase;
        phaseStartedMs_ = millis();
    }
    strlcpy(state().pandaStatusText, text ? text : "", sizeof(state().pandaStatusText));
}

void PandaBreathService::requestOff() {
    if (desiredOn_ || desiredDrying_ || desiredTargetC_ != 0U) commandDirty_ = true;
    desiredOn_ = false;
    desiredDrying_ = false;
    desiredTargetC_ = 0;
    desiredHours_ = 0;
    state().pandaTargetTempC = 0;
}

void PandaBreathService::requestHeat(uint8_t targetC, PandaWorkflowPhase phase, const char* text) {
    if (!desiredOn_ || desiredDrying_ || desiredTargetC_ != targetC) commandDirty_ = true;
    desiredOn_ = true;
    desiredDrying_ = false;
    desiredTargetC_ = targetC;
    desiredHours_ = 0;
    state().pandaTargetTempC = targetC;
    setPhase(phase, text);
}

void PandaBreathService::requestDry(uint8_t targetC, uint8_t hours) {
    if (!desiredOn_ || !desiredDrying_ || desiredTargetC_ != targetC || desiredHours_ != hours) commandDirty_ = true;
    desiredOn_ = true;
    desiredDrying_ = true;
    desiredTargetC_ = targetC;
    desiredHours_ = hours;
    state().pandaTargetTempC = targetC;
}

void PandaBreathService::sendDesired(bool force) {
    if (!connected_) return;
    const uint32_t now = millis();
    if (!force && !commandDirty_ && now - lastCommandMs_ < kCommandRefreshMs) return;
    if (!desiredOn_) sendOff();
    else if (desiredDrying_) sendDry(desiredTargetC_, desiredHours_);
    else sendHeat(desiredTargetC_);
    commandDirty_ = false;
    lastCommandMs_ = now;
}

void PandaBreathService::sendOff() {
    socket_.sendTXT("{\"isrunning\":0,\"drying_running\":false,\"target_temp\":0}");
    socket_.sendTXT("{\"work_on\":false}");
    state().pandaHeating = false;
}

void PandaBreathService::sendHeat(uint8_t targetC) {
    char message[96] = "";
    socket_.sendTXT("{\"isrunning\":0,\"drying_running\":false}");
    socket_.sendTXT("{\"work_mode\":2}");
    snprintf(message, sizeof(message), "{\"set_temp\":%u,\"target_temp\":%u}", targetC, targetC);
    socket_.sendTXT(message);
    socket_.sendTXT("{\"work_on\":true}");
    state().pandaHeating = true;
}

void PandaBreathService::sendDry(uint8_t targetC, uint8_t hours) {
    char message[192] = "";
    socket_.sendTXT("{\"work_mode\":3}");
    snprintf(message, sizeof(message),
             "{\"custom_temp\":%u,\"custom_timer\":%u,\"filament_temp\":%u,\"filament_timer\":%u,\"target_temp\":%u}",
             targetC, hours, targetC, hours, targetC);
    socket_.sendTXT(message);
    socket_.sendTXT("{\"isrunning\":1,\"drying_running\":true,\"work_on\":true}");
    state().pandaHeating = true;
}

PandaBreathService::DryProfile PandaBreathService::dryProfile() const {
    const AppSettings& settings = settingsService().settings();
    switch (settings.pandaDryPreset) {
        case PandaDryPreset::Petg: return {60, settings.pandaDryHours};
        case PandaDryPreset::AbsAsa: return {60, settings.pandaDryHours};
        case PandaDryPreset::Tpu: return {45, settings.pandaDryHours};
        case PandaDryPreset::NylonPa: return {60, settings.pandaDryHours};
        case PandaDryPreset::Pc: return {60, settings.pandaDryHours};
        case PandaDryPreset::Custom: return {settings.pandaTargetTempC, settings.pandaDryHours};
        case PandaDryPreset::Pla:
        default: return {55, settings.pandaDryHours};
    }
}

uint8_t PandaBreathService::temperingTarget(uint32_t now) const {
    const AppSettings& settings = settingsService().settings();
    const uint8_t start = settings.pandaPrintTargetTempC;
    const uint8_t end = settings.pandaTemperingEndTempC;
    const uint32_t duration = static_cast<uint32_t>(settings.pandaTemperingDurationMinutes) * 60000UL;
    if (!duration || now - phaseStartedMs_ >= duration) return 0;
    const uint32_t elapsed = now - phaseStartedMs_;
    return static_cast<uint8_t>(start - (static_cast<uint32_t>(start - min(start, end)) * elapsed / duration));
}

void PandaBreathService::normalizeHost(const char* input, char output[65]) {
    output[0] = '\0';
    if (!input) return;
    const char* start = input;
    if (strncmp(start, "http://", 7) == 0) start += 7;
    else if (strncmp(start, "https://", 8) == 0) start += 8;
    size_t length = strcspn(start, "/:");
    length = min(length, static_cast<size_t>(64));
    memcpy(output, start, length);
    output[length] = '\0';
}

}
