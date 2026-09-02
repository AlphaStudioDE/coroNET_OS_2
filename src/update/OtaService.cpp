#include "OtaService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <SD_MMC.h>
#include <Update.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <time.h>

#include "../audio/AudioService.h"
#include "../config/AppConfig.h"
#include "../core/SystemState.h"
#include "../led/LedService.h"
#include "../panda/PandaBreathService.h"
#include "../settings/SettingsService.h"
#include "../vent/VentService.h"

namespace coronet {

namespace {
OtaService gOtaService;
constexpr uint32_t kTaskStackBytes = 12288;
constexpr uint32_t kMinimumImageBytes = 128U * 1024U;
constexpr uint32_t kValidationDelayMs = 30000U;
constexpr uint32_t kValidationRetryMs = 5000U;
constexpr time_t kMinimumTrustedEpoch = 1700000000;

class TrustedNetworkClient final : public NetworkClientSecure {
public:
    TrustedNetworkClient() {
        attach_ssl_certificate_bundle(sslclient.get(), true);
        _use_ca_bundle = true;
        _use_insecure = false;
        setHandshakeTimeout(15);
    }
};

struct SemanticVersion {
    uint32_t component[3] = {};
    bool prerelease = false;
    char prereleaseName[24] = "";
};

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

bool parseVersion(const char* text, SemanticVersion& output) {
    if (!text) return false;
    const char* cursor = text;
    while (*cursor == 'v' || *cursor == 'V' || isspace(static_cast<unsigned char>(*cursor))) ++cursor;

    for (uint8_t index = 0; index < 3; ++index) {
        if (!isdigit(static_cast<unsigned char>(*cursor))) return false;
        uint32_t value = 0;
        while (isdigit(static_cast<unsigned char>(*cursor))) {
            if (value > 1000000U) return false;
            value = value * 10U + static_cast<uint32_t>(*cursor++ - '0');
        }
        output.component[index] = value;
        if (index < 2 && *cursor++ != '.') return false;
    }

    if (*cursor == '-') {
        output.prerelease = true;
        ++cursor;
        size_t length = 0;
        while (cursor[length] && cursor[length] != '+' &&
               !isspace(static_cast<unsigned char>(cursor[length]))) ++length;
        if (length == 0 || length >= sizeof(output.prereleaseName)) return false;
        memcpy(output.prereleaseName, cursor, length);
        output.prereleaseName[length] = '\0';
        cursor += length;
    }
    if (*cursor == '+') {
        ++cursor;
        if (!*cursor) return false;
        while (*cursor && !isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    }
    while (isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    return *cursor == '\0';
}

int compareVersions(const char* remoteText, const char* installedText) {
    SemanticVersion remote;
    SemanticVersion installed;
    if (!parseVersion(remoteText, remote) || !parseVersion(installedText, installed)) return INT8_MIN;
    for (uint8_t index = 0; index < 3; ++index) {
        if (remote.component[index] > installed.component[index]) return 1;
        if (remote.component[index] < installed.component[index]) return -1;
    }
    if (remote.prerelease != installed.prerelease) return remote.prerelease ? -1 : 1;
    if (!remote.prerelease) return 0;
    const int suffix = strcmp(remote.prereleaseName, installed.prereleaseName);
    return suffix > 0 ? 1 : (suffix < 0 ? -1 : 0);
}

bool isHexDigest(const char* value) {
    if (!value) return false;
    for (size_t index = 0; index < 32; ++index) {
        if (!isxdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return value[32] == '\0';
}
}

OtaService& otaService() {
    return gOtaService;
}

void OtaService::begin() {
    stableSinceMs_ = millis();
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState = ESP_OTA_IMG_UNDEFINED;
    appPendingValidation_ = running &&
                            esp_ota_get_state_partition(running, &imageState) == ESP_OK &&
                            imageState == ESP_OTA_IMG_PENDING_VERIFY;
    appMarkedValid_ = !appPendingValidation_;
    setState(OtaState::Idle,
             appPendingValidation_ ? "Validating updated firmware" : "Ready");
    if (appPendingValidation_) Serial.println("[ota] rollback validation window started");
}

void OtaService::loop() {
    if (!appPendingValidation_ || appMarkedValid_) return;
    const uint32_t now = millis();
    if (now - stableSinceMs_ < kValidationDelayMs ||
        now - lastValidationAttemptMs_ < kValidationRetryMs) return;
    if (!state().displayReady || !state().touchReady || !state().ledReady) return;

    lastValidationAttemptMs_ = now;
    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    if (result == ESP_OK) {
        appMarkedValid_ = true;
        appPendingValidation_ = false;
        setState(OtaState::Idle, "Ready");
        Serial.println("[ota] running firmware marked valid");
    } else {
        Serial.printf("[ota] app validation failed: %s\n", esp_err_to_name(result));
    }
}

bool OtaService::requestCheck() { return startRequest(Request::Check); }

bool OtaService::requestInstall(bool allowSameVersion) {
    return startRequest(allowSameVersion ? Request::Reinstall : Request::Install);
}

bool OtaService::requestSdRecovery() { return startRequest(Request::SdRecovery); }

bool OtaService::startRequest(Request request) {
    if (request != Request::Check && appPendingValidation_) {
        setState(OtaState::Failed, "Wait for startup validation to finish");
        return false;
    }

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
        checkLatestRelease();
        return;
    }
    if (request == Request::SdRecovery) {
        ok = installFromSd();
    } else {
        if (!state().wifiConnected) {
            setState(OtaState::Failed, "Wi-Fi connection required");
            return;
        }
        if (!checkLatestRelease()) return;
        const bool reinstall = request == Request::Reinstall;
        if (!downloadUrl_[0]) {
            setState(OtaState::Failed, "No published firmware release");
            return;
        }
        if (releaseComparison_ < 0) {
            setState(OtaState::Failed, "Latest release is older than installed firmware");
            return;
        }
        if (releaseComparison_ == 0 && !reinstall) {
            setState(OtaState::UpToDate, "Firmware is up to date", 100);
            return;
        }
        ok = installFromUrl(downloadUrl_);
    }

    if (ok) {
        setState(OtaState::Success, "Update complete; restarting", 100);
        delay(800);
        ESP.restart();
    }
}

bool OtaService::ensureSecureClock() {
    if (time(nullptr) >= kMinimumTrustedEpoch) {
        state().timeReady = true;
        return true;
    }
    setState(OtaState::Checking, "Synchronizing secure clock");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    const uint32_t started = millis();
    while (millis() - started < 8000U) {
        if (time(nullptr) >= kMinimumTrustedEpoch) {
            state().timeReady = true;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    setState(OtaState::Failed, "Secure clock synchronization failed");
    return false;
}

bool OtaService::checkLatestRelease() {
    if (!state().wifiConnected) {
        setState(OtaState::Failed, "Wi-Fi connection required");
        return false;
    }
    state().otaUpdateAvailable = false;
    state().otaAvailableVersion[0] = '\0';
    downloadUrl_[0] = '\0';
    checksumUrl_[0] = '\0';
    expectedImageSize_ = 0;
    releaseComparison_ = 0;
    if (!ensureSecureClock()) return false;
    setState(OtaState::Checking, "Checking GitHub releases");

    TrustedNetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    if (!http.begin(client, config::GitHubLatestReleaseApi)) {
        setState(OtaState::Failed, "Could not open update service");
        return false;
    }
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");
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

    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["size"] = true;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(
        doc,
        http.getStream(),
        DeserializationOption::Filter(filter));
    if (error) {
        http.end();
        setState(OtaState::Failed, "Invalid release metadata");
        return false;
    }

    const char* tag = doc["tag_name"] | "";
    const char* selectedUrl = "";
    const char* checksumUrl = "";
    size_t selectedSize = 0;
    for (JsonObjectConst asset : doc["assets"].as<JsonArrayConst>()) {
        const char* name = asset["name"] | "";
        if (strcmp(name, "coronet_os2.bin") == 0 || strcmp(name, "firmware.bin") == 0 ||
            strcmp(name, "coroNET_OS_2.bin") == 0) {
            selectedUrl = asset["browser_download_url"] | "";
            selectedSize = asset["size"] | 0U;
        } else if (strcmp(name, "coronet_os2.bin.md5") == 0 ||
                   strcmp(name, "firmware.bin.md5") == 0 ||
                   strcmp(name, "coroNET_OS_2.bin.md5") == 0) {
            checksumUrl = asset["browser_download_url"] | "";
        }
    }

    const int comparison = compareVersions(tag, config::FirmwareVersion);
    if (!tag[0] || comparison == INT8_MIN) {
        http.end();
        setState(OtaState::Failed, "Release has an invalid version tag");
        return false;
    }
    if (!selectedUrl[0] || selectedSize < kMinimumImageBytes) {
        http.end();
        setState(OtaState::Failed, "Release has no compatible firmware asset");
        return false;
    }
    if (!checksumUrl[0]) {
        http.end();
        setState(OtaState::Failed, "Release has no firmware checksum");
        return false;
    }

    strlcpy(state().otaAvailableVersion, tag, sizeof(state().otaAvailableVersion));
    strlcpy(downloadUrl_, selectedUrl, sizeof(downloadUrl_));
    strlcpy(checksumUrl_, checksumUrl, sizeof(checksumUrl_));
    expectedImageSize_ = selectedSize;
    releaseComparison_ = static_cast<int8_t>(comparison);
    state().otaUpdateAvailable = comparison > 0;
    http.end();

    if (comparison > 0) setState(OtaState::UpdateAvailable, "Update available", 100);
    else if (comparison == 0) setState(OtaState::UpToDate, "Firmware is up to date", 100);
    else setState(OtaState::UpToDate, "Installed firmware is newer than latest release", 100);
    return true;
}

bool OtaService::fetchExpectedMd5(const char* url, char output[33]) {
    if (!url || !url[0] || !output) return false;
    output[0] = '\0';

    TrustedNetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return false;
    http.addHeader("User-Agent", "coroNET-OS-2");
    if (http.GET() != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    for (size_t start = 0; start + 32U <= body.length(); ++start) {
        bool valid = true;
        for (size_t index = 0; index < 32; ++index) {
            if (!isxdigit(static_cast<unsigned char>(body[start + index]))) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;
        for (size_t index = 0; index < 32; ++index) {
            output[index] = static_cast<char>(tolower(static_cast<unsigned char>(body[start + index])));
        }
        output[32] = '\0';
        return isHexDigest(output);
    }
    return false;
}

void OtaService::enterMaintenance() {
    setState(OtaState::Preparing, "Preparing system for update");
    settingsService().flush();
    state().maintenanceMode = true;
    ledService().cancelPreview();
    pandaBreathService().disconnect();
    ventService().applyNow();
    audioWasReady_ = state().audioReady;
    audioService().release();

    const uint32_t started = millis();
    while (millis() - started < 800U && (state().bleReady || state().webReady)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    Serial.printf("[ota-memory] dma=%lu largest=%lu internal=%lu largest=%lu\n",
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

void OtaService::leaveMaintenance() {
    state().maintenanceMode = false;
    if (audioWasReady_ && !state().audioReady) {
        audioService().useDmaProfile(AudioDmaProfile::Balanced);
    }
    audioWasReady_ = false;
}

bool OtaService::validateImageHeader(Stream& stream, size_t expectedSize) {
    if (expectedSize < kMinimumImageBytes) return false;
    return stream.peek() == 0xE9;
}

bool OtaService::installFromUrl(const char* url) {
    if (!url || !url[0] || !checksumUrl_[0]) {
        setState(OtaState::Failed, "Incomplete firmware release metadata");
        return false;
    }

    char expectedMd5[33] = "";
    setState(OtaState::Preparing, "Verifying release checksum");
    if (!fetchExpectedMd5(checksumUrl_, expectedMd5)) {
        setState(OtaState::Failed, "Firmware checksum could not be verified");
        return false;
    }
    enterMaintenance();

    TrustedNetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(12000);
    http.setTimeout(30000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        leaveMaintenance();
        setState(OtaState::Failed, "Firmware download could not start");
        return false;
    }
    http.addHeader("User-Agent", "coroNET-OS-2");
    const int code = http.GET();
    const int length = http.getSize();
    if (code != HTTP_CODE_OK || length < static_cast<int>(kMinimumImageBytes) ||
        (expectedImageSize_ && static_cast<size_t>(length) != expectedImageSize_)) {
        http.end();
        leaveMaintenance();
        setState(OtaState::Failed, "Invalid firmware download");
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream || !validateImageHeader(*stream, static_cast<size_t>(length)) ||
        !Update.begin(static_cast<size_t>(length)) || !Update.setMD5(expectedMd5)) {
        http.end();
        leaveMaintenance();
        setState(OtaState::Failed, "Firmware image rejected");
        return false;
    }

    setState(OtaState::Downloading, "Downloading firmware", 1);
    Update.onProgress([](size_t current, size_t total) {
        if (total > 0) {
            state().otaProgress = static_cast<uint8_t>(constrain(
                static_cast<uint32_t>((static_cast<uint64_t>(current) * 98ULL) / total), 1U, 98U));
        }
    });
    const size_t written = Update.writeStream(*stream);
    setState(OtaState::Installing, "Verifying installed firmware", 99);
    const bool ok = written == static_cast<size_t>(length) && Update.end(true) && Update.isFinished();
    http.end();
    if (!ok) {
        leaveMaintenance();
        char message[96];
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
    if (!firmware || firmware.size() < kMinimumImageBytes ||
        !validateImageHeader(firmware, firmware.size())) {
        if (firmware) firmware.close();
        setState(OtaState::Failed, "No valid /firmware.bin");
        return false;
    }

    enterMaintenance();
    const size_t size = firmware.size();
    if (!Update.begin(size)) {
        firmware.close();
        leaveMaintenance();
        setState(OtaState::Failed, "Firmware image does not fit");
        return false;
    }
    setState(OtaState::Installing, "Installing recovery firmware", 1);
    Update.onProgress([](size_t current, size_t total) {
        if (total > 0) {
            state().otaProgress = static_cast<uint8_t>(constrain(
                static_cast<uint32_t>((static_cast<uint64_t>(current) * 99ULL) / total), 1U, 99U));
        }
    });
    const size_t written = Update.writeStream(firmware);
    firmware.close();
    const bool ok = written == size && Update.end(true) && Update.isFinished();
    if (!ok) {
        leaveMaintenance();
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
