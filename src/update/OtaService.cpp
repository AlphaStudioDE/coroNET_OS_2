#include "OtaService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <SD_MMC.h>
#include <Update.h>
#include <esp_ota_ops.h>

#include "../audio/AudioService.h"
#include "../config/AppConfig.h"
#include "../core/SystemState.h"
#include "../panda/PandaBreathService.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {
OtaService gOtaService;
constexpr uint32_t kTaskStackBytes = 10240;
constexpr uint32_t kMinimumImageBytes = 128U * 1024U;

const char* otaStateName(OtaState value) {
    switch (value) {
        case OtaState::Checking: return "checking";
        case OtaState::UpdateAvailable: return "available";
        case OtaState::UpToDate: return "up_to_date";
        case OtaState::Preparing: return "preparing";
        case OtaState::Downloading: return "downloading";
        case OtaState::Installing: return "installing";
        case OtaState::Success: return "success";
        case OtaState::Failed: return "failed";
        case OtaState::Idle:
        default: return "idle";
    }
}

bool versionMatches(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a == 'v' || *a == 'V') ++a;
    while (*b == 'v' || *b == 'V') ++b;
    return strcmp(a, b) == 0;
}
}

OtaService& otaService() {
    return gOtaService;
}

void OtaService::begin() {
    stableSinceMs_ = millis();
    setState(OtaState::Idle, "Ready");
}

void OtaService::loop() {
    if (!appMarkedValid_ && millis() - stableSinceMs_ >= 30000U) {
        const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
        appMarkedValid_ = true;
        if (result == ESP_OK) Serial.println("[ota] running firmware marked valid");
    }
}

bool OtaService::requestCheck() { return startRequest(Request::Check); }
bool OtaService::requestInstall(bool allowSameVersion) {
    return startRequest(allowSameVersion ? Request::Reinstall : Request::Install);
}
bool OtaService::requestSdRecovery() { return startRequest(Request::SdRecovery); }

bool OtaService::startRequest(Request request) {
    portENTER_CRITICAL(&mux_);
    if (task_) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    pendingRequest_ = request;
    const BaseType_t result = xTaskCreatePinnedToCore(taskEntry, "coronet-ota",
                                                      kTaskStackBytes, this, 6, &task_, 0);
    if (result != pdPASS) task_ = nullptr;
    portEXIT_CRITICAL(&mux_);
    if (result != pdPASS) setState(OtaState::Failed, "Unable to start update task");
    return result == pdPASS;
}

void OtaService::taskEntry(void* context) {
    OtaService* service = static_cast<OtaService*>(context);
    Request request;
    portENTER_CRITICAL(&service->mux_);
    request = service->pendingRequest_;
    portEXIT_CRITICAL(&service->mux_);
    service->taskLoop(request);
    portENTER_CRITICAL(&service->mux_);
    service->task_ = nullptr;
    service->pendingRequest_ = Request::None;
    portEXIT_CRITICAL(&service->mux_);
    vTaskDelete(nullptr);
}

void OtaService::taskLoop(Request request) {
    bool ok = false;
    if (request == Request::Check) {
        ok = checkLatestRelease();
    } else if (request == Request::SdRecovery) {
        ok = installFromSd();
    } else {
        if (!state().wifiConnected) {
            setState(OtaState::Failed, "Wi-Fi connection required");
            return;
        }
        const bool checked = checkLatestRelease();
        const bool reinstall = request == Request::Reinstall;
        if (!checked) return;
        if (!state().otaUpdateAvailable && !reinstall) {
            setState(OtaState::UpToDate, "Firmware is up to date", 100);
            return;
        }
        ok = installFromUrl(downloadUrl_);
    }
    if (ok && (request == Request::Install || request == Request::Reinstall || request == Request::SdRecovery)) {
        setState(OtaState::Success, "Update complete; restarting", 100);
        delay(800);
        ESP.restart();
    }
}

bool OtaService::checkLatestRelease() {
    if (!state().wifiConnected) {
        setState(OtaState::Failed, "Wi-Fi connection required");
        return false;
    }
    setState(OtaState::Checking, "Checking GitHub releases");
    NetworkClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    if (!http.begin(client, config::GitHubLatestReleaseApi)) {
        setState(OtaState::Failed, "Could not open update service");
        return false;
    }
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "coroNET-OS-2");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code == HTTP_CODE_NOT_FOUND) {
            http.end();
            setState(OtaState::UpToDate, "No published release found");
            return true;
        }
        char message[64];
        snprintf(message, sizeof(message), "GitHub check failed (HTTP %d)", code);
        http.end();
        setState(OtaState::Failed, message);
        return false;
    }
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
        http.end();
        setState(OtaState::Failed, "Invalid release metadata");
        return false;
    }
    const char* tag = doc["tag_name"] | "";
    const char* selectedUrl = "";
    for (JsonObjectConst asset : doc["assets"].as<JsonArrayConst>()) {
        const char* name = asset["name"] | "";
        if (strcmp(name, "coronet_os2.bin") == 0 || strcmp(name, "firmware.bin") == 0 ||
            strcmp(name, "coroNET_OS_2.bin") == 0) {
            selectedUrl = asset["browser_download_url"] | "";
            break;
        }
    }
    if (!tag[0] || !selectedUrl[0]) {
        http.end();
        setState(OtaState::Failed, "Release has no compatible firmware asset");
        return false;
    }
    strlcpy(state().otaAvailableVersion, tag, sizeof(state().otaAvailableVersion));
    strlcpy(downloadUrl_, selectedUrl, sizeof(downloadUrl_));
    state().otaUpdateAvailable = !versionMatches(tag, config::FirmwareVersion);
    http.end();
    if (state().otaUpdateAvailable) setState(OtaState::UpdateAvailable, "Update available", 100);
    else setState(OtaState::UpToDate, "Firmware is up to date", 100);
    return true;
}

bool OtaService::validateImageHeader(Stream& stream, size_t expectedSize) {
    if (expectedSize && expectedSize < kMinimumImageBytes) return false;
    const int first = stream.peek();
    return first == 0xE9;
}

bool OtaService::installFromUrl(const char* url) {
    if (!url || !url[0]) {
        setState(OtaState::Failed, "No firmware download URL");
        return false;
    }
    setState(OtaState::Preparing, "Preparing system for update");
    state().maintenanceMode = true;
    pandaBreathService().disconnect();
    audioService().stop();
    settingsService().flush();
    delay(150);

    NetworkClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(12000);
    http.setTimeout(30000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        state().maintenanceMode = false;
        setState(OtaState::Failed, "Firmware download could not start");
        return false;
    }
    http.addHeader("User-Agent", "coroNET-OS-2");
    const int code = http.GET();
    const int length = http.getSize();
    if (code != HTTP_CODE_OK || length < static_cast<int>(kMinimumImageBytes)) {
        http.end();
        state().maintenanceMode = false;
        setState(OtaState::Failed, "Invalid firmware download");
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    if (!stream || !validateImageHeader(*stream, static_cast<size_t>(length)) || !Update.begin(length)) {
        http.end();
        state().maintenanceMode = false;
        setState(OtaState::Failed, "Firmware image rejected");
        return false;
    }
    setState(OtaState::Downloading, "Downloading and installing", 1);
    Update.onProgress([](size_t current, size_t total) {
        if (total > 0) state().otaProgress = static_cast<uint8_t>(constrain((current * 100U) / total, 1U, 99U));
    });
    const size_t written = Update.writeStream(*stream);
    const bool ok = written == static_cast<size_t>(length) && Update.end(true) && Update.isFinished();
    http.end();
    if (!ok) {
        state().maintenanceMode = false;
        char message[80];
        snprintf(message, sizeof(message), "Update failed: %s", Update.errorString());
        setState(OtaState::Failed, message);
        return false;
    }
    return true;
}

bool OtaService::installFromSd() {
    setState(OtaState::Preparing, "Checking /firmware.bin on SD");
    if (!audioService().mountStorage()) {
        setState(OtaState::Failed, "SD card unavailable");
        return false;
    }
    audioService().stop();
    delay(100);
    File firmware = SD_MMC.open("/firmware.bin", FILE_READ);
    if (!firmware || firmware.size() < kMinimumImageBytes || !validateImageHeader(firmware, firmware.size())) {
        if (firmware) firmware.close();
        setState(OtaState::Failed, "No valid /firmware.bin");
        return false;
    }
    state().maintenanceMode = true;
    pandaBreathService().disconnect();
    settingsService().flush();
    const size_t size = firmware.size();
    if (!Update.begin(size)) {
        firmware.close();
        state().maintenanceMode = false;
        setState(OtaState::Failed, "Firmware image does not fit");
        return false;
    }
    setState(OtaState::Installing, "Installing recovery firmware", 5);
    Update.onProgress([](size_t current, size_t total) {
        if (total > 0) state().otaProgress = static_cast<uint8_t>(constrain((current * 100U) / total, 5U, 99U));
    });
    const size_t written = Update.writeStream(firmware);
    firmware.close();
    const bool ok = written == size && Update.end(true) && Update.isFinished();
    if (!ok) {
        state().maintenanceMode = false;
        setState(OtaState::Failed, Update.errorString());
        return false;
    }
    SD_MMC.remove("/firmware.applied");
    SD_MMC.rename("/firmware.bin", "/firmware.applied");
    return true;
}

void OtaService::factoryReset() {
    settingsService().resetToDefaults();
    settingsService().flush();
    delay(100);
    ESP.restart();
}

void OtaService::setState(OtaState value, const char* message, uint8_t progress) {
    state().otaState = value;
    state().otaProgress = progress;
    strlcpy(state().otaStatusText, message ? message : "", sizeof(state().otaStatusText));
    Serial.printf("[ota] state=%s progress=%u %s\n", otaStateName(value),
                  static_cast<unsigned>(progress), state().otaStatusText);
}

void OtaService::logStatus() const {
    Serial.printf("[ota] state=%s progress=%u available=%u version=%s status=%s\n",
                  otaStateName(state().otaState), static_cast<unsigned>(state().otaProgress),
                  state().otaUpdateAvailable, state().otaAvailableVersion,
                  state().otaStatusText);
}

}
