#include "AudioService.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../config/HardwareConfig.h"
#include "../core/SystemState.h"

namespace coronet {

namespace {

constexpr uint32_t kToneFrequencyHz = 660;
constexpr int32_t kToneAmplitude = 5200;

// One cycle in flash. DDS indexing avoids a floating-point sine calculation per sample.
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

const char* profileName(AudioDmaProfile profile) {
    return profile == AudioDmaProfile::Coronet1 ? "coronet1" : "balanced";
}

}

void AudioService::begin() {
    state().audioReady = false;

    pcmBuffer_ = static_cast<int16_t*>(heap_caps_calloc(
        BufferFrames, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pcmBuffer_) {
        Serial.println("[audio] PSRAM PCM allocation failed");
        return;
    }

    if (!installDriver(BalancedDmaBufferCount)) return;

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry,
        "coronet-audio",
        TaskStackBytes,
        this,
        TaskPriority,
        &task_,
        TaskCore);
    if (created != pdPASS) {
        Serial.println("[audio] task creation failed");
        uninstallDriver();
        return;
    }

    state().audioReady = true;
    Serial.println("[audio] ready; type 'audio test' to play the diagnostic tone");
    logStatus();
}

void AudioService::loop() {
    if (playing_ && static_cast<int32_t>(millis() - stopAtMs_) >= 0) stop();
}

bool AudioService::playTestTone(uint32_t durationMs) {
    if (!driverReady_ || !task_) {
        Serial.println("[audio] test unavailable: driver is not ready");
        return false;
    }

    if (durationMs < 250U) durationMs = 250U;
    if (durationMs > 60000U) durationMs = 60000U;
    phase_ = 0;
    outputFrames_ = 0;
    stopAtMs_ = millis() + durationMs;
    playing_ = true;
    xTaskNotifyGive(task_);
    Serial.printf("[audio] test tone started for %lums\n", static_cast<unsigned long>(durationMs));
    logMemory("playing");
    return true;
}

void AudioService::stop() {
    if (!playing_) return;
    playing_ = false;
    Serial.printf("[audio] stopped; writeFailures=%lu\n", static_cast<unsigned long>(writeFailures_));
    logMemory("stopped");
}

void AudioService::release() {
    stop();
    uninstallDriver();
    state().audioReady = false;
    logMemory("released");
}

bool AudioService::useDmaProfile(AudioDmaProfile profile) {
    const uint16_t requestedCount = profile == AudioDmaProfile::Coronet1
                                        ? Coronet1DmaBufferCount
                                        : BalancedDmaBufferCount;
    stop();
    uninstallDriver();
    profile_ = profile;
    if (!installDriver(requestedCount)) {
        state().audioReady = false;
        return false;
    }
    state().audioReady = task_ != nullptr;
    logStatus();
    return true;
}

bool AudioService::setSampleRate(uint32_t sampleRate) {
    if (sampleRate != 22050U && sampleRate != 44100U && sampleRate != 48000U) {
        Serial.println("[audio] supported diagnostic rates: 22050, 44100, 48000");
        return false;
    }
    if (!driverReady_ || !txChannel_) return false;

    stop();
    const uint32_t waitStartedMs = millis();
    while (writing_ && millis() - waitStartedMs < 200U) vTaskDelay(pdMS_TO_TICKS(1));

    esp_err_t result = i2s_channel_disable(txChannel_);
    i2s_std_clk_config_t clockConfig = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    if (result == ESP_OK) result = i2s_channel_reconfig_std_clock(txChannel_, &clockConfig);
    if (result == ESP_OK) result = i2s_channel_enable(txChannel_);
    if (result != ESP_OK) {
        Serial.printf("[audio] sample-rate change failed: %d\n", static_cast<int>(result));
        state().audioReady = false;
        return false;
    }

    sampleRate_ = sampleRate;
    state().audioReady = true;
    logStatus();
    return true;
}

void AudioService::logStatus() const {
    Serial.printf(
        "[audio] ready=%u playing=%u profile=%s format=mono/16-bit/%luHz dma=%ux%u (%lu PCM bytes, %.1fms) failures=%lu\n",
        driverReady_ ? 1U : 0U,
        playing_ ? 1U : 0U,
        profileName(profile_),
        static_cast<unsigned long>(sampleRate_),
        static_cast<unsigned>(dmaBufferCount_),
        static_cast<unsigned>(BufferFrames),
        static_cast<unsigned long>(dmaBufferCount_ * BufferFrames * sizeof(int16_t)),
        dmaBufferCount_ * BufferFrames * 1000.0 / sampleRate_,
        static_cast<unsigned long>(writeFailures_));
    logMemory("status");
}

void AudioService::taskEntry(void* context) {
    static_cast<AudioService*>(context)->taskLoop();
}

void AudioService::taskLoop() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (playing_) {
            if (static_cast<int32_t>(millis() - stopAtMs_) >= 0) {
                playing_ = false;
                break;
            }
            if (!writeToneBuffer()) {
                ++writeFailures_;
                playing_ = false;
                break;
            }
        }
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
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(hw::I2sMckPin),
            .bclk = static_cast<gpio_num_t>(hw::I2sBckPin),
            .ws = static_cast<gpio_num_t>(hw::I2sLrckPin),
            .dout = static_cast<gpio_num_t>(hw::I2sDoutPin),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    result = i2s_channel_init_std_mode(txChannel_, &standardConfig);
    if (result == ESP_OK) result = i2s_channel_enable(txChannel_);
    if (result != ESP_OK) {
        Serial.printf("[audio] I2S channel setup failed: %d\n", static_cast<int>(result));
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
    if (!driverReady_) return;
    playing_ = false;
    const uint32_t waitStartedMs = millis();
    while (writing_ && millis() - waitStartedMs < 200U) vTaskDelay(pdMS_TO_TICKS(1));
    i2s_channel_disable(txChannel_);
    i2s_del_channel(txChannel_);
    txChannel_ = nullptr;
    driverReady_ = false;
    dmaBufferCount_ = 0;
}

bool AudioService::writeToneBuffer() {
    if (!driverReady_ || !pcmBuffer_) return false;

    const uint32_t phaseStep = static_cast<uint32_t>(
        (static_cast<uint64_t>(kToneFrequencyHz) << 32U) / sampleRate_);
    const uint32_t fadeFrames = sampleRate_ / 50U;
    const int32_t remainingMs = static_cast<int32_t>(stopAtMs_ - millis());
    const uint32_t remainingFrames = remainingMs > 0
                                         ? static_cast<uint32_t>(remainingMs) * sampleRate_ / 1000U
                                         : 0U;

    for (size_t i = 0; i < BufferFrames; ++i) {
        int32_t amplitude = kToneAmplitude;
        if (outputFrames_ < fadeFrames) {
            amplitude = (amplitude * static_cast<int32_t>(outputFrames_)) / static_cast<int32_t>(fadeFrames);
        }
        if (remainingFrames <= fadeFrames + i) {
            const uint32_t fadeRemaining = remainingFrames > i ? remainingFrames - i : 0U;
            amplitude = (amplitude * static_cast<int32_t>(fadeRemaining)) / static_cast<int32_t>(fadeFrames);
        }
        const uint8_t lutIndex = static_cast<uint8_t>(phase_ >> 26U);
        pcmBuffer_[i] = static_cast<int16_t>((static_cast<int32_t>(kSineLut[lutIndex]) * amplitude) / 32767);
        phase_ += phaseStep;
        ++outputFrames_;
    }

    size_t bytesWritten = 0;
    writing_ = true;
    const esp_err_t result = i2s_channel_write(
        txChannel_,
        pcmBuffer_,
        BufferFrames * sizeof(int16_t),
        &bytesWritten,
        150);
    writing_ = false;
    return result == ESP_OK && bytesWritten == BufferFrames * sizeof(int16_t);
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
