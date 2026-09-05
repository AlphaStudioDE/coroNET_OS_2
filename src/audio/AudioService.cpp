#include "AudioService.h"

#include <SD_MMC.h>
#include <esp_heap_caps.h>

#include "../config/HardwareConfig.h"
#include "../boot/BootExperience.h"
#include "../core/SystemState.h"
#include "../led/LedAnimations.h"
#include "../settings/SettingsService.h"

namespace coronet {

namespace {

AudioService gAudioService;
constexpr uint32_t kToneFrequencyHz = 660;
constexpr int32_t kToneAmplitude = 5200;
constexpr uint32_t kFadeMs = 45;

bool quietSuppressesSound(SoundScenario scenario) {
    const AppSettings settings = settingsService().snapshot();
    if (!state().quietActive) return false;
    if (scenario == SoundScenario::Error && settings.quietErrorsBypass) return false;
    return settings.quietTarget == QuietTarget::Sound ||
           settings.quietTarget == QuietTarget::SoundAndLeds;
}

constexpr int16_t kSineLut[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30274, 31357, 32138, 32610,
    32767, 32610, 32138, 31357, 30274, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30274, -31357, -32138, -32610,
    -32767, -32610, -32138, -31357, -30274, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212,
};

uint16_t readLe16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8U));
}

uint32_t readLe32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8U) |
           (static_cast<uint32_t>(data[2]) << 16U) |
           (static_cast<uint32_t>(data[3]) << 24U);
}

const char* profileName(AudioDmaProfile profile) {
    return profile == AudioDmaProfile::Coronet1 ? "coronet1" : "balanced";
}

const char* scenarioStem(SoundScenario scenario) {
    switch (scenario) {
        case SoundScenario::Start: return "start";
        case SoundScenario::Finish: return "finish";
        case SoundScenario::Error: return "error";
        case SoundScenario::Pause: return "pause";
        case SoundScenario::Idle: return "idle";
        default: return "start";
    }
}

const char* pathLeaf(const char* path) {
    if (!path || !path[0]) return "";
    const char* slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

}

AudioService& audioService() {
    return gAudioService;
}

void AudioService::begin() {
    state().audioReady = false;
    state().audioPlaying = false;
    state().sdReady = false;
    strlcpy(state().audioStatusText, "Initializing audio...", sizeof(state().audioStatusText));

    pcmBuffer_ = static_cast<int16_t*>(heap_caps_calloc(
        BufferFrames, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    rawBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
        RawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    fileIndex_ = static_cast<char (*)[65]>(heap_caps_calloc(
        MaxIndexedFiles, 65, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    folderQueue_ = static_cast<FolderScanEntry*>(heap_caps_calloc(
        MaxScanFolders, sizeof(FolderScanEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    libraryFolderNames_ = static_cast<char (*)[33]>(heap_caps_calloc(
        MaxLibraryFolders, 33, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    fileFolderIds_ = static_cast<uint8_t*>(heap_caps_calloc(
        MaxIndexedFiles, sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pcmBuffer_ || !rawBuffer_ || !fileIndex_ || !folderQueue_ ||
        !libraryFolderNames_ || !fileFolderIds_) {
        Serial.println("[audio] PSRAM buffer allocation failed");
        strlcpy(state().audioStatusText, "Audio memory allocation failed", sizeof(state().audioStatusText));
        return;
    }

    mountStorage();

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry, "coronet-audio", TaskStackBytes, this, TaskPriority, &task_, TaskCore);
    if (created != pdPASS) {
        Serial.println("[audio] task creation failed");
        strlcpy(state().audioStatusText, "Audio task could not start", sizeof(state().audioStatusText));
        task_ = nullptr;
        return;
    }

    const uint32_t initStarted = millis();
    while (!taskInitDone_ && millis() - initStarted < 2000U) vTaskDelay(pdMS_TO_TICKS(1));
    if (!taskInitDone_ || !taskInitOk_) {
        Serial.println("[audio] Core 0 I2S initialization failed or timed out");
        strlcpy(state().audioStatusText, "I2S initialization failed", sizeof(state().audioStatusText));
        return;
    }

    state().audioReady = true;
    strlcpy(state().audioStatusText, "Ready", sizeof(state().audioStatusText));
    Serial.println("[audio] ready; I2S interrupt and WAV playback run on Core 0");
    logStatus();

}

void AudioService::loop() {
    state().audioPlaying = playing_;
    if (playing_ && !bootAudioActive_ && state().lastTouchMs > playbackStartedMs_) {
        stop();
    }
    processPrinterSoundEvents();
}

bool AudioService::mountStorage() {
    if (storageReady_ && SD_MMC.cardType() != CARD_NONE) return true;
    storageReady_ = false;
    state().sdReady = false;
    SD_MMC.end();
    delay(250);
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (!SD_MMC.setPins(hw::SdMmcClkPin, hw::SdMmcCmdPin, hw::SdMmcD0Pin)) {
            Serial.println("[audio] SD_MMC pin assignment failed");
            return false;
        }
        if (SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT) &&
            SD_MMC.cardType() != CARD_NONE) {
            storageReady_ = true;
            state().sdReady = true;
            Serial.printf("[audio] SD ready: %lluMB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
            refreshFileIndex();
            return true;
        }
        Serial.printf("[audio] SD mount attempt %u failed\n", static_cast<unsigned>(attempt + 1U));
        SD_MMC.end();
        delay(300);
    }
    SD_MMC.end();
    state().sdReady = false;
    strlcpy(state().audioStatusText, "SD card unavailable", sizeof(state().audioStatusText));
    return false;
}

const char* AudioService::filePath(uint8_t index) const {
    return fileIndex_ && !indexingFiles_ && index < fileCount_ ? fileIndex_[index] : nullptr;
}

const char* AudioService::folderName(uint8_t folder) const {
    return libraryFolderNames_ && !indexingFiles_ && folder < libraryFolderCount_
               ? libraryFolderNames_[folder]
               : nullptr;
}

uint8_t AudioService::folderFileCount(uint8_t folder) const {
    if (indexingFiles_ || !fileFolderIds_ || folder >= libraryFolderCount_) return 0;
    uint8_t count = 0;
    for (uint8_t index = 0; index < fileCount_; ++index) {
        if (fileFolderIds_[index] == folder) ++count;
    }
    return count;
}

const char* AudioService::folderFilePath(uint8_t folder, uint8_t index) const {
    if (indexingFiles_ || !fileIndex_ || !fileFolderIds_ ||
        folder >= libraryFolderCount_) {
        return nullptr;
    }
    uint8_t position = 0;
    for (uint8_t file = 0; file < fileCount_; ++file) {
        if (fileFolderIds_[file] != folder) continue;
        if (position++ == index) return fileIndex_[file];
    }
    return nullptr;
}

uint8_t AudioService::folderForPath(const char* path) const {
    if (indexingFiles_ || !fileIndex_ || !fileFolderIds_ || !path || !path[0]) return UINT8_MAX;
    for (uint8_t index = 0; index < fileCount_; ++index) {
        if (strcasecmp(fileIndex_[index], path) == 0) return fileFolderIds_[index];
    }
    return UINT8_MAX;
}

bool AudioService::pathAvailable(const char* path) const {
    if (!storageReady_ || indexingFiles_ || !fileIndex_ || !path || path[0] != '/') return false;
    for (uint8_t index = 0; index < fileCount_; ++index) {
        if (strcasecmp(fileIndex_[index], path) == 0) return true;
    }
    return false;
}

bool AudioService::refreshFileIndex() {
    if (!storageReady_ || !fileIndex_ || playing_) return false;
    if (SD_MMC.cardType() == CARD_NONE) {
        storageReady_ = false;
        state().sdReady = false;
        state().audioFileCount = 0;
        state().audioAssetsValid = false;
        strlcpy(state().audioAssetStatus, "SD card removed", sizeof(state().audioAssetStatus));
        return false;
    }
    indexingFiles_ = true;
    fileCount_ = 0;
    folderQueueCount_ = 0;
    folderQueueRead_ = 0;
    libraryFolderCount_ = 0;
    memset(fileIndex_, 0, MaxIndexedFiles * 65U);
    memset(folderQueue_, 0, MaxScanFolders * sizeof(FolderScanEntry));
    memset(libraryFolderNames_, 0, MaxLibraryFolders * 33U);
    memset(fileFolderIds_, 0, MaxIndexedFiles * sizeof(uint8_t));
    // The organized /sounds library has priority. Root WAV files are retained
    // only as a compatibility fallback when the bounded index still has room.
    enqueueDirectory("/sounds", 0U);
    while (folderQueueRead_ < folderQueueCount_ && fileCount_ < MaxIndexedFiles) {
        const FolderScanEntry folder = folderQueue_[folderQueueRead_++];
        indexDirectory(folder.path, folder.depth, true);
    }
    indexDirectory("/", 0U, false);
    sortFileIndex();
    buildLibraryFolders();
    indexingFiles_ = false;
    state().audioFileCount = fileCount_;
    validateAssets();
    snprintf(state().audioStatusText, sizeof(state().audioStatusText),
             "%u status WAV file%s ready", static_cast<unsigned>(fileCount_),
             fileCount_ == 1U ? "" : "s");
    Serial.printf("[audio] indexed %u WAV files in %u folders; assets=%s\n",
                  static_cast<unsigned>(fileCount_),
                  static_cast<unsigned>(libraryFolderCount_), state().audioAssetStatus);
    return true;
}

bool AudioService::requestStorageRefresh() {
    if (!task_ || bootAudioActive_) return false;
    strlcpy(state().audioAssetStatus, "Scanning SD card...", sizeof(state().audioAssetStatus));
    strlcpy(state().audioStatusText, "Scanning SD card...", sizeof(state().audioStatusText));
    submitRequest(RequestType::RescanStorage, "", 0, false, SoundScenario::Start, false);
    return true;
}

void AudioService::indexDirectory(const char* path, uint8_t depth, bool enqueueSubdirectories) {
    if (fileCount_ >= MaxIndexedFiles) return;
    File directory = SD_MMC.open(path);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return;
    }
    File entry = directory.openNextFile();
    while (entry && fileCount_ < MaxIndexedFiles) {
        if (entry.isDirectory()) {
            const char* entryName = entry.name();
            if (enqueueSubdirectories && depth < 3U && entryName && entryName[0] != '.' &&
                strcasecmp(entryName, "System Volume Information") != 0) {
                enqueueDirectory(entry.path(), static_cast<uint8_t>(depth + 1U));
            }
        } else {
            const char* entryPath = entry.path();
            const char* extension = entryPath ? strrchr(entryPath, '.') : nullptr;
            if (extension && strcasecmp(extension, ".wav") == 0) addIndexedFile(entryPath);
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
}

bool AudioService::enqueueDirectory(const char* path, uint8_t depth) {
    if (!folderQueue_ || !path || !path[0] || strlen(path) >= 65U ||
        folderQueueCount_ >= MaxScanFolders) {
        return false;
    }
    for (uint8_t i = 0; i < folderQueueCount_; ++i) {
        if (strcasecmp(folderQueue_[i].path, path) == 0) return false;
    }
    strlcpy(folderQueue_[folderQueueCount_].path, path, sizeof(folderQueue_[0].path));
    folderQueue_[folderQueueCount_].depth = depth;
    ++folderQueueCount_;
    return true;
}

bool AudioService::addIndexedFile(const char* path) {
    if (!path || !path[0] || fileCount_ >= MaxIndexedFiles) return false;
    if (strlen(path) >= 65U) return false;
    char normalized[65] = "/";
    if (path[0] == '/') strlcpy(normalized, path, sizeof(normalized));
    else strlcpy(normalized + 1, path, sizeof(normalized) - 1U);
    for (uint8_t i = 0; i < fileCount_; ++i) {
        if (strcasecmp(fileIndex_[i], normalized) == 0) return false;
    }
    if (strcasecmp(normalized, "/boot.wav") == 0) return false;
    strlcpy(fileIndex_[fileCount_++], normalized, 65);
    return true;
}

void AudioService::sortFileIndex() {
    char temporary[65] = "";
    for (uint8_t index = 1U; index < fileCount_; ++index) {
        strlcpy(temporary, fileIndex_[index], sizeof(temporary));
        uint8_t position = index;
        while (position > 0U && strcasecmp(fileIndex_[position - 1U], temporary) > 0) {
            strlcpy(fileIndex_[position], fileIndex_[position - 1U], 65);
            --position;
        }
        strlcpy(fileIndex_[position], temporary, 65);
    }
}

void AudioService::buildLibraryFolders() {
    libraryFolderCount_ = 0;
    for (uint8_t file = 0; file < fileCount_; ++file) {
        const char* path = fileIndex_[file];
        char folderName[33] = "SD ROOT";
        if (strncasecmp(path, "/sounds/", 8U) == 0) {
            const char* relative = path + 8U;
            const char* separator = strchr(relative, '/');
            if (separator && separator > relative) {
                const size_t length = min<size_t>(32U, static_cast<size_t>(separator - relative));
                memcpy(folderName, relative, length);
                folderName[length] = '\0';
            } else {
                strlcpy(folderName, "GENERAL", sizeof(folderName));
            }
        }

        uint8_t folder = 0;
        bool found = false;
        for (; folder < libraryFolderCount_; ++folder) {
            if (strcasecmp(libraryFolderNames_[folder], folderName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (libraryFolderCount_ >= MaxLibraryFolders) {
                fileFolderIds_[file] = 0;
                continue;
            }
            folder = libraryFolderCount_++;
            strlcpy(libraryFolderNames_[folder], folderName, 33);
        }
        fileFolderIds_[file] = folder;
    }
}

void AudioService::validateAssets() {
    uint8_t missing = 0;
    char resolved[65];
    for (uint8_t i = 0; i < enumCount(SoundScenario{}); ++i) {
        if (!resolveScenarioPath(static_cast<SoundScenario>(i), resolved)) ++missing;
    }
    const bool bootPresent = SD_MMC.exists("/boot.wav");
    state().audioAssetsValid = missing == 0 && bootPresent;
    if (missing == 0 && bootPresent) strlcpy(state().audioAssetStatus, "All required WAV assets ready", sizeof(state().audioAssetStatus));
    else if (!bootPresent) strlcpy(state().audioAssetStatus, "boot.wav missing or scenarios incomplete", sizeof(state().audioAssetStatus));
    else snprintf(state().audioAssetStatus, sizeof(state().audioAssetStatus), "%u scenario file(s) missing", static_cast<unsigned>(missing));
}

bool AudioService::playTestTone(uint32_t durationMs) {
    if (!driverReady_ || !task_) return false;
    durationMs = constrain(durationMs, 250U, 60000U);
    submitRequest(RequestType::Tone, "", 100, false, SoundScenario::Start, false, durationMs);
    return true;
}

bool AudioService::playFile(const char* path, uint8_t volumePercent, bool repeat,
                            SoundScenario scenario, bool bootAudio) {
    if (!driverReady_ || !task_ || !path || path[0] != '/') return false;
    if (!storageReady_ && !mountStorage()) return false;
    if (!SD_MMC.exists(path)) {
        Serial.printf("[audio] file not found: %s\n", path);
        snprintf(state().audioStatusText, sizeof(state().audioStatusText),
                 "WAV not found: %s", pathLeaf(path));
        return false;
    }
    submitRequest(RequestType::Wav, path, constrain(volumePercent, 0, 100), repeat,
                  scenario, bootAudio);
    return true;
}

bool AudioService::playScenario(SoundScenario scenario) {
    const uint8_t index = static_cast<uint8_t>(scenario);
    if (index >= enumCount(SoundScenario{})) return false;
    if (quietSuppressesSound(scenario)) {
        stop();
        return false;
    }
    char path[65] = "";
    if (!resolveScenarioPath(scenario, path)) {
        stop();
        return false;
    }
    const AppSettings settings = settingsService().snapshot();
    return playFile(path, settings.soundVolume[index], settings.soundRepeat[index], scenario, false);
}

void AudioService::stop() {
    if (!task_) return;
    strlcpy(state().audioStatusText, playing_ ? "Stopping playback..." : "Ready",
            sizeof(state().audioStatusText));
    submitRequest(RequestType::Stop, "", 0, false, SoundScenario::Start, false);
}

void AudioService::release() {
    if (!stopAndWait()) {
        Serial.println("[audio] release aborted: task did not stop");
        return;
    }
    uninstallDriver();
    state().audioReady = false;
    logMemory("released");
}

bool AudioService::useDmaProfile(AudioDmaProfile profile) {
    if (profile == profile_ && driverReady_) return true;
    if (!stopAndWait()) {
        Serial.println("[audio] DMA profile change aborted: task did not stop");
        return false;
    }
    uninstallDriver();
    profile_ = profile;
    const uint16_t count = profile == AudioDmaProfile::Coronet1
                               ? Coronet1DmaBufferCount
                               : BalancedDmaBufferCount;
    state().audioReady = installDriver(count) && task_;
    logStatus();
    return state().audioReady;
}

bool AudioService::setSampleRate(uint32_t sampleRate) {
    if (sampleRate != 22050U && sampleRate != 44100U && sampleRate != 48000U) return false;
    if (!stopAndWait()) {
        Serial.println("[audio] sample-rate change aborted: task did not stop");
        return false;
    }
    return reconfigureClock(sampleRate);
}

void AudioService::logStatus() const {
    const UBaseType_t stackHeadroom = task_ ? uxTaskGetStackHighWaterMark(task_) : 0;
    Serial.printf(
        "[audio] ready=%u sd=%u playing=%u boot=%u profile=%s mono/16-bit/%luHz dma=%ux%u failures=%lu retries=%lu indexed=%u folders=%u completed=%lu stackHeadroom=%uB status=%s path=%s\n",
        driverReady_ ? 1U : 0U, storageReady_ ? 1U : 0U, playing_ ? 1U : 0U,
        bootAudioActive_ ? 1U : 0U, profileName(profile_), static_cast<unsigned long>(sampleRate_),
        static_cast<unsigned>(dmaBufferCount_), static_cast<unsigned>(BufferFrames),
        static_cast<unsigned long>(writeFailures_), static_cast<unsigned long>(writeRetries_),
        static_cast<unsigned>(fileCount_),
        static_cast<unsigned>(libraryFolderCount_),
        static_cast<unsigned long>(completedFiles_),
        static_cast<unsigned>(stackHeadroom),
        state().audioStatusText,
        state().activeSoundPath[0] ? state().activeSoundPath : "-");
    logMemory("status");
}

uint32_t AudioService::submitRequest(RequestType type, const char* path, uint8_t volumePercent,
                                     bool repeat, SoundScenario scenario, bool bootAudio,
                                     uint32_t durationMs) {
    portENTER_CRITICAL(&requestMux_);
    requestedType_ = type;
    strlcpy(requestedPath_, path ? path : "", sizeof(requestedPath_));
    requestedVolume_ = volumePercent;
    requestedRepeat_ = repeat;
    requestedScenario_ = scenario;
    requestedBootAudio_ = bootAudio;
    requestedDurationMs_ = durationMs;
    const uint32_t sequence = requestSequence_ + 1U;
    requestSequence_ = sequence;
    portEXIT_CRITICAL(&requestMux_);
    xTaskNotifyGive(task_);
    return sequence;
}

bool AudioService::stopAndWait(uint32_t timeoutMs) {
    if (!task_) return false;
    const uint32_t sequence = submitRequest(
        RequestType::Stop, "", 0, false, SoundScenario::Start, false);
    const uint32_t started = millis();
    while (static_cast<int32_t>(completedRequestSequence_ - sequence) < 0) {
        if (millis() - started >= timeoutMs) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

void AudioService::snapshotRequest(uint32_t& sequence, RequestType& type, char path[65],
                                   uint8_t& volumePercent, bool& repeat, SoundScenario& scenario,
                                   bool& bootAudio, uint32_t& durationMs) {
    portENTER_CRITICAL(&requestMux_);
    sequence = requestSequence_;
    type = requestedType_;
    strlcpy(path, requestedPath_, 65);
    volumePercent = requestedVolume_;
    repeat = requestedRepeat_;
    scenario = requestedScenario_;
    bootAudio = requestedBootAudio_;
    durationMs = requestedDurationMs_;
    portEXIT_CRITICAL(&requestMux_);
}

void AudioService::taskEntry(void* context) {
    static_cast<AudioService*>(context)->taskLoop();
}

void AudioService::taskLoop() {
    taskInitOk_ = installDriver(BalancedDmaBufferCount);
    taskInitDone_ = true;
    if (!taskInitOk_) {
        task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    uint32_t handledSequence = 0;
    while (true) {
        if (handledSequence == requestSequence_) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint32_t sequence = 0;
        RequestType type = RequestType::Stop;
        char path[65] = "";
        uint8_t volume = 0;
        bool repeat = false;
        SoundScenario scenario = SoundScenario::Start;
        bool bootAudio = false;
        uint32_t durationMs = 0;
        snapshotRequest(sequence, type, path, volume, repeat, scenario, bootAudio, durationMs);
        handledSequence = sequence;

        closeWav();
        playing_ = false;
        bootAudioActive_ = false;
        vTaskPrioritySet(nullptr, TaskPriority);
        state().audioPlaying = false;
        state().activeSoundPath[0] = '\0';
        primeSilence();
        if (type == RequestType::Stop) {
            strlcpy(state().audioStatusText, "Ready", sizeof(state().audioStatusText));
            completedRequestSequence_ = sequence;
            continue;
        }
        if (type == RequestType::RescanStorage) {
            storageReady_ = false;
            state().sdReady = false;
            state().audioFileCount = 0;
            SD_MMC.end();
            vTaskDelay(pdMS_TO_TICKS(30));
            mountStorage();
            completedRequestSequence_ = sequence;
            continue;
        }

        playbackStartedMs_ = millis();
        playing_ = true;
        bootAudioActive_ = bootAudio;
        vTaskPrioritySet(nullptr, bootAudio ? BootTaskPriority : TaskPriority);
        state().audioPlaying = true;
        state().activeSoundScenario = scenario;

        if (type == RequestType::Tone) {
            phase_ = 0;
            outputFrames_ = 0;
            const uint32_t stopAt = millis() + durationMs;
            bool writeOk = true;
            while (requestSequence_ == sequence && static_cast<int32_t>(millis() - stopAt) < 0) {
                if (!writeToneBuffer(stopAt)) {
                    ++writeFailures_;
                    writeOk = false;
                    break;
                }
            }
            const bool interrupted = requestSequence_ != sequence;
            if (interrupted && !fadeToneToSilence()) {
                ++writeFailures_;
                writeOk = false;
            }
            finishPlayback(writeOk && !interrupted, interrupted);
            completedRequestSequence_ = sequence;
            continue;
        }

        strlcpy(state().activeSoundPath, path, sizeof(state().activeSoundPath));
        snprintf(state().audioStatusText, sizeof(state().audioStatusText),
                 "Playing %s", pathLeaf(path));
        bool naturalEnd = false;
        do {
            if (!openWav(path)) break;
            while (requestSequence_ == sequence && wav_.dataRemaining > 0) {
                if (!writeWavBuffer(volume)) {
                    ++writeFailures_;
                    break;
                }
            }
            naturalEnd = requestSequence_ == sequence && wav_.dataRemaining == 0;
            if (!naturalEnd && requestSequence_ != sequence && !fadeWavToSilence(volume)) {
                ++writeFailures_;
            }
            closeWav();
            if (naturalEnd) ++completedFiles_;
        } while (repeat && naturalEnd && requestSequence_ == sequence);
        finishPlayback(naturalEnd, requestSequence_ != sequence);
        completedRequestSequence_ = sequence;
    }
}

bool AudioService::installDriver(uint16_t dmaBufferCount) {
    logMemory("before-driver");
    i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channelConfig.dma_desc_num = dmaBufferCount;
    channelConfig.dma_frame_num = BufferFrames;
    channelConfig.auto_clear_after_cb = true;
    channelConfig.allow_pd = false;

    esp_err_t result = i2s_new_channel(&channelConfig, &txChannel_, nullptr);
    if (result != ESP_OK) {
        txChannel_ = nullptr;
        Serial.printf("[audio] i2s_new_channel failed: %d\n", static_cast<int>(result));
        return false;
    }

    i2s_std_config_t standardConfig = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate_),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(hw::I2sMckPin),
            .bclk = static_cast<gpio_num_t>(hw::I2sBckPin),
            .ws = static_cast<gpio_num_t>(hw::I2sLrckPin),
            .dout = static_cast<gpio_num_t>(hw::I2sDoutPin),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    result = i2s_channel_init_std_mode(txChannel_, &standardConfig);
    if (result == ESP_OK && !preloadSilence()) result = ESP_FAIL;
    if (result == ESP_OK) result = i2s_channel_enable(txChannel_);
    if (result != ESP_OK) {
        i2s_del_channel(txChannel_);
        txChannel_ = nullptr;
        return false;
    }
    dmaBufferCount_ = dmaBufferCount;
    driverReady_ = true;
    logMemory("after-driver");
    return true;
}

void AudioService::uninstallDriver() {
    if (!driverReady_ || !txChannel_) return;
    settleToSilence();
    i2s_channel_disable(txChannel_);
    i2s_del_channel(txChannel_);
    txChannel_ = nullptr;
    driverReady_ = false;
    dmaBufferCount_ = 0;
}

bool AudioService::reconfigureClock(uint32_t sampleRate) {
    if (!driverReady_ || !txChannel_ || sampleRate < 8000U || sampleRate > 48000U) return false;
    if (sampleRate_ == sampleRate) return true;
    settleToSilence();
    esp_err_t result = i2s_channel_disable(txChannel_);
    i2s_std_clk_config_t clockConfig = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    if (result == ESP_OK) result = i2s_channel_reconfig_std_clock(txChannel_, &clockConfig);
    if (result == ESP_OK) {
        sampleRate_ = sampleRate;
        if (!preloadSilence()) result = ESP_FAIL;
    }
    if (result == ESP_OK) result = i2s_channel_enable(txChannel_);
    if (result != ESP_OK) return false;
    return true;
}

bool AudioService::openWav(const char* path) {
    closeWav();
    wavFile_ = SD_MMC.open(path, FILE_READ);
    if (!wavFile_ || wavFile_.isDirectory()) return false;

    uint8_t riff[12] = {};
    if (wavFile_.read(riff, sizeof(riff)) != sizeof(riff) || memcmp(riff, "RIFF", 4) != 0 ||
        memcmp(riff + 8, "WAVE", 4) != 0) {
        closeWav();
        return false;
    }

    bool gotFormat = false;
    bool gotData = false;
    uint16_t format = 0;
    while (wavFile_.available()) {
        uint8_t chunk[8] = {};
        if (wavFile_.read(chunk, sizeof(chunk)) != sizeof(chunk)) break;
        const uint32_t chunkSize = readLe32(chunk + 4);
        const uint32_t chunkData = wavFile_.position();
        if (memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16U) {
            uint8_t formatData[16] = {};
            if (wavFile_.read(formatData, sizeof(formatData)) != sizeof(formatData)) break;
            format = readLe16(formatData);
            wav_.channels = readLe16(formatData + 2);
            wav_.sampleRate = readLe32(formatData + 4);
            wav_.bitsPerSample = readLe16(formatData + 14);
            gotFormat = true;
        } else if (memcmp(chunk, "data", 4) == 0) {
            wav_.dataOffset = chunkData;
            wav_.dataSize = chunkSize;
            gotData = true;
        }
        wavFile_.seek(chunkData + chunkSize + (chunkSize & 1U));
        if (gotFormat && gotData) break;
    }

    if (!gotFormat || !gotData || format != 1U || (wav_.channels != 1U && wav_.channels != 2U) ||
        (wav_.bitsPerSample != 8U && wav_.bitsPerSample != 16U) ||
        wav_.sampleRate < 8000U || wav_.sampleRate > 48000U) {
        Serial.printf("[audio] unsupported WAV: %s\n", path);
        closeWav();
        return false;
    }
    if (!reconfigureClock(wav_.sampleRate)) {
        closeWav();
        return false;
    }
    wav_.dataRemaining = wav_.dataSize;
    wav_.outputFrames = 0;
    wavFile_.seek(wav_.dataOffset);
    primeSilence();
    return true;
}

void AudioService::closeWav() {
    if (wavFile_) wavFile_.close();
    wav_ = {};
}

bool AudioService::writeWavBuffer(uint8_t volumePercent, uint32_t stopFadeOffset,
                                  uint32_t stopFadeFrames) {
    const size_t bytesPerSample = wav_.bitsPerSample / 8U;
    const size_t bytesPerFrame = bytesPerSample * wav_.channels;
    if (!bytesPerFrame || !wav_.dataRemaining) return true;
    size_t frames = min(BufferFrames, static_cast<size_t>(wav_.dataRemaining / bytesPerFrame));
    const size_t bytesWanted = frames * bytesPerFrame;
    const int bytesRead = wavFile_.read(rawBuffer_, bytesWanted);
    if (bytesRead <= 0) return false;
    frames = static_cast<size_t>(bytesRead) / bytesPerFrame;
    wav_.dataRemaining -= frames * bytesPerFrame;

    const uint32_t totalFrames = wav_.dataSize / bytesPerFrame;
    const uint32_t fadeFrames = max<uint32_t>(32U, wav_.sampleRate * kFadeMs / 1000UL);
    const int32_t gainQ15 = static_cast<int32_t>(volumePercent) * 32767 / 100;
    for (size_t frame = 0; frame < frames; ++frame) {
        const uint8_t* input = rawBuffer_ + frame * bytesPerFrame;
        int32_t left = 0;
        int32_t right = 0;
        if (wav_.bitsPerSample == 8U) {
            left = (static_cast<int32_t>(input[0]) - 128) << 8U;
            right = wav_.channels == 2U ? (static_cast<int32_t>(input[1]) - 128) << 8U : left;
        } else {
            left = static_cast<int16_t>(readLe16(input));
            right = wav_.channels == 2U ? static_cast<int16_t>(readLe16(input + 2)) : left;
        }
        int32_t sample = ((left + right) / 2) * gainQ15 / 32767;
        const uint32_t absoluteFrame = wav_.outputFrames + frame;
        if (absoluteFrame < fadeFrames) sample = sample * absoluteFrame / fadeFrames;
        if (totalFrames > absoluteFrame && totalFrames - absoluteFrame < fadeFrames) {
            sample = sample * (totalFrames - absoluteFrame) / fadeFrames;
        }
        if (stopFadeFrames > 0) {
            const uint32_t stopFrame = stopFadeOffset + frame;
            const uint32_t stopRemaining = stopFrame < stopFadeFrames
                                               ? stopFadeFrames - stopFrame
                                               : 0U;
            sample = sample * stopRemaining / stopFadeFrames;
        }
        pcmBuffer_[frame] = static_cast<int16_t>(constrain(sample, -32768, 32767));
    }
    wav_.outputFrames += frames;
    if (frames < BufferFrames) memset(pcmBuffer_ + frames, 0, (BufferFrames - frames) * sizeof(int16_t));
    return writePcm(pcmBuffer_, BufferFrames);
}

bool AudioService::fadeWavToSilence(uint8_t volumePercent) {
    if (!wavFile_ || !wav_.dataRemaining || sampleRate_ == 0) return true;
    const uint32_t fadeFrames = max<uint32_t>(32U, sampleRate_ * kFadeMs / 1000UL);
    uint32_t fadeOffset = 0;
    while (fadeOffset < fadeFrames && wav_.dataRemaining > 0) {
        if (!writeWavBuffer(volumePercent, fadeOffset, fadeFrames)) return false;
        fadeOffset += BufferFrames;
    }
    return true;
}

bool AudioService::writeToneBuffer(uint32_t stopAtMs) {
    const uint32_t phaseStep = static_cast<uint32_t>(
        (static_cast<uint64_t>(kToneFrequencyHz) << 32U) / sampleRate_);
    const uint32_t fadeFrames = sampleRate_ / 50U;
    const int32_t remainingMs = static_cast<int32_t>(stopAtMs - millis());
    const uint32_t remainingFrames = remainingMs > 0
                                         ? static_cast<uint32_t>(remainingMs) * sampleRate_ / 1000U
                                         : 0U;
    for (size_t i = 0; i < BufferFrames; ++i) {
        int32_t amplitude = kToneAmplitude;
        if (outputFrames_ < fadeFrames) amplitude = amplitude * outputFrames_ / fadeFrames;
        if (remainingFrames <= fadeFrames + i) {
            const uint32_t left = remainingFrames > i ? remainingFrames - i : 0U;
            amplitude = amplitude * left / fadeFrames;
        }
        pcmBuffer_[i] = static_cast<int16_t>(
            static_cast<int32_t>(kSineLut[phase_ >> 26U]) * amplitude / 32767);
        phase_ += phaseStep;
        ++outputFrames_;
    }
    return writePcm(pcmBuffer_, BufferFrames);
}

bool AudioService::fadeToneToSilence() {
    if (!driverReady_ || !pcmBuffer_ || sampleRate_ == 0) return false;
    const uint32_t fadeFrames = max<uint32_t>(32U, sampleRate_ * kFadeMs / 1000UL);
    const uint32_t phaseStep = static_cast<uint32_t>(
        (static_cast<uint64_t>(kToneFrequencyHz) << 32U) / sampleRate_);
    uint32_t fadeOffset = 0;
    while (fadeOffset < fadeFrames) {
        const size_t frames = min<size_t>(BufferFrames, fadeFrames - fadeOffset);
        for (size_t frame = 0; frame < frames; ++frame) {
            const uint32_t remaining = fadeFrames - fadeOffset - frame;
            const int32_t amplitude = kToneAmplitude * remaining / fadeFrames;
            pcmBuffer_[frame] = static_cast<int16_t>(
                static_cast<int32_t>(kSineLut[phase_ >> 26U]) * amplitude / 32767);
            phase_ += phaseStep;
        }
        if (frames < BufferFrames) {
            memset(pcmBuffer_ + frames, 0, (BufferFrames - frames) * sizeof(int16_t));
        }
        if (!writePcm(pcmBuffer_, BufferFrames)) return false;
        fadeOffset += frames;
    }
    return true;
}

bool AudioService::writePcm(const int16_t* samples, size_t frameCount, uint32_t timeoutMs) {
    if (!driverReady_ || !samples || !frameCount) return false;
    const size_t totalBytes = frameCount * sizeof(int16_t);
    size_t totalWritten = 0;
    const uint32_t startedMs = millis();
    writing_ = true;
    while (totalWritten < totalBytes) {
        const uint32_t elapsedMs = millis() - startedMs;
        if (elapsedMs >= timeoutMs) {
            writing_ = false;
            return false;
        }

        size_t written = 0;
        const esp_err_t result = i2s_channel_write(
            txChannel_, reinterpret_cast<const uint8_t*>(samples) + totalWritten,
            totalBytes - totalWritten, &written, timeoutMs - elapsedMs);
        totalWritten += written;
        if (totalWritten == totalBytes) break;
        if (result != ESP_OK && result != ESP_ERR_TIMEOUT) {
            writing_ = false;
            return false;
        }
        ++writeRetries_;
        if (written == 0) taskYIELD();
    }
    writing_ = false;
    return true;
}

bool AudioService::preloadSilence(uint8_t bufferCount) {
    if (!txChannel_ || !pcmBuffer_ || bufferCount == 0) return false;
    memset(pcmBuffer_, 0, BufferFrames * sizeof(int16_t));
    bool loadedAny = false;
    for (uint8_t index = 0; index < bufferCount; ++index) {
        size_t loaded = 0;
        const esp_err_t result = i2s_channel_preload_data(
            txChannel_, pcmBuffer_, BufferFrames * sizeof(int16_t), &loaded);
        if (result != ESP_OK) return false;
        loadedAny = loadedAny || loaded > 0;
        if (loaded < BufferFrames * sizeof(int16_t)) break;
    }
    return loadedAny;
}

void AudioService::primeSilence(uint8_t bufferCount) {
    if (!driverReady_ || !pcmBuffer_) return;
    memset(pcmBuffer_, 0, BufferFrames * sizeof(int16_t));
    for (uint8_t index = 0; index < bufferCount; ++index) {
        if (!writePcm(pcmBuffer_, BufferFrames, 100)) break;
    }
}

void AudioService::settleToSilence() {
    if (!driverReady_ || !txChannel_ || sampleRate_ == 0) return;
    primeSilence();
    const uint32_t drainMs =
        (static_cast<uint32_t>(dmaBufferCount_ + 2U) * BufferFrames * 1000U + sampleRate_ - 1U) /
        sampleRate_;
    vTaskDelay(pdMS_TO_TICKS(drainMs + 2U));
}

void AudioService::finishPlayback(bool naturalEnd, bool interrupted) {
    closeWav();
    primeSilence();
    playing_ = false;
    bootAudioActive_ = false;
    vTaskPrioritySet(nullptr, TaskPriority);
    state().audioPlaying = false;
    state().activeSoundPath[0] = '\0';
    const char* status = naturalEnd ? "Playback complete"
                         : interrupted ? "Playback stopped"
                                       : "Playback failed";
    strlcpy(state().audioStatusText, status, sizeof(state().audioStatusText));
    if (!naturalEnd && !interrupted) Serial.println("[audio] playback failed");
}

bool AudioService::resolveScenarioPath(SoundScenario scenario, char path[65]) const {
    const uint8_t index = static_cast<uint8_t>(scenario);
    if (index >= enumCount(SoundScenario{}) || !storageReady_) return false;
    const AppSettings settings = settingsService().snapshot();
    const char* custom = settings.soundPath[index];
    if (custom[0] == '/' && SD_MMC.exists(custom)) {
        strlcpy(path, custom, 65);
        return true;
    }
    snprintf(path, 65, "/sounds/%s.wav", scenarioStem(scenario));
    if (SD_MMC.exists(path)) return true;
    snprintf(path, 65, "/%s.wav", scenarioStem(scenario));
    return SD_MMC.exists(path);
}

void AudioService::processPrinterSoundEvents() {
    const SystemState& system = state();
    if (bootExperience().active() || bootAudioActive_) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        pendingFinishSound_ = false;
        return;
    }

    if (pendingFinishSound_) {
        if (system.printerStateEventSequence != pendingFinishEventSequence_) {
            pendingFinishSound_ = false;
        } else if (static_cast<int32_t>(millis() - pendingFinishSoundDueMs_) >= 0) {
            pendingFinishSound_ = false;
            playScenario(SoundScenario::Finish);
            return;
        } else {
            return;
        }
    }
    if (system.printerStateEventSequence == observedPrinterEventSequence_) return;
    observedPrinterEventSequence_ = system.printerStateEventSequence;

    SoundScenario scenario = SoundScenario::Idle;
    bool shouldPlay = false;
    if (system.printerEventTo == PrinterState::Error) {
        scenario = SoundScenario::Error;
        shouldPlay = true;
    } else if (system.printerEventTo == PrinterState::Printing) {
        scenario = SoundScenario::Start;
        shouldPlay = true;
    } else if (system.printerEventTo == PrinterState::Paused) {
        scenario = SoundScenario::Pause;
        shouldPlay = true;
    } else if (system.printerEventTo == PrinterState::Complete) {
        const AppSettings settings = settingsService().snapshot();
        const uint8_t selectedPrintAnimation = normalizeLedAnimation(
            LedCategory::Print,
            settings.ledAnimation[static_cast<uint8_t>(LedCategory::Print)]);
        if (settings.ledEnabled && !settings.ledOtherMode &&
            (system.printerEventFrom == PrinterState::Printing ||
             system.printerEventFrom == PrinterState::Paused) &&
            selectedPrintAnimation == static_cast<uint8_t>(PrintAnimation::Snake)) {
            pendingFinishSound_ = true;
            pendingFinishEventSequence_ = system.printerStateEventSequence;
            pendingFinishSoundDueMs_ = millis() + SnakeFinishDurationMs;
            return;
        }
        scenario = SoundScenario::Finish;
        shouldPlay = true;
    } else if (system.printerEventTo == PrinterState::Idle) {
        scenario = SoundScenario::Idle;
        shouldPlay = true;
    }
    if (shouldPlay) playScenario(scenario);
}

void AudioService::logMemory(const char* tag) const {
    Serial.printf(
        "[audio-memory] %-13s internal=%lu largest=%lu dma=%lu largest=%lu psram=%lu largest=%lu\n",
        tag ? tag : "-",
        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)),
        static_cast<unsigned long>(ESP.getFreePsram()),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
}

}
