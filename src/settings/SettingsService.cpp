#include "SettingsService.h"

#include <Preferences.h>
#include <esp_system.h>
#include <nvs.h>

#include "../config/AppConfig.h"
#include "../led/LedAnimations.h"

namespace coronet {

namespace {
constexpr const char* Namespace = "coronet2";
constexpr uint16_t CurrentSchema = 9;

uint16_t sanePort(uint16_t port) {
    return port == 0 ? 7125 : port;
}

template <typename T>
T clampValue(T value, T minimum, T maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void readExactBytes(Preferences& prefs, const char* key, void* target, size_t size) {
    if (prefs.getBytesLength(key) == size) prefs.getBytes(key, target, size);
}

bool usesLegacyLedCalibrationDefaults(const AppSettings& settings) {
    for (uint8_t index = 0; index < 8U; ++index) {
        if (settings.ledCalibrationHue[index] != 0 ||
            settings.ledCalibrationSaturation[index] != 100U ||
            settings.ledCalibrationBrightness[index] != 100U) {
            return false;
        }
    }
    return true;
}
}

static SettingsService gSettingsService;

SettingsService& settingsService() {
    return gSettingsService;
}

void SettingsService::begin() {
    saveMutex_ = xSemaphoreCreateMutex();
    if (!saveMutex_) Serial.println("[settings] save mutex allocation failed");
    load();
    const bool tokenWasMissing = settings_.apiToken[0] == '\0';
    ensureApiToken();
    if (tokenWasMissing) saveNow();
}

void SettingsService::loop() {
    const uint32_t now = millis();
    bool saveDue = false;
    portENTER_CRITICAL(&settingsMux_);
    if (savePending_) {
        const bool debounceElapsed = now - lastChangeMs_ >= config::SettingsSaveDebounceMs;
        const bool maxDelayElapsed = now - dirtySinceMs_ >= config::SettingsSaveMaxDelayMs;
        saveDue = debounceElapsed || maxDelayElapsed;
    }
    portEXIT_CRITICAL(&settingsMux_);
    if (saveDue) saveNow();
}

void SettingsService::load() {
    Preferences prefs;
    if (!prefs.begin(Namespace, true)) {
        resetToDefaults();
        return;
    }

    settings_.schemaVersion = prefs.getUShort("schema", CurrentSchema);
    settings_.setupDone = prefs.getBool("setupDone", false);
    settings_.bleEnabled = prefs.getBool("ble", true);
    settings_.displayBrightness = prefs.getUChar("brightness", 80);
    settings_.uiSkin = static_cast<UiSkin>(prefs.getUChar("uiSkin", static_cast<uint8_t>(UiSkin::Coronet)));
    settings_.uiColorMode = static_cast<UiColorMode>(prefs.getUChar("uiColor", static_cast<uint8_t>(UiColorMode::Dark)));
    settings_.companionTransport = static_cast<CompanionTransport>(prefs.getUChar("transport", static_cast<uint8_t>(CompanionTransport::Auto)));
    String deviceName = prefs.getString("deviceName", "");
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("wifiPass", "");
    String printerHost = prefs.getString("printerHost", "");
    settings_.printerPort = prefs.getUShort("printerPort", 7125);
    String printerApiKey = prefs.getString("printerKey", "");
    String apiToken = prefs.getString("apiToken", "");
    settings_.apiPaired = prefs.getBool("apiPaired", false);

    settings_.ledEnabled = prefs.getBool("ledEn", settings_.ledEnabled);
    settings_.ledOtherMode = prefs.getBool("ledOther", settings_.ledOtherMode);
    settings_.ledLegacyAnimations = prefs.getBool("ledLegacy", false);
    readExactBytes(prefs, "ledBr", settings_.ledBrightness, sizeof(settings_.ledBrightness));
    readExactBytes(prefs, "ledDimEn", settings_.ledDimmEnabled, sizeof(settings_.ledDimmEnabled));
    readExactBytes(prefs, "ledDimPct", settings_.ledDimmPercent, sizeof(settings_.ledDimmPercent));
    settings_.insideColorStyle = static_cast<InsideColorStyle>(
        prefs.getUChar("inStyle", static_cast<uint8_t>(settings_.insideColorStyle)));
    settings_.mirrorLedLayout = prefs.getBool("ledMirror", settings_.mirrorLedLayout);
    readExactBytes(prefs, "ledAnim", settings_.ledAnimation, sizeof(settings_.ledAnimation));
    // NEW and LEGACY are two renderers for the same catalog selection.
    // Keep the old field mirrored for compatibility with 0.4.6 clients.
    memcpy(settings_.ledLegacyAnimation, settings_.ledAnimation,
           sizeof(settings_.ledLegacyAnimation));
    readExactBytes(prefs, "ledRemix", settings_.ledColorRemixDegrees,
                   sizeof(settings_.ledColorRemixDegrees));
    readExactBytes(prefs, "ledCalHue", settings_.ledCalibrationHue,
                   sizeof(settings_.ledCalibrationHue));
    readExactBytes(prefs, "ledCalSat", settings_.ledCalibrationSaturation,
                   sizeof(settings_.ledCalibrationSaturation));
    readExactBytes(prefs, "ledCalVal", settings_.ledCalibrationBrightness,
                   sizeof(settings_.ledCalibrationBrightness));

    readExactBytes(prefs, "sndVol", settings_.soundVolume, sizeof(settings_.soundVolume));
    readExactBytes(prefs, "sndRepeat", settings_.soundRepeat, sizeof(settings_.soundRepeat));
    for (uint8_t index = 0; index < enumCount(SoundScenario{}); ++index) {
        char key[8] = "";
        snprintf(key, sizeof(key), "snd%u", static_cast<unsigned>(index));
        const String path = prefs.getString(key, "");
        path.toCharArray(settings_.soundPath[index], sizeof(settings_.soundPath[index]));
    }

    settings_.ventMode = static_cast<VentMode>(
        prefs.getUChar("ventMode", static_cast<uint8_t>(settings_.ventMode)));
    settings_.ventTargetTempC = prefs.getUChar("ventTarget", settings_.ventTargetTempC);
    settings_.manualFanPercent = prefs.getUChar("manFan", settings_.manualFanPercent);
    settings_.manualFlapPercent = prefs.getUChar("manFlap", settings_.manualFlapPercent);
    settings_.fanMinPercent = prefs.getUChar("fanMin", settings_.fanMinPercent);
    settings_.fanMaxPercent = prefs.getUChar("fanMax", settings_.fanMaxPercent);
    settings_.failsafeFanPercent = prefs.getUChar("failFan", settings_.failsafeFanPercent);
    settings_.failsafeFlapPercent = prefs.getUChar("failFlap", settings_.failsafeFlapPercent);
    settings_.servoClosedUs = prefs.getUShort("srvClosed", settings_.servoClosedUs);
    settings_.servoOpenUs = prefs.getUShort("srvOpen", settings_.servoOpenUs);
    settings_.servoReverse = prefs.getBool("srvRev", settings_.servoReverse);
    settings_.diyHeaterOutputHigh = prefs.getBool("diyHeatHi", settings_.diyHeaterOutputHigh);

    String pandaHost = prefs.getString("pandaHost", "");
    pandaHost.toCharArray(settings_.pandaHost, sizeof(settings_.pandaHost));
    settings_.pandaEnabled = prefs.getBool("pandaEn", settings_.pandaEnabled);
    settings_.pandaMode = static_cast<PandaBreathMode>(
        prefs.getUChar("pandaMode", static_cast<uint8_t>(settings_.pandaMode)));
    settings_.pandaTargetTempC = prefs.getUChar("pandaTgt", settings_.pandaTargetTempC);
    settings_.pandaPrintTargetTempC = prefs.getUChar("pandaPrint", settings_.pandaPrintTargetTempC);
    settings_.pandaDryPreset = static_cast<PandaDryPreset>(
        prefs.getUChar("pandaPreset", static_cast<uint8_t>(settings_.pandaDryPreset)));
    settings_.pandaDryHours = prefs.getUChar("pandaHours", settings_.pandaDryHours);
    settings_.pandaPreheatHoldMinutes = prefs.getUChar("pandaHold", settings_.pandaPreheatHoldMinutes);
    settings_.pandaTemperingDurationMinutes = prefs.getUChar("pandaTempMin", settings_.pandaTemperingDurationMinutes);
    settings_.pandaTemperingEndTempC = prefs.getUChar("pandaTempEnd", settings_.pandaTemperingEndTempC);
    settings_.pandaTemperingAfterPrint = prefs.getBool("pandaTempAft", settings_.pandaTemperingAfterPrint);

    settings_.accentHueDegrees = prefs.getUShort("accentHue", settings_.accentHueDegrees);
    settings_.screenSaverMode = static_cast<ScreenSaverMode>(
        prefs.getUChar("saverMode", static_cast<uint8_t>(settings_.screenSaverMode)));
    settings_.screenSaverDelayMinutes = prefs.getUChar("saverDelay", settings_.screenSaverDelayMinutes);
    settings_.clockBrightness = prefs.getUChar("clockBright", settings_.clockBrightness);
    settings_.clockStyle = static_cast<ClockStyle>(
        prefs.getUChar("clockStyle", static_cast<uint8_t>(settings_.clockStyle)));
    settings_.clock24Hour = prefs.getBool("clock24", settings_.clock24Hour);
    String timeZone = prefs.getString("timeZone", settings_.timeZone);
    timeZone.toCharArray(settings_.timeZone, sizeof(settings_.timeZone));
    settings_.quietTarget = static_cast<QuietTarget>(
        prefs.getUChar("quietTarget", static_cast<uint8_t>(settings_.quietTarget)));
    settings_.quietDurationMinutes = prefs.getUShort("quietMin", settings_.quietDurationMinutes);
    settings_.quietErrorsBypass = prefs.getBool("quietErr", settings_.quietErrorsBypass);
    prefs.end();

    const bool needsMigrationSave = settings_.schemaVersion < CurrentSchema;
    if (settings_.schemaVersion > CurrentSchema) {
        resetToDefaults();
        ensureApiToken();
        saveNow();
        return;
    }

    if (settings_.schemaVersion < 9U && usesLegacyLedCalibrationDefaults(settings_)) {
        for (uint8_t index = 0; index < 8U; ++index) {
            settings_.ledCalibrationHue[index] = DefaultLedCalibrationHue[index];
            settings_.ledCalibrationSaturation[index] = DefaultLedCalibrationSaturation;
            settings_.ledCalibrationBrightness[index] = DefaultLedCalibrationBrightness;
        }
    }

    deviceName.toCharArray(settings_.deviceName, sizeof(settings_.deviceName));
    ssid.toCharArray(settings_.wifiSsid, sizeof(settings_.wifiSsid));
    pass.toCharArray(settings_.wifiPassword, sizeof(settings_.wifiPassword));
    printerHost.toCharArray(settings_.printerHost, sizeof(settings_.printerHost));
    printerApiKey.toCharArray(settings_.printerApiKey, sizeof(settings_.printerApiKey));
    apiToken.toCharArray(settings_.apiToken, sizeof(settings_.apiToken));
    settings_.displayBrightness = clampValue<uint8_t>(settings_.displayBrightness, 10, 100);
    settings_.printerPort = sanePort(settings_.printerPort);
    if (static_cast<uint8_t>(settings_.uiSkin) > static_cast<uint8_t>(UiSkin::Minimal)) {
        settings_.uiSkin = UiSkin::Coronet;
    }
    if (static_cast<uint8_t>(settings_.uiColorMode) > static_cast<uint8_t>(UiColorMode::Auto)) {
        settings_.uiColorMode = UiColorMode::Dark;
    }
    if (static_cast<uint8_t>(settings_.companionTransport) > static_cast<uint8_t>(CompanionTransport::Wifi)) {
        settings_.companionTransport = CompanionTransport::Auto;
    }

    for (uint8_t index = 0; index < enumCount(LedSection{}); ++index) {
        settings_.ledBrightness[index] = clampValue<uint8_t>(settings_.ledBrightness[index], 0, 100);
        settings_.ledDimmPercent[index] = clampValue<uint8_t>(settings_.ledDimmPercent[index], 0, 100);
    }
    if (static_cast<uint8_t>(settings_.insideColorStyle) > static_cast<uint8_t>(InsideColorStyle::Ambient)) {
        settings_.insideColorStyle = InsideColorStyle::White;
    }
    for (uint8_t index = 0; index < enumCount(LedCategory{}); ++index) {
        const LedCategory category = static_cast<LedCategory>(index);
        settings_.ledAnimation[index] = normalizeLedAnimation(category, settings_.ledAnimation[index]);
        settings_.ledLegacyAnimation[index] = settings_.ledAnimation[index];
        int16_t remix = settings_.ledColorRemixDegrees[index] % 360;
        if (remix > 180) remix -= 360;
        if (remix < -180) remix += 360;
        settings_.ledColorRemixDegrees[index] = remix;
    }
    for (uint8_t index = 0; index < 8U; ++index) {
        settings_.ledCalibrationHue[index] = clampValue<int8_t>(
            settings_.ledCalibrationHue[index], -45, 45);
        settings_.ledCalibrationSaturation[index] = clampValue<uint8_t>(
            settings_.ledCalibrationSaturation[index], 50, 150);
        settings_.ledCalibrationBrightness[index] = clampValue<uint8_t>(
            settings_.ledCalibrationBrightness[index], 50, 150);
    }
    for (uint8_t index = 0; index < enumCount(SoundScenario{}); ++index) {
        settings_.soundVolume[index] = clampValue<uint8_t>(settings_.soundVolume[index], 0, 100);
        settings_.soundPath[index][sizeof(settings_.soundPath[index]) - 1] = '\0';
    }

    if (static_cast<uint8_t>(settings_.ventMode) > static_cast<uint8_t>(VentMode::Manual)) {
        settings_.ventMode = VentMode::Automatic;
    }
    settings_.ventTargetTempC = clampValue<uint8_t>(settings_.ventTargetTempC, 20, 80);
    settings_.manualFanPercent = clampValue<uint8_t>(settings_.manualFanPercent, 0, 100);
    settings_.manualFlapPercent = clampValue<uint8_t>(settings_.manualFlapPercent, 0, 100);
    settings_.fanMinPercent = clampValue<uint8_t>(settings_.fanMinPercent, 0, 100);
    settings_.fanMaxPercent = clampValue<uint8_t>(settings_.fanMaxPercent, settings_.fanMinPercent, 100);
    settings_.failsafeFanPercent = clampValue<uint8_t>(settings_.failsafeFanPercent, 0, 100);
    settings_.failsafeFlapPercent = clampValue<uint8_t>(settings_.failsafeFlapPercent, 0, 100);
    settings_.servoClosedUs = clampValue<uint16_t>(settings_.servoClosedUs, 500, 2500);
    settings_.servoOpenUs = clampValue<uint16_t>(settings_.servoOpenUs, 500, 2500);

    settings_.pandaHost[sizeof(settings_.pandaHost) - 1] = '\0';
    if (static_cast<uint8_t>(settings_.pandaMode) >= static_cast<uint8_t>(PandaBreathMode::Count)) {
        settings_.pandaMode = PandaBreathMode::Off;
    }
    if (static_cast<uint8_t>(settings_.pandaDryPreset) >= static_cast<uint8_t>(PandaDryPreset::Count)) {
        settings_.pandaDryPreset = PandaDryPreset::Pla;
    }
    settings_.pandaTargetTempC = clampValue<uint8_t>(settings_.pandaTargetTempC, 30, 60);
    settings_.pandaPrintTargetTempC = clampValue<uint8_t>(settings_.pandaPrintTargetTempC, 30, 60);
    settings_.pandaDryHours = clampValue<uint8_t>(settings_.pandaDryHours, 1, 24);
    settings_.pandaPreheatHoldMinutes = clampValue<uint8_t>(settings_.pandaPreheatHoldMinutes, 1, 180);
    settings_.pandaTemperingDurationMinutes = clampValue<uint8_t>(settings_.pandaTemperingDurationMinutes, 1, 180);
    if (settings_.pandaTemperingEndTempC > 60) settings_.pandaTemperingEndTempC = 0;

    settings_.accentHueDegrees %= 360;
    if (static_cast<uint8_t>(settings_.screenSaverMode) > static_cast<uint8_t>(ScreenSaverMode::Clock)) {
        settings_.screenSaverMode = ScreenSaverMode::Clock;
    }
    settings_.screenSaverDelayMinutes = clampValue<uint8_t>(settings_.screenSaverDelayMinutes, 1, 60);
    settings_.clockBrightness = clampValue<uint8_t>(settings_.clockBrightness, 5, 100);
    if (static_cast<uint8_t>(settings_.clockStyle) >= static_cast<uint8_t>(ClockStyle::Count)) {
        settings_.clockStyle = ClockStyle::Digital;
    }
    if (!settings_.timeZone[0]) {
        strlcpy(settings_.timeZone, "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(settings_.timeZone));
    }
    if (static_cast<uint8_t>(settings_.quietTarget) > static_cast<uint8_t>(QuietTarget::SoundAndLeds)) {
        settings_.quietTarget = QuietTarget::Off;
    }
    if (settings_.quietDurationMinutes > 1440U) settings_.quietDurationMinutes = 1440U;
    if (needsMigrationSave) {
        settings_.schemaVersion = CurrentSchema;
        ensureApiToken();
        saveNow();
    }
    loaded_ = true;
}

void SettingsService::save() {
    portENTER_CRITICAL(&settingsMux_);
    memcpy(settings_.ledLegacyAnimation, settings_.ledAnimation,
           sizeof(settings_.ledLegacyAnimation));
    const uint32_t now = millis();
    if (!savePending_) dirtySinceMs_ = now;
    lastChangeMs_ = now;
    savePending_ = true;
    revision_++;
    portEXIT_CRITICAL(&settingsMux_);
}

AppSettings SettingsService::snapshot(uint32_t* revision) const {
    AppSettings copy;
    portENTER_CRITICAL(&settingsMux_);
    copy = settings_;
    if (revision) *revision = revision_;
    portEXIT_CRITICAL(&settingsMux_);
    return copy;
}

uint32_t SettingsService::revision() const {
    uint32_t revision = 0;
    portENTER_CRITICAL(&settingsMux_);
    revision = revision_;
    portEXIT_CRITICAL(&settingsMux_);
    return revision;
}

void SettingsService::replace(const AppSettings& settings) {
    portENTER_CRITICAL(&settingsMux_);
    settings_ = settings;
    memcpy(settings_.ledLegacyAnimation, settings_.ledAnimation,
           sizeof(settings_.ledLegacyAnimation));
    const uint32_t now = millis();
    if (!savePending_) dirtySinceMs_ = now;
    lastChangeMs_ = now;
    savePending_ = true;
    revision_++;
    portEXIT_CRITICAL(&settingsMux_);
}

bool SettingsService::replaceIfRevision(const AppSettings& settings,
                                        uint32_t expectedRevision,
                                        uint32_t* appliedRevision) {
    bool replaced = false;
    portENTER_CRITICAL(&settingsMux_);
    if (revision_ == expectedRevision) {
        settings_ = settings;
        memcpy(settings_.ledLegacyAnimation, settings_.ledAnimation,
               sizeof(settings_.ledLegacyAnimation));
        const uint32_t now = millis();
        if (!savePending_) dirtySinceMs_ = now;
        lastChangeMs_ = now;
        savePending_ = true;
        revision_++;
        if (appliedRevision) *appliedRevision = revision_;
        replaced = true;
    }
    portEXIT_CRITICAL(&settingsMux_);
    return replaced;
}

void SettingsService::flush() {
    bool pending = false;
    portENTER_CRITICAL(&settingsMux_);
    pending = savePending_;
    portEXIT_CRITICAL(&settingsMux_);
    if (pending) saveNow();
}

void SettingsService::saveNow() {
    if (saveMutex_ && xSemaphoreTake(saveMutex_, portMAX_DELAY) != pdTRUE) return;

    AppSettings stored;
    uint32_t storedRevision = 0;
    portENTER_CRITICAL(&settingsMux_);
    stored = settings_;
    memcpy(stored.ledLegacyAnimation, stored.ledAnimation,
           sizeof(stored.ledLegacyAnimation));
    storedRevision = revision_;
    portEXIT_CRITICAL(&settingsMux_);

    // Preferences commits after every put call. coroNET 1 avoided the resulting
    // multi-second flash stall by writing all keys through NVS and committing once.
    nvs_handle_t handle = 0;
    esp_err_t writeError = nvs_open(Namespace, NVS_READWRITE, &handle);
    if (writeError == ESP_OK) {
        auto write = [&writeError](esp_err_t result) {
            if (writeError == ESP_OK && result != ESP_OK) writeError = result;
        };

        write(nvs_set_u16(handle, "schema", CurrentSchema));
        write(nvs_set_u8(handle, "setupDone", stored.setupDone ? 1U : 0U));
        write(nvs_set_u8(handle, "ble", stored.bleEnabled ? 1U : 0U));
        write(nvs_set_u8(handle, "brightness", stored.displayBrightness));
        write(nvs_set_u8(handle, "uiSkin", static_cast<uint8_t>(stored.uiSkin)));
        write(nvs_set_u8(handle, "uiColor", static_cast<uint8_t>(stored.uiColorMode)));
        write(nvs_set_u8(handle, "transport", static_cast<uint8_t>(stored.companionTransport)));
        write(nvs_set_str(handle, "deviceName", stored.deviceName));
        write(nvs_set_str(handle, "ssid", stored.wifiSsid));
        write(nvs_set_str(handle, "wifiPass", stored.wifiPassword));
        write(nvs_set_str(handle, "printerHost", stored.printerHost));
        write(nvs_set_u16(handle, "printerPort", sanePort(stored.printerPort)));
        write(nvs_set_str(handle, "printerKey", stored.printerApiKey));
        write(nvs_set_str(handle, "apiToken", stored.apiToken));
        write(nvs_set_u8(handle, "apiPaired", stored.apiPaired ? 1U : 0U));

        write(nvs_set_u8(handle, "ledEn", stored.ledEnabled ? 1U : 0U));
        write(nvs_set_u8(handle, "ledOther", stored.ledOtherMode ? 1U : 0U));
        write(nvs_set_u8(handle, "ledLegacy", stored.ledLegacyAnimations ? 1U : 0U));
        write(nvs_set_blob(handle, "ledBr", stored.ledBrightness, sizeof(stored.ledBrightness)));
        write(nvs_set_blob(handle, "ledDimEn", stored.ledDimmEnabled, sizeof(stored.ledDimmEnabled)));
        write(nvs_set_blob(handle, "ledDimPct", stored.ledDimmPercent, sizeof(stored.ledDimmPercent)));
        write(nvs_set_u8(handle, "inStyle", static_cast<uint8_t>(stored.insideColorStyle)));
        write(nvs_set_u8(handle, "ledMirror", stored.mirrorLedLayout ? 1U : 0U));
        write(nvs_set_blob(handle, "ledAnim", stored.ledAnimation, sizeof(stored.ledAnimation)));
        write(nvs_set_blob(handle, "ledLegAnim", stored.ledLegacyAnimation,
                           sizeof(stored.ledLegacyAnimation)));
        write(nvs_set_blob(handle, "ledRemix", stored.ledColorRemixDegrees,
                           sizeof(stored.ledColorRemixDegrees)));
        write(nvs_set_blob(handle, "ledCalHue", stored.ledCalibrationHue,
                           sizeof(stored.ledCalibrationHue)));
        write(nvs_set_blob(handle, "ledCalSat", stored.ledCalibrationSaturation,
                           sizeof(stored.ledCalibrationSaturation)));
        write(nvs_set_blob(handle, "ledCalVal", stored.ledCalibrationBrightness,
                           sizeof(stored.ledCalibrationBrightness)));

        write(nvs_set_blob(handle, "sndVol", stored.soundVolume, sizeof(stored.soundVolume)));
        write(nvs_set_blob(handle, "sndRepeat", stored.soundRepeat, sizeof(stored.soundRepeat)));
        for (uint8_t index = 0; index < enumCount(SoundScenario{}); ++index) {
            char key[8] = "";
            snprintf(key, sizeof(key), "snd%u", static_cast<unsigned>(index));
            write(nvs_set_str(handle, key, stored.soundPath[index]));
        }

        write(nvs_set_u8(handle, "ventMode", static_cast<uint8_t>(stored.ventMode)));
        write(nvs_set_u8(handle, "ventTarget", stored.ventTargetTempC));
        write(nvs_set_u8(handle, "manFan", stored.manualFanPercent));
        write(nvs_set_u8(handle, "manFlap", stored.manualFlapPercent));
        write(nvs_set_u8(handle, "fanMin", stored.fanMinPercent));
        write(nvs_set_u8(handle, "fanMax", stored.fanMaxPercent));
        write(nvs_set_u8(handle, "failFan", stored.failsafeFanPercent));
        write(nvs_set_u8(handle, "failFlap", stored.failsafeFlapPercent));
        write(nvs_set_u16(handle, "srvClosed", stored.servoClosedUs));
        write(nvs_set_u16(handle, "srvOpen", stored.servoOpenUs));
        write(nvs_set_u8(handle, "srvRev", stored.servoReverse ? 1U : 0U));
        write(nvs_set_u8(handle, "diyHeatHi", stored.diyHeaterOutputHigh ? 1U : 0U));

        write(nvs_set_str(handle, "pandaHost", stored.pandaHost));
        write(nvs_set_u8(handle, "pandaEn", stored.pandaEnabled ? 1U : 0U));
        write(nvs_set_u8(handle, "pandaMode", static_cast<uint8_t>(stored.pandaMode)));
        write(nvs_set_u8(handle, "pandaTgt", stored.pandaTargetTempC));
        write(nvs_set_u8(handle, "pandaPrint", stored.pandaPrintTargetTempC));
        write(nvs_set_u8(handle, "pandaPreset", static_cast<uint8_t>(stored.pandaDryPreset)));
        write(nvs_set_u8(handle, "pandaHours", stored.pandaDryHours));
        write(nvs_set_u8(handle, "pandaHold", stored.pandaPreheatHoldMinutes));
        write(nvs_set_u8(handle, "pandaTempMin", stored.pandaTemperingDurationMinutes));
        write(nvs_set_u8(handle, "pandaTempEnd", stored.pandaTemperingEndTempC));
        write(nvs_set_u8(handle, "pandaTempAft", stored.pandaTemperingAfterPrint ? 1U : 0U));

        write(nvs_set_u16(handle, "accentHue", stored.accentHueDegrees));
        write(nvs_set_u8(handle, "saverMode", static_cast<uint8_t>(stored.screenSaverMode)));
        write(nvs_set_u8(handle, "saverDelay", stored.screenSaverDelayMinutes));
        write(nvs_set_u8(handle, "clockBright", stored.clockBrightness));
        write(nvs_set_u8(handle, "clockStyle", static_cast<uint8_t>(stored.clockStyle)));
        write(nvs_set_u8(handle, "clock24", stored.clock24Hour ? 1U : 0U));
        write(nvs_set_str(handle, "timeZone", stored.timeZone));
        write(nvs_set_u8(handle, "quietTarget", static_cast<uint8_t>(stored.quietTarget)));
        write(nvs_set_u16(handle, "quietMin", stored.quietDurationMinutes));
        write(nvs_set_u8(handle, "quietErr", stored.quietErrorsBypass ? 1U : 0U));

        if (writeError == ESP_OK) writeError = nvs_commit(handle);
        nvs_close(handle);
    }

    portENTER_CRITICAL(&settingsMux_);
    if (writeError == ESP_OK && revision_ == storedRevision) {
        savePending_ = false;
    } else if (writeError != ESP_OK) {
        dirtySinceMs_ = millis();
        lastChangeMs_ = dirtySinceMs_;
    }
    portEXIT_CRITICAL(&settingsMux_);

    if (writeError != ESP_OK) {
        Serial.printf("[settings] NVS save failed: %s\n", esp_err_to_name(writeError));
    }
    if (saveMutex_) xSemaphoreGive(saveMutex_);
}

void SettingsService::resetToDefaults() {
    AppSettings defaults;
    defaults.schemaVersion = CurrentSchema;
    static constexpr char Hex[] = "0123456789abcdef";
    for (size_t offset = 0; offset < 32; offset += 8) {
        const uint32_t randomValue = esp_random();
        for (size_t nibble = 0; nibble < 8; ++nibble) {
            const uint8_t shift = static_cast<uint8_t>((7 - nibble) * 4);
            defaults.apiToken[offset + nibble] = Hex[(randomValue >> shift) & 0x0F];
        }
    }
    defaults.apiToken[32] = '\0';
    replace(defaults);
    loaded_ = true;
}

void SettingsService::resetApiPairing() {
    update([](AppSettings& settings) {
        settings.apiPaired = false;
        static constexpr char Hex[] = "0123456789abcdef";
        for (size_t offset = 0; offset < 32; offset += 8) {
            const uint32_t randomValue = esp_random();
            for (size_t nibble = 0; nibble < 8; ++nibble) {
                const uint8_t shift = static_cast<uint8_t>((7 - nibble) * 4);
                settings.apiToken[offset + nibble] = Hex[(randomValue >> shift) & 0x0F];
            }
        }
        settings.apiToken[32] = '\0';
    });
    flush();
}

void SettingsService::ensureApiToken() {
    if (settings_.apiToken[0]) return;

    static constexpr char Hex[] = "0123456789abcdef";
    for (size_t offset = 0; offset < 32; offset += 8) {
        const uint32_t randomValue = esp_random();
        for (size_t nibble = 0; nibble < 8; ++nibble) {
            const uint8_t shift = static_cast<uint8_t>((7 - nibble) * 4);
            settings_.apiToken[offset + nibble] = Hex[(randomValue >> shift) & 0x0F];
        }
    }
    settings_.apiToken[32] = '\0';
}

}
