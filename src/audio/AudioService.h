#pragma once

#include <Arduino.h>
#include <FS.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../core/SystemState.h"

namespace coronet {

enum class AudioDmaProfile : uint8_t { Balanced, Coronet1 };

class AudioService {
public:
    void begin();
    void loop();
    bool playTestTone(uint32_t durationMs = 8000);
    bool playFile(const char* path, uint8_t volumePercent = 75, bool repeat = false,
                  SoundScenario scenario = SoundScenario::Start, bool bootAudio = false);
    bool playScenario(SoundScenario scenario);
    void stop();
    void release();
    bool useDmaProfile(AudioDmaProfile profile);
    bool setSampleRate(uint32_t sampleRate);
    bool mountStorage();
    bool refreshFileIndex();
    uint8_t fileCount() const { return fileCount_; }
    const char* filePath(uint8_t index) const;
    bool bootAudioActive() const { return bootAudioActive_; }
    uint32_t playbackStartedMs() const { return playbackStartedMs_; }
    void logStatus() const;

private:
    enum class RequestType : uint8_t { Stop, Tone, Wav };

    struct WavInfo {
        uint32_t dataOffset = 0;
        uint32_t dataSize = 0;
        uint32_t dataRemaining = 0;
        uint32_t sampleRate = 0;
        uint32_t outputFrames = 0;
        uint16_t channels = 0;
        uint16_t bitsPerSample = 0;
    };

    static constexpr uint32_t DefaultSampleRate = 22050;
    static constexpr size_t BufferFrames = 128;
    static constexpr size_t RawBufferBytes = BufferFrames * 4;
    static constexpr uint16_t BalancedDmaBufferCount = 16;
    static constexpr uint16_t Coronet1DmaBufferCount = 48;
    static constexpr uint32_t TaskStackBytes = 4096;
    static constexpr UBaseType_t TaskPriority = 7;
    static constexpr UBaseType_t BootTaskPriority = 20;
    static constexpr BaseType_t TaskCore = 0;
    static constexpr uint8_t MaxIndexedFiles = 64;

    static void taskEntry(void* context);
    void taskLoop();
    bool installDriver(uint16_t dmaBufferCount);
    void uninstallDriver();
    bool reconfigureClock(uint32_t sampleRate);
    bool openWav(const char* path);
    void closeWav();
    bool writeWavBuffer(uint8_t volumePercent);
    bool writeToneBuffer(uint32_t stopAtMs);
    bool writePcm(const int16_t* samples, size_t frameCount, uint32_t timeoutMs = 150);
    void primeSilence();
    void finishPlayback(bool naturalEnd);
    uint32_t submitRequest(RequestType type, const char* path, uint8_t volumePercent,
                           bool repeat, SoundScenario scenario, bool bootAudio,
                           uint32_t durationMs = 0);
    bool stopAndWait(uint32_t timeoutMs = 1000);
    void snapshotRequest(uint32_t& sequence, RequestType& type, char path[65],
                         uint8_t& volumePercent, bool& repeat, SoundScenario& scenario,
                         bool& bootAudio, uint32_t& durationMs);
    void processPrinterSoundEvents();
    bool resolveScenarioPath(SoundScenario scenario, char path[65]) const;
    void logMemory(const char* tag) const;
    void indexDirectory(const char* path);
    bool addIndexedFile(const char* path);
    void validateAssets();

    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t txChannel_ = nullptr;
    int16_t* pcmBuffer_ = nullptr;
    uint8_t* rawBuffer_ = nullptr;
    char (*fileIndex_)[65] = nullptr;
    uint8_t fileCount_ = 0;
    File wavFile_;
    WavInfo wav_;
    portMUX_TYPE requestMux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t requestSequence_ = 0;
    volatile uint32_t completedRequestSequence_ = 0;
    RequestType requestedType_ = RequestType::Stop;
    char requestedPath_[65] = "";
    uint8_t requestedVolume_ = 75;
    bool requestedRepeat_ = false;
    SoundScenario requestedScenario_ = SoundScenario::Start;
    bool requestedBootAudio_ = false;
    uint32_t requestedDurationMs_ = 0;
    volatile bool playing_ = false;
    volatile bool writing_ = false;
    volatile bool bootAudioActive_ = false;
    volatile uint32_t playbackStartedMs_ = 0;
    volatile bool taskInitDone_ = false;
    volatile bool taskInitOk_ = false;
    bool driverReady_ = false;
    bool storageReady_ = false;
    AudioDmaProfile profile_ = AudioDmaProfile::Balanced;
    uint16_t dmaBufferCount_ = 0;
    uint32_t sampleRate_ = DefaultSampleRate;
    uint32_t phase_ = 0;
    uint32_t outputFrames_ = 0;
    uint32_t writeFailures_ = 0;
    uint32_t completedFiles_ = 0;
    uint32_t observedPrinterEventSequence_ = 0;
};

AudioService& audioService();

}
