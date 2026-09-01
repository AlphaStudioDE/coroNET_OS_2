#pragma once

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace coronet {

enum class AudioDmaProfile : uint8_t {
    Balanced,
    Coronet1,
};

class AudioService {
public:
    void begin();
    void loop();
    bool playTestTone(uint32_t durationMs = 8000);
    void stop();
    void release();
    bool useDmaProfile(AudioDmaProfile profile);
    bool setSampleRate(uint32_t sampleRate);
    void logStatus() const;

private:
    static constexpr uint32_t DefaultSampleRate = 22050;
    static constexpr size_t BufferFrames = 128;
    static constexpr uint16_t BalancedDmaBufferCount = 16;
    static constexpr uint16_t Coronet1DmaBufferCount = 48;
    static constexpr uint32_t TaskStackBytes = 3072;
    static constexpr UBaseType_t TaskPriority = 6;
    static constexpr BaseType_t TaskCore = 0;

    static void taskEntry(void* context);
    void taskLoop();
    bool installDriver(uint16_t dmaBufferCount);
    void uninstallDriver();
    bool writeToneBuffer();
    void logMemory(const char* tag) const;

    TaskHandle_t task_ = nullptr;
    i2s_chan_handle_t txChannel_ = nullptr;
    int16_t* pcmBuffer_ = nullptr;
    volatile bool playing_ = false;
    volatile bool writing_ = false;
    volatile uint32_t stopAtMs_ = 0;
    bool driverReady_ = false;
    AudioDmaProfile profile_ = AudioDmaProfile::Balanced;
    uint16_t dmaBufferCount_ = 0;
    uint32_t sampleRate_ = DefaultSampleRate;
    uint32_t phase_ = 0;
    uint32_t outputFrames_ = 0;
    uint32_t writeFailures_ = 0;
};

}
