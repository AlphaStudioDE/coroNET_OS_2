#include "PandaBreathService.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "../settings/SettingsService.h"
#include "../wifi/WifiService.h"

namespace coronet {

namespace {

PandaBreathService gPandaBreathService;
constexpr uint32_t kConnectRetryMs = 5000;
constexpr uint32_t kWorkflowIntervalMs = 500;
constexpr uint32_t kCommandRefreshMs = 15000;
constexpr uint32_t kDiscoveryQueryMs = 1200;
constexpr uint32_t kDiscoveryMessageMs = 5000;
constexpr uint32_t kTemperatureStaleMs = 10000;
constexpr float kHeatOnMarginC = 0.6f;
constexpr float kHeatOffMarginC = 0.2f;
constexpr float kPreheatReadyMarginC = 0.5f;

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
    SystemState system = stateSnapshot();
    if (system.maintenanceMode || system.otaTlsWindowActive) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        if (socketConfigured_) disconnect();
        return;
    }
    if (observedSettingsRevision_ != settingsService().revision()) configureFromSettings();

    if (discoveryRequested_) {
        discoveryRequested_ = false;
        performDiscovery();
        return;
    }

    const AppSettings& settings = settingsService().settings();
    if (!settings.pandaEnabled || !configuredHost_[0]) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        if (socketConfigured_) disconnect();
        updateState([](SystemState& current) { current.pandaConnected = false; });
        if (static_cast<int32_t>(discoveryStatusUntilMs_ - millis()) <= 0) {
            if (!settings.pandaEnabled && oneShotComplete_) {
                setPhase(PandaWorkflowPhase::Complete, "Panda cycle complete");
            } else {
                setPhase(PandaWorkflowPhase::Idle,
                         settings.pandaEnabled ? "Panda address required" : "Panda disabled");
            }
        }
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        updateState([](SystemState& current) { current.pandaConnected = false; });
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

void PandaBreathService::requestDiscovery() {
    discoveryRequested_ = true;
    updateState([](SystemState& system) {
        strlcpy(system.pandaStatusText, "Panda discovery queued", sizeof(system.pandaStatusText));
    });
}

void PandaBreathService::performDiscovery() {
    if (WiFi.status() != WL_CONNECTED) {
        updateState([](SystemState& system) {
            strlcpy(system.pandaStatusText, "Connect Wi-Fi before discovery",
                    sizeof(system.pandaStatusText));
        });
        discoveryStatusUntilMs_ = millis() + kDiscoveryMessageMs;
        return;
    }

    updateState([](SystemState& system) {
        strlcpy(system.pandaStatusText, "Searching for Panda Breath",
                sizeof(system.pandaStatusText));
    });
    if (!wifiService().acquireMdns(1500, pdMS_TO_TICKS(250))) {
        updateState([](SystemState& system) {
            strlcpy(system.pandaStatusText, "Panda discovery unavailable",
                    sizeof(system.pandaStatusText));
        });
        discoveryStatusUntilMs_ = millis() + kDiscoveryMessageMs;
        return;
    }

    const IPAddress address = MDNS.queryHost("PandaBreath", kDiscoveryQueryMs);
    wifiService().releaseMdns();
    if (!address || address == IPAddress(0, 0, 0, 0)) {
        updateState([](SystemState& system) {
            strlcpy(system.pandaStatusText, "Panda Breath not found",
                    sizeof(system.pandaStatusText));
        });
        discoveryStatusUntilMs_ = millis() + kDiscoveryMessageMs;
        return;
    }

    const String host = address.toString();
    settingsService().update([&host](AppSettings& settings) {
        strlcpy(settings.pandaHost, host.c_str(), sizeof(settings.pandaHost));
        settings.pandaEnabled = true;
    });
    configureFromSettings();
    updateState([](SystemState& system) {
        strlcpy(system.pandaStatusText, "Panda Breath found; connecting",
                sizeof(system.pandaStatusText));
    });
    discoveryStatusUntilMs_ = millis() + kDiscoveryMessageMs;
}

void PandaBreathService::disconnect() {
    if (connected_) sendOff();
    socket_.disconnect();
    connected_ = false;
    socketConfigured_ = false;
    updateState([](SystemState& system) { system.pandaConnected = false; });
}

void PandaBreathService::logStatus() const {
    const AppSettings settings = settingsService().settings();
    const SystemState system = stateSnapshot();
    Serial.printf(
        "[panda] enabled=%u host=%s socket=%u phase=%u target=%uC current=%.1fC heating=%u status=%s\n",
        settings.pandaEnabled ? 1U : 0U,
        configuredHost_[0] ? configuredHost_ : "-", connected_ ? 1U : 0U,
        static_cast<unsigned>(system.pandaPhase), static_cast<unsigned>(system.pandaTargetTempC),
        system.pandaCurrentTempC, system.pandaHeating ? 1U : 0U, system.pandaStatusText);
}

void PandaBreathService::configureFromSettings() {
    observedSettingsRevision_ = settingsService().revision();
    const AppSettings settings = settingsService().settings();
    const bool workflowChanged = observedWorkflowSettings_ &&
        (settings.pandaMode != observedMode_ ||
         (settings.pandaEnabled && !observedEnabled_));
    if (workflowChanged) {
        automaticSawPrint_ = false;
        holdStartedMs_ = 0;
        phaseStartedMs_ = 0;
        temperingStartTempC_ = NAN;
        oneShotComplete_ = false;
    }
    observedMode_ = settings.pandaMode;
    observedEnabled_ = settings.pandaEnabled;
    observedWorkflowSettings_ = true;
    char normalized[65] = "";
    normalizeHost(settings.pandaHost, normalized);
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
    updateState([](SystemState& system) {
        strlcpy(system.pandaStatusText, "Connecting to Panda Breath", sizeof(system.pandaStatusText));
    });
}

void PandaBreathService::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            connected_ = true;
            updateState([](SystemState& system) { system.pandaConnected = true; });
            commandDirty_ = true;
            Serial.printf("[panda] connected to %s\n", configuredHost_);
            break;
        case WStype_DISCONNECTED:
            connected_ = false;
            updateState([](SystemState& system) {
                system.pandaConnected = false;
                strlcpy(system.pandaStatusText, "Panda disconnected", sizeof(system.pandaStatusText));
            });
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
    JsonVariantConst root = document["settings"].is<JsonObjectConst>()
                                ? document["settings"].as<JsonVariantConst>()
                                : document.as<JsonVariantConst>();
    const bool hasCurrent = root["current_temp"].is<float>() || root["temp"].is<float>() ||
                            root["temperature"].is<float>();
    const float current = root["current_temp"].is<float>() ? root["current_temp"].as<float>() :
                          root["temp"].is<float>() ? root["temp"].as<float>() :
                          root["temperature"].as<float>();
    const bool hasTarget = root["target_temp"].is<int>();
    const uint8_t target = hasTarget ? root["target_temp"].as<uint8_t>() : 0;
    const bool hasHeating = root["work_on"].is<bool>() || root["isrunning"].is<int>();
    const bool heating = root["work_on"].is<bool>() ? root["work_on"].as<bool>() :
                         root["isrunning"].as<int>() != 0;
    updateState([&](SystemState& system) {
        if (hasCurrent) system.pandaCurrentTempC = current;
        if (hasTarget) system.pandaTargetTempC = target;
        if (hasHeating) system.pandaHeating = heating;
    });
}

void PandaBreathService::updateWorkflow(uint32_t now) {
    const AppSettings& settings = settingsService().settings();
    const SystemState system = stateSnapshot();
    const PrinterState printer = system.printerState;
    const bool printerError = printer == PrinterState::Error;
    const bool printCompleted = system.printerStateEventSequence != observedPrinterEventSequence_ &&
                                (system.printerEventFrom == PrinterState::Printing ||
                                 system.printerEventFrom == PrinterState::Paused) &&
                                system.printerEventTo == PrinterState::Complete;
    observedPrinterEventSequence_ = system.printerStateEventSequence;
    if (settings.pandaEnabled) oneShotComplete_ = false;

    if (settings.pandaMode == PandaBreathMode::Off || printerError) {
        automaticSawPrint_ = false;
        holdStartedMs_ = 0;
        requestOff();
        setPhase(printerError ? PandaWorkflowPhase::Fault : PandaWorkflowPhase::Idle,
                 printerError ? "Panda stopped: printer error" : "Panda off");
    } else if (settings.pandaMode == PandaBreathMode::FilamentDrying) {
        automaticSawPrint_ = false;
        const DryProfile profile = dryProfile();
        requestDry(profile.temperatureC, profile.hours);
        setPhase(PandaWorkflowPhase::Drying, "Filament drying");
    } else {
        if (settings.pandaMode == PandaBreathMode::Automatic &&
            (printer == PrinterState::Printing || printer == PrinterState::Paused)) {
            automaticSawPrint_ = true;
        }
        float controlTempC = NAN;
        if (!controlTemperature(now, controlTempC)) {
            requestOff();
            setPhase(PandaWorkflowPhase::Fault, "Waiting for printer chamber temperature");
            return;
        }

        if (settings.pandaMode == PandaBreathMode::ForcedOn) {
            automaticSawPrint_ = false;
            requestControlledHeat(settings.pandaTargetTempC, controlTempC,
                                  PandaWorkflowPhase::Holding, "Forced chamber hold");
        } else if (settings.pandaMode == PandaBreathMode::PreheatHold) {
            automaticSawPrint_ = false;
            if (printer == PrinterState::Printing || printer == PrinterState::Paused) {
                holdStartedMs_ = 0;
                settingsService().update([](AppSettings& current) {
                    current.pandaMode = PandaBreathMode::Automatic;
                });
                requestControlledHeat(settings.pandaPrintTargetTempC, controlTempC,
                                      PandaWorkflowPhase::PrintHold, "Print chamber hold");
                return;
            }
            if (!holdStartedMs_ &&
                controlTempC >= static_cast<float>(settings.pandaTargetTempC) - kPreheatReadyMarginC) {
                holdStartedMs_ = now;
            }
            const PandaWorkflowPhase phase = holdStartedMs_ ? PandaWorkflowPhase::Holding
                                                            : PandaWorkflowPhase::Preheating;
            requestControlledHeat(settings.pandaTargetTempC, controlTempC, phase,
                                  holdStartedMs_ ? "Preheat hold" : "Preheating chamber");
            const uint32_t holdMs = static_cast<uint32_t>(settings.pandaPreheatHoldMinutes) * 60000UL;
            if (holdStartedMs_ && now - holdStartedMs_ >= holdMs) {
                finishOneShotMode(PandaWorkflowPhase::Complete, "Preheat hold complete");
            }
        } else if (settings.pandaMode == PandaBreathMode::Automatic) {
            if (printer == PrinterState::Printing || printer == PrinterState::Paused) {
                automaticSawPrint_ = true;
                requestControlledHeat(settings.pandaPrintTargetTempC, controlTempC,
                                      PandaWorkflowPhase::PrintHold,
                                      printer == PrinterState::Paused
                                          ? "Print paused: chamber hold"
                                          : "Automatic print hold");
            } else if ((automaticSawPrint_ || printCompleted) &&
                       settings.pandaTemperingAfterPrint) {
                automaticSawPrint_ = false;
                temperingStartTempC_ = constrain(controlTempC, 0.0f, 80.0f);
                phaseStartedMs_ = now;
                requestControlledHeat(temperingTarget(now), controlTempC,
                                      PandaWorkflowPhase::Tempering, "Post-print tempering");
            } else if (system.pandaPhase == PandaWorkflowPhase::Tempering &&
                       settings.pandaTemperingAfterPrint) {
                const uint32_t durationMs =
                    static_cast<uint32_t>(settings.pandaTemperingDurationMinutes) * 60000UL;
                if (!durationMs || now - phaseStartedMs_ >= durationMs) {
                    finishOneShotMode(PandaWorkflowPhase::Complete,
                                      "Post-print tempering complete");
                } else {
                    const uint8_t target = temperingTarget(now);
                    if (target == 0U) {
                        requestOff();
                        setPhase(PandaWorkflowPhase::Tempering, "Post-print tempering to off");
                    } else {
                        requestControlledHeat(target, controlTempC,
                                              PandaWorkflowPhase::Tempering,
                                              "Post-print tempering");
                    }
                }
            } else if (automaticSawPrint_ || printCompleted) {
                automaticSawPrint_ = false;
                finishOneShotMode(PandaWorkflowPhase::Complete, "Print ended: Panda off");
            } else {
                requestOff();
                setPhase(PandaWorkflowPhase::WaitingForPrint, "Automatic: waiting for print");
            }
        } else if (settings.pandaMode == PandaBreathMode::Tempering) {
            automaticSawPrint_ = false;
            if (system.pandaPhase != PandaWorkflowPhase::Tempering &&
                system.pandaPhase != PandaWorkflowPhase::Complete) {
                temperingStartTempC_ = constrain(controlTempC, 0.0f, 80.0f);
                phaseStartedMs_ = now;
                setPhase(PandaWorkflowPhase::Tempering, "Controlled tempering");
            }
            const uint32_t durationMs =
                static_cast<uint32_t>(settings.pandaTemperingDurationMinutes) * 60000UL;
            if (!durationMs || now - phaseStartedMs_ >= durationMs) {
                finishOneShotMode(PandaWorkflowPhase::Complete, "Tempering complete");
            } else {
                const uint8_t target = temperingTarget(now);
                if (target == 0U) {
                    requestOff();
                    setPhase(PandaWorkflowPhase::Tempering, "Tempering ramp to off");
                } else {
                    requestControlledHeat(target, controlTempC,
                                          PandaWorkflowPhase::Tempering,
                                          "Controlled tempering");
                }
            }
        }
    }
}

void PandaBreathService::setPhase(PandaWorkflowPhase phase, const char* text) {
    bool phaseChanged = false;
    updateState([&](SystemState& system) {
        phaseChanged = system.pandaPhase != phase;
        system.pandaPhase = phase;
        strlcpy(system.pandaStatusText, text ? text : "", sizeof(system.pandaStatusText));
    });
    if (phaseChanged) phaseStartedMs_ = millis();
}

void PandaBreathService::requestOff() {
    if (desiredOn_ || desiredDrying_ || desiredTargetC_ != 0U) commandDirty_ = true;
    desiredOn_ = false;
    desiredDrying_ = false;
    desiredTargetC_ = 0;
    desiredHours_ = 0;
    updateState([](SystemState& system) { system.pandaTargetTempC = 0; });
}

void PandaBreathService::requestControlledHeat(uint8_t targetC, float currentTempC,
                                               PandaWorkflowPhase phase, const char* text) {
    bool wantsHeat = desiredOn_ && !desiredDrying_;
    if (currentTempC <= static_cast<float>(targetC) - kHeatOnMarginC) wantsHeat = true;
    else if (currentTempC >= static_cast<float>(targetC) + kHeatOffMarginC) wantsHeat = false;
    if (desiredOn_ != wantsHeat || desiredDrying_ || desiredTargetC_ != targetC) commandDirty_ = true;
    desiredOn_ = wantsHeat;
    desiredDrying_ = false;
    desiredTargetC_ = targetC;
    desiredHours_ = 0;
    updateState([targetC](SystemState& system) { system.pandaTargetTempC = targetC; });
    setPhase(phase, text);
}

void PandaBreathService::requestDry(uint8_t targetC, uint8_t hours) {
    if (!desiredOn_ || !desiredDrying_ || desiredTargetC_ != targetC || desiredHours_ != hours) commandDirty_ = true;
    desiredOn_ = true;
    desiredDrying_ = true;
    desiredTargetC_ = targetC;
    desiredHours_ = hours;
    updateState([targetC](SystemState& system) { system.pandaTargetTempC = targetC; });
}

void PandaBreathService::sendDesired(bool force) {
    if (!connected_) return;
    const uint32_t now = millis();
    if (!force && !commandDirty_ && now - lastCommandMs_ < kCommandRefreshMs) return;
    bool sent = false;
    if (!desiredOn_) sent = sendOff();
    else if (desiredDrying_) sent = sendDry(desiredTargetC_, desiredHours_);
    else sent = sendHeat(desiredTargetC_);
    if (!sent) return;
    commandDirty_ = false;
    lastCommandMs_ = now;
}

bool PandaBreathService::sendSettings(const char* fieldsJson) {
    if (!connected_ || !fieldsJson || !fieldsJson[0]) return false;
    String message = "{\"settings\":";
    message += fieldsJson;
    message += '}';
    return socket_.sendTXT(message);
}

bool PandaBreathService::sendOff() {
    const bool first = sendSettings("{\"isrunning\":0,\"drying_running\":false,\"target_temp\":0}");
    const bool second = sendSettings("{\"work_on\":false}");
    updateState([](SystemState& system) { system.pandaHeating = false; });
    return first || second;
}

bool PandaBreathService::sendHeat(uint8_t targetC) {
    char message[96] = "";
    bool ok = sendSettings("{\"isrunning\":0,\"drying_running\":false}");
    ok = sendSettings("{\"work_mode\":2}") && ok;
    snprintf(message, sizeof(message), "{\"set_temp\":%u,\"target_temp\":%u}", targetC, targetC);
    ok = sendSettings(message) && ok;
    ok = sendSettings("{\"work_on\":true}") && ok;
    if (ok) updateState([](SystemState& system) { system.pandaHeating = true; });
    return ok;
}

bool PandaBreathService::sendDry(uint8_t targetC, uint8_t hours) {
    char message[64] = "";
    bool ok = sendSettings("{\"work_mode\":3}");
    snprintf(message, sizeof(message), "{\"custom_temp\":%u}", targetC);
    ok = sendSettings(message) && ok;
    snprintf(message, sizeof(message), "{\"custom_timer\":%u}", hours);
    ok = sendSettings(message) && ok;
    snprintf(message, sizeof(message), "{\"filament_temp\":%u}", targetC);
    ok = sendSettings(message) && ok;
    snprintf(message, sizeof(message), "{\"filament_timer\":%u}", hours);
    ok = sendSettings(message) && ok;
    ok = sendSettings("{\"isrunning\":1,\"drying_running\":true}") && ok;
    ok = sendSettings("{\"work_on\":true}") && ok;
    if (ok) updateState([](SystemState& system) { system.pandaHeating = true; });
    return ok;
}

bool PandaBreathService::controlTemperature(uint32_t now, float& temperatureC) const {
    const SystemState system = stateSnapshot();
    if (!system.printerConnected || !system.printerTelemetryValid ||
        system.lastPrinterUpdateMs == 0U || now - system.lastPrinterUpdateMs > kTemperatureStaleMs ||
        isnan(system.chamberTempC)) {
        temperatureC = NAN;
        return false;
    }
    temperatureC = system.chamberTempC;
    return true;
}

void PandaBreathService::finishOneShotMode(PandaWorkflowPhase phase, const char* text) {
    requestOff();
    automaticSawPrint_ = false;
    holdStartedMs_ = 0;
    temperingStartTempC_ = NAN;
    oneShotComplete_ = true;
    setPhase(phase, text);
    settingsService().update([](AppSettings& settings) { settings.pandaEnabled = false; });
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
    const float start = isnan(temperingStartTempC_)
                            ? static_cast<float>(settings.pandaPrintTargetTempC)
                            : temperingStartTempC_;
    const uint8_t end = settings.pandaTemperingEndTempC;
    const uint32_t duration = static_cast<uint32_t>(settings.pandaTemperingDurationMinutes) * 60000UL;
    if (!duration || now - phaseStartedMs_ >= duration) return 0;
    const uint32_t elapsed = now - phaseStartedMs_;
    const float ratio = static_cast<float>(elapsed) / static_cast<float>(duration);
    const float target = start + (static_cast<float>(end) - start) * ratio;
    if (target <= 0.5f) return 0;
    return static_cast<uint8_t>(constrain(static_cast<int>(roundf(target)), 1, 60));
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
