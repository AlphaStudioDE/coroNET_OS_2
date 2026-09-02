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
    const AppSettings& settings = settingsService().settings();
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

}

AudioService& audioService() {
    return gAudioService;
}

void AudioService::begin() {
    state().audioReady = false;
    state().audioPlaying = false;
    state().sdReady = false;

    pcmBuffer_ = static_cast<int16_t*>(heap_caps_calloc(
        BufferFrames, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    rawBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(
        RawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    fileIndex_ = static_cast<char (*)[65]>(heap_caps_calloc(
        MaxIndexedFiles, 65, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pcmBuffer_ || !rawBuffer_ || !fileIndex_) {
        Serial.println("[audio] PSRAM buffer allocation failed");
        return;
    }

    mountStorage();

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry, "coronet-audio", TaskStackBytes, this, TaskPriority, &task_, TaskCore);
    if (created != pdPASS) {
        Serial.println("[audio] task creation failed");
        task_ = nullptr;
        return;
    }

    const uint32_t initStarted = millis();
    while (!taskInitDone_ && millis() - initStarted < 2000U) vTaskDelay(pdMS_TO_TICKS(1));
    if (!taskInitDone_ || !taskInitOk_) {
        Serial.println("[audio] Core 0 I2S initialization failed or timed out");
        return;
    }

    state().audioReady = true;
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
    if (storageReady_) return true;
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
    return false;
}

const char* AudioService::filePath(uint8_t index) const {
    return fileIndex_ && index < fileCount_ ? fileIndex_[index] : nullptr;
}

bool AudioService::refreshFileIndex() {
    if (!storageReady_ || !fileIndex_ || playing_) return false;
    fileCount_ = 0;
    memset(fileIndex_, 0, MaxIndexedFiles * 65U);
    indexDirectory("/");
    indexDirectory("/sounds");
    state().audioFileCount = fileCount_;
    validateAssets();
    Serial.printf("[audio] indexed %u WAV files; assets=%s\n",
                  static_cast<unsigned>(fileCount_), state().audioAssetStatus);
    return true;
}

void AudioService::indexDirectory(const char* path) {
    if (fileCount_ >= MaxIndexedFiles) return;
    File directory = SD_MMC.open(path);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return;
    }
    File entry = directory.openNextFile();
    while (entry && fileCount_ < MaxIndexedFiles) {
        if (!entry.isDirectory()) {
            String entryPath = entry.path();
            String lower = entryPath;
            lower.toLowerCase();
            if (lower.endsWith(".wav")) addIndexedFile(entryPath.c_str());
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
}

bool AudioService::addIndexedFile(const char* path) {
    if (!path || !path[0] || fileCount_ >= MaxIndexedFiles) return false;
    char normalized[65] = "/";
    if (path[0] == '/') strlcpy(normalized, path, sizeof(normalized));
    else strlcpy(normalized + 1, path, sizeof(normalized) - 1U);
    for (uint8_t i = 0; i < fileCount_; ++i) {
        if (strcasecmp(fileIndex_[i], normalized) == 0) return false;
    }
    strlcpy(fileIndex_[fileCount_++], normalized, 65);
    return true;
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
        return false;
    }
    submitRequest(RequestType::Wav, path, constrain(volumePercent, 0, 100), repeat,
                  scenario, bootAudio);
    return true;
}

bool AudioService::playScenario(SoundScenario scenario) {
    const uint8_t index = static_cast<uint8_t>(scenario);
    if (index >= enumCount(SoundScenario{})) return false;
    if (quietSuppressesSound(scenario)) return false;
    char path[65] = "";
    if (!resolveScenarioPath(scenario, path)) return false;
    const AppSettings& settings = settingsService().settings();
    return playFile(path, settings.soundVolume[index], settings.soundRepeat[index], scenario, false);
}

void AudioService::stop() {
    if (!task_) return;
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
        "[audio] ready=%u sd=%u playing=%u boot=%u profile=%s mono/16-bit/%luHz dma=%ux%u failures=%lu files=%lu stackHeadroom=%uB path=%s\n",
        driverReady_ ? 1U : 0U, storageReady_ ? 1U : 0U, playing_ ? 1U : 0U,
        bootAudioActive_ ? 1U : 0U, profileName(profile_), static_cast<unsigned long>(sampleRate_),
        static_cast<unsigned>(dmaBufferCount_), static_cast<unsigned>(BufferFrames),
        static_cast<unsigned long>(writeFailures_), static_cast<unsigned long>(completedFiles_),
        static_cast<unsigned>(stackHeadroom),
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
            while (requestSequence_ == sequence && static_cast<int32_t>(millis() - stopAt) < 0) {
                if (!writeToneBuffer(stopAt)) {
                    ++writeFailures_;
                    break;
                }
            }
            finishPlayback(true);
            completedRequestSequence_ = sequence;
            continue;
        }

        strlcpy(state().activeSoundPath, path, sizeof(state().activeSoundPath));
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
            closeWav();
            if (naturalEnd) ++completedFiles_;
        } while (repeat && naturalEnd && requestSequence_ == sequence);
        finishPlayback(naturalEnd);
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
    i2s_channel_disable(txChannel_);
    i2s_del_channel(txChannel_);
    txChannel_ = nullptr;
    driverReady_ = false;
    dmaBufferCount_ = 0;
}

bool AudioService::reconfigureClock(uint32_t sampleRate) {
    if (!driverReady_ || !txChannel_ || sampleRate < 8000U || sampleRate > 48000U) return false;
    if (sampleRate_ == sampleRate) return true;
    esp_err_t result = i2s_channel_disable(txChannel_);
    i2s_std_clk_config_t clockConfig = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    if (result == ESP_OK) result = i2s_channel_reconfig_std_clock(txChannel_, &clockConfig);
    if (result == ESP_OK) result = i2s_channel_enable(txChannel_);
    if (result != ESP_OK) return false;
    sampleRate_ = sampleRate;
    primeSilence();
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

bool AudioService::writeWavBuffer(uint8_t volumePercent) {
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
        pcmBuffer_[frame] = static_cast<int16_t>(constrain(sample, -32768, 32767));
    }
    wav_.outputFrames += frames;
    if (frames < BufferFrames) memset(pcmBuffer_ + frames, 0, (BufferFrames - frames) * sizeof(int16_t));
    return writePcm(pcmBuffer_, BufferFrames);
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

bool AudioService::writePcm(const int16_t* samples, size_t frameCount, uint32_t timeoutMs) {
    if (!driverReady_ || !samples || !frameCount) return false;
    size_t written = 0;
    writing_ = true;
    const esp_err_t result = i2s_channel_write(
        txChannel_, samples, frameCount * sizeof(int16_t), &written, timeoutMs);
    writing_ = false;
    return result == ESP_OK && written == frameCount * sizeof(int16_t);
}

void AudioService::primeSilence() {
    if (!driverReady_ || !pcmBuffer_) return;
    memset(pcmBuffer_, 0, BufferFrames * sizeof(int16_t));
    writePcm(pcmBuffer_, BufferFrames, 100);
}

void AudioService::finishPlayback(bool naturalEnd) {
    closeWav();
    primeSilence();
    playing_ = false;
    bootAudioActive_ = false;
    vTaskPrioritySet(nullptr, TaskPriority);
    state().audioPlaying = false;
    state().activeSoundPath[0] = '\0';
    if (!naturalEnd) Serial.println("[audio] playback stopped or failed");
}

bool AudioService::resolveScenarioPath(SoundScenario scenario, char path[65]) const {
    const uint8_t index = static_cast<uint8_t>(scenario);
    if (index >= enumCount(SoundScenario{}) || !storageReady_) return false;
    const char* custom = settingsService().settings().soundPath[index];
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
        const AppSettings& settings = settingsService().settings();
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
