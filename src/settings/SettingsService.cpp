#include "SettingsService.h"

#include <Preferences.h>
#include <esp_system.h>

#include "../config/AppConfig.h"
#include "../led/LedAnimations.h"

namespace coronet {

namespace {
constexpr const char* Namespace = "coronet2";
constexpr uint16_t CurrentSchema = 7;

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
}

static SettingsService gSettingsService;

SettingsService& settingsService() {
    return gSettingsService;
}

void SettingsService::begin() {
    load();
    const bool tokenWasMissing = settings_.apiToken[0] == '\0';
    ensureApiToken();
    if (tokenWasMissing) saveNow();
}

void SettingsService::loop() {
    if (!savePending_) return;

    const uint32_t now = millis();
    const bool debounceElapsed = now - lastChangeMs_ >= config::SettingsSaveDebounceMs;
    const bool maxDelayElapsed = now - dirtySinceMs_ >= config::SettingsSaveMaxDelayMs;
    if (debounceElapsed || maxDelayElapsed) saveNow();
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
    readExactBytes(prefs, "ledBr", settings_.ledBrightness, sizeof(settings_.ledBrightness));
    readExactBytes(prefs, "ledDimEn", settings_.ledDimmEnabled, sizeof(settings_.ledDimmEnabled));
    readExactBytes(prefs, "ledDimPct", settings_.ledDimmPercent, sizeof(settings_.ledDimmPercent));
    settings_.insideColorStyle = static_cast<InsideColorStyle>(
        prefs.getUChar("inStyle", static_cast<uint8_t>(settings_.insideColorStyle)));
    settings_.mirrorLedLayout = prefs.getBool("ledMirror", settings_.mirrorLedLayout);
    readExactBytes(prefs, "ledAnim", settings_.ledAnimation, sizeof(settings_.ledAnimation));
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
    settings_.clockBrightness = clampValue<uint8_t>(settings_.clockBrightness, 1, 100);
    if (static_cast<uint8_t>(settings_.clockStyle) >= static_cast<uint8_t>(ClockStyle::Count)) {
        settings_.clockStyle = ClockStyle::Digital;
    }
    if (!settings_.timeZone[0]) {
        strlcpy(settings_.timeZone, "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(settings_.timeZone));
    }
    if (static_cast<uint8_t>(settings_.quietTarget) > static_cast<uint8_t>(QuietTarget::SoundAndLeds)) {
        settings_.quietTarget = QuietTarget::Off;
    }
    settings_.quietDurationMinutes = clampValue<uint16_t>(settings_.quietDurationMinutes, 1, 1440);
    if (needsMigrationSave) {
        settings_.schemaVersion = CurrentSchema;
        ensureApiToken();
        saveNow();
    }
    loaded_ = true;
}

void SettingsService::save() {
    const uint32_t now = millis();
    if (!savePending_) dirtySinceMs_ = now;
    lastChangeMs_ = now;
    savePending_ = true;
    revision_++;
}

AppSettings SettingsService::snapshot() const {
    AppSettings copy;
    portENTER_CRITICAL(&settingsMux_);
    copy = settings_;
    portEXIT_CRITICAL(&settingsMux_);
    return copy;
}

void SettingsService::replace(const AppSettings& settings) {
    portENTER_CRITICAL(&settingsMux_);
    settings_ = settings;
    portEXIT_CRITICAL(&settingsMux_);
}

void SettingsService::flush() {
    if (savePending_) saveNow();
}

void SettingsService::saveNow() {
    Preferences prefs;
    if (!prefs.begin(Namespace, false)) return;
    prefs.putUShort("schema", CurrentSchema);
    prefs.putBool("setupDone", settings_.setupDone);
    prefs.putBool("ble", settings_.bleEnabled);
    prefs.putUChar("brightness", settings_.displayBrightness);
    prefs.putUChar("uiSkin", static_cast<uint8_t>(settings_.uiSkin));
    prefs.putUChar("uiColor", static_cast<uint8_t>(settings_.uiColorMode));
    prefs.putUChar("transport", static_cast<uint8_t>(settings_.companionTransport));
    prefs.putString("deviceName", settings_.deviceName);
    prefs.putString("ssid", settings_.wifiSsid);
    prefs.putString("wifiPass", settings_.wifiPassword);
    prefs.putString("printerHost", settings_.printerHost);
    prefs.putUShort("printerPort", sanePort(settings_.printerPort));
    prefs.putString("printerKey", settings_.printerApiKey);
    prefs.putString("apiToken", settings_.apiToken);
    prefs.putBool("apiPaired", settings_.apiPaired);

    prefs.putBool("ledEn", settings_.ledEnabled);
    prefs.putBool("ledOther", settings_.ledOtherMode);
    prefs.putBytes("ledBr", settings_.ledBrightness, sizeof(settings_.ledBrightness));
    prefs.putBytes("ledDimEn", settings_.ledDimmEnabled, sizeof(settings_.ledDimmEnabled));
    prefs.putBytes("ledDimPct", settings_.ledDimmPercent, sizeof(settings_.ledDimmPercent));
    prefs.putUChar("inStyle", static_cast<uint8_t>(settings_.insideColorStyle));
    prefs.putBool("ledMirror", settings_.mirrorLedLayout);
    prefs.putBytes("ledAnim", settings_.ledAnimation, sizeof(settings_.ledAnimation));
    prefs.putBytes("ledRemix", settings_.ledColorRemixDegrees, sizeof(settings_.ledColorRemixDegrees));
    prefs.putBytes("ledCalHue", settings_.ledCalibrationHue, sizeof(settings_.ledCalibrationHue));
    prefs.putBytes("ledCalSat", settings_.ledCalibrationSaturation,
                   sizeof(settings_.ledCalibrationSaturation));
    prefs.putBytes("ledCalVal", settings_.ledCalibrationBrightness,
                   sizeof(settings_.ledCalibrationBrightness));

    prefs.putBytes("sndVol", settings_.soundVolume, sizeof(settings_.soundVolume));
    prefs.putBytes("sndRepeat", settings_.soundRepeat, sizeof(settings_.soundRepeat));
    for (uint8_t index = 0; index < enumCount(SoundScenario{}); ++index) {
        char key[8] = "";
        snprintf(key, sizeof(key), "snd%u", static_cast<unsigned>(index));
        prefs.putString(key, settings_.soundPath[index]);
    }

    prefs.putUChar("ventMode", static_cast<uint8_t>(settings_.ventMode));
    prefs.putUChar("ventTarget", settings_.ventTargetTempC);
    prefs.putUChar("manFan", settings_.manualFanPercent);
    prefs.putUChar("manFlap", settings_.manualFlapPercent);
    prefs.putUChar("fanMin", settings_.fanMinPercent);
    prefs.putUChar("fanMax", settings_.fanMaxPercent);
    prefs.putUChar("failFan", settings_.failsafeFanPercent);
    prefs.putUChar("failFlap", settings_.failsafeFlapPercent);
    prefs.putUShort("srvClosed", settings_.servoClosedUs);
    prefs.putUShort("srvOpen", settings_.servoOpenUs);
    prefs.putBool("srvRev", settings_.servoReverse);
    prefs.putBool("diyHeatHi", settings_.diyHeaterOutputHigh);

    prefs.putString("pandaHost", settings_.pandaHost);
    prefs.putBool("pandaEn", settings_.pandaEnabled);
    prefs.putUChar("pandaMode", static_cast<uint8_t>(settings_.pandaMode));
    prefs.putUChar("pandaTgt", settings_.pandaTargetTempC);
    prefs.putUChar("pandaPrint", settings_.pandaPrintTargetTempC);
    prefs.putUChar("pandaPreset", static_cast<uint8_t>(settings_.pandaDryPreset));
    prefs.putUChar("pandaHours", settings_.pandaDryHours);
    prefs.putUChar("pandaHold", settings_.pandaPreheatHoldMinutes);
    prefs.putUChar("pandaTempMin", settings_.pandaTemperingDurationMinutes);
    prefs.putUChar("pandaTempEnd", settings_.pandaTemperingEndTempC);
    prefs.putBool("pandaTempAft", settings_.pandaTemperingAfterPrint);

    prefs.putUShort("accentHue", settings_.accentHueDegrees);
    prefs.putUChar("saverMode", static_cast<uint8_t>(settings_.screenSaverMode));
    prefs.putUChar("saverDelay", settings_.screenSaverDelayMinutes);
    prefs.putUChar("clockBright", settings_.clockBrightness);
    prefs.putUChar("clockStyle", static_cast<uint8_t>(settings_.clockStyle));
    prefs.putBool("clock24", settings_.clock24Hour);
    prefs.putString("timeZone", settings_.timeZone);
    prefs.putUChar("quietTarget", static_cast<uint8_t>(settings_.quietTarget));
    prefs.putUShort("quietMin", settings_.quietDurationMinutes);
    prefs.putBool("quietErr", settings_.quietErrorsBypass);
    prefs.end();
    savePending_ = false;
}

void SettingsService::resetToDefaults() {
    settings_ = AppSettings{};
    settings_.schemaVersion = CurrentSchema;
    ensureApiToken();
    loaded_ = true;
    save();
}

void SettingsService::resetApiPairing() {
    settings_.apiPaired = false;
    settings_.apiToken[0] = '\0';
    ensureApiToken();
    save();
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
