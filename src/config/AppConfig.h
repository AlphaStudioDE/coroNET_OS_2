#pragma once

#include <Arduino.h>

namespace coronet::config {

static constexpr const char* AppName = "coroNET";
static constexpr const char* FirmwareName = "coroNET OS 2";
static constexpr const char* FirmwareVersion = "0.0.1-dev";
static constexpr const char* GitHubLatestReleaseApi =
    "https://api.github.com/repos/AlphaStudioDE/coroNET_OS_2/releases/latest";

static constexpr uint32_t SerialBaud = 115200;
static constexpr uint32_t ExpectedCpuFrequencyMhz = 240;
static constexpr uint32_t ExpectedFlashFrequencyHz = 80000000;
static constexpr uint32_t ExpectedFlashSizeBytes = 16U * 1024U * 1024U;
static constexpr uint32_t ExpectedPsramSizeBytes = 8U * 1024U * 1024U;
static constexpr uint32_t HealthLogIntervalMs = 5000;
static constexpr uint32_t HealthSampleIntervalMs = 500;
static constexpr uint32_t BleStateNotifyIntervalMs = 1000;
static constexpr uint32_t BleWifiFallbackDelayMs = 45000;
static constexpr uint32_t SettingsSaveDebounceMs = 1500;
static constexpr uint32_t SettingsSaveMaxDelayMs = 5000;
static constexpr size_t WebMaxJsonBodyBytes = 4096;
static constexpr size_t PsramMallocThresholdBytes = 256;
static constexpr size_t DeviceIdLength = 12;
static constexpr size_t DeviceNameMaxLength = 24;

static constexpr const char* BleServiceUuid = "7b7e0001-9f2a-4f3c-8d2a-c0a0e7c0ffee";
static constexpr const char* BleStateUuid = "7b7e0002-9f2a-4f3c-8d2a-c0a0e7c0ffee";
static constexpr const char* BleCommandUuid = "7b7e0003-9f2a-4f3c-8d2a-c0a0e7c0ffee";
static constexpr const char* BleEventUuid = "7b7e0004-9f2a-4f3c-8d2a-c0a0e7c0ffee";

}
