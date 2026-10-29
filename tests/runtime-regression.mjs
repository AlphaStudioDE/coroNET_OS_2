// Runs production C++ functions against deterministic hardware/RTOS substitutes.
// Run: node tests/runtime-regression.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8').replaceAll('\r\n', '\n');
const stripIncludes = text => text.replace(/^#(?:include|pragma).*\n/gm, '');
function method(file, signature) {
  const source = read(file);
  const start = source.indexOf(signature);
  const end = source.indexOf('\n}\n', start);
  if (start < 0 || end < 0) throw new Error(`Method missing: ${signature}`);
  return source.slice(start, end + 2);
}
const settings = stripIncludes(read('src/settings/SettingsService.h').split('class SettingsService')[0]) + '\n}\n';
const state = stripIncludes(read('src/core/SystemState.h').split('SystemState& state();')[0]) + '\n}\n';
const pandaHeader = stripIncludes(read('src/panda/PandaBreathService.h')).replace('private:', 'public:');
const pandaMethods = [
  'void PandaBreathService::configureFromSettings()',
  'void PandaBreathService::service()',
  'void PandaBreathService::updateWorkflow(uint32_t now)',
  'void PandaBreathService::setPhase(',
  'void PandaBreathService::requestOff()',
  'void PandaBreathService::requestControlledHeat(',
  'void PandaBreathService::requestDry(',
  'bool PandaBreathService::controlTemperature(',
  'void PandaBreathService::finishOneShotMode(',
  'PandaBreathService::DryProfile PandaBreathService::dryProfile()',
  'uint8_t PandaBreathService::temperingTarget(',
  'void PandaBreathService::normalizeHost(',
].map(signature => method('src/panda/PandaBreathService.cpp', signature)).join('\n\n');
const realtime = method('src/printer/PrinterService.cpp', 'void PrinterService::applyRealtimeStatus(');
const chamberStart = realtime.indexOf('    if (workerChamberObject_[0]) {');
const chamberEnd = realtime.indexOf('\n    }', chamberStart) + 6;
if (chamberStart < 0 || chamberEnd < 6) throw Error('Chamber block missing');
const printerLoop = method('src/printer/PrinterService.cpp', 'void PrinterService::loop()');
const ageStart = printerLoop.indexOf('    const uint32_t measurementNow');
const ageEnd = printerLoop.indexOf('\n    const bool setupDone', ageStart);
if (ageStart < 0 || ageEnd < 0) throw Error('Measurement expiry missing');
const source = String.raw`
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <ArduinoJson.h>
using std::min;
using std::isnan;
using std::isfinite;
uint32_t fakeNow = 3600000U;
uint32_t millis() { return fakeNow; }
template<class T> T constrain(T v, T lo, T hi) { return std::clamp(v, lo, hi); }
size_t strlcpy(char* dst, const char* src, size_t size) {
  const size_t n = strlen(src);
  if (size) { const size_t count = std::min(n, size - 1); memcpy(dst, src, count); dst[count] = 0; }
  return n;
}
using WStype_t = int;
using TaskHandle_t = void*;
std::function<void()> socketHook, delayHook;
int socketLoops = 0, disconnects = 0, desiredSends = 0, connects = 0;
struct WebSocketsClient { void loop() { ++socketLoops; if (socketHook) socketHook(); } };
constexpr int WL_CONNECTED = 1;
struct { int status() { return WL_CONNECTED; } } WiFi;
${stripIncludes(read('src/core/ProductTypes.h'))}
${settings}
${state}
namespace coronet {
SystemState currentState;
SystemState stateSnapshot() { return currentState; }
template<class F> void updateState(F f) { f(currentState); }
struct FakeSettings {
  AppSettings value;
  uint32_t rev = 1;
  AppSettings settings() const { return value; }
  uint32_t revision() const { return rev; }
  template<class F> void update(F f) { f(value); ++rev; }
  void flush() {}
} fakeSettings;
FakeSettings& settingsService() { return fakeSettings; }
}
${pandaHeader}
namespace coronet {
constexpr uint32_t kTemperatureStaleMs = 10000;
constexpr uint32_t kWorkflowIntervalMs = 500;
constexpr float kHeatOnMarginC = 0.6f;
constexpr float kHeatOffMarginC = 0.2f;
constexpr float kPreheatReadyMarginC = 0.5f;
void PandaBreathService::disconnect() { ++disconnects; socketConfigured_ = false; connected_ = false; }
void PandaBreathService::connectIfNeeded() { ++connects; socketConfigured_ = true; }
void PandaBreathService::performDiscovery() {}
void PandaBreathService::sendDesired(bool) { ++desiredSends; }
${pandaMethods}

constexpr uint32_t kChamberFilterResetGapMs = 30000;
constexpr uint32_t kChamberFilterTimeConstantMs = 6000;
struct PrinterService {
  float workerChamberFilteredC_ = NAN;
  uint32_t workerChamberFilterMs_ = 0;
  float filterChamberTemperature(float);
  void resetChamberFilter();
};
${method('src/printer/PrinterService.cpp', 'void PrinterService::resetChamberFilter(')}
${method('src/printer/PrinterService.cpp', 'float PrinterService::filterChamberTemperature(')}
struct DeltaPrinter : PrinterService {
  char workerChamberObject_[32] = "temperature_sensor chamber";
  struct { float chamberTempC = 25; uint32_t chamberSampleMs = 123; } workerSnapshot_;
  void apply(JsonVariantConst status) {
${realtime.slice(chamberStart,chamberEnd)}
  }
};
constexpr uint32_t kChamberMeasurementStaleMs = 65000;
void expireMeasurement() {
${printerLoop.slice(ageStart,ageEnd)}
}

struct DisplayService {
  bool wizardActive_ = false;
  uint32_t observedPrinterEventSequence_ = 0;
  bool screenSaverActive_ = true;
  uint32_t screenSaverActivityMark_ = 100;
  bool screenSaverClock_ = true;
  int wakeCalls = 0;
  void updateScreenSaver(uint32_t now);
  void leaveScreenSaver(bool) { ++wakeCalls; screenSaverActive_ = false; }
  void enterScreenSaver() { screenSaverActive_ = true; }
};
${method('src/display/DisplayService.cpp', 'void DisplayService::updateScreenSaver(')}

struct FakeFile {
  std::vector<uint8_t> bytes;
  uint32_t pos = 0;
  unsigned reads = 0;
  bool failSeek = false, stalledSeek = false;
  explicit operator bool() const { return !bytes.empty(); }
  bool isDirectory() const { return false; }
  bool available() const { return pos < bytes.size(); }
  uint32_t position() const { return pos; }
  size_t size() const { return bytes.size(); }
  size_t read(uint8_t* dst, size_t wanted) {
    if (++reads > 20) throw std::runtime_error("Repeated WAV chunk reads");
    const size_t n = std::min(wanted, bytes.size() - pos);
    memcpy(dst, bytes.data() + pos, n); pos += n; return n;
  }
  bool seek(uint32_t target) { if (failSeek || target > bytes.size()) return false; if (!stalledSeek) pos = target; return true; }
  void close() { bytes.clear(); }
};
struct FakeSD { FakeFile input; FakeFile open(const char*, int) { return input; } void end() {} } SD_MMC;
constexpr int FILE_READ = 0;
constexpr uint8_t kGuardSilenceBuffers = 2;
struct FakeSerial { template<class... T> void printf(const char*, T...) {} void println(const char*) {} } Serial;
${method('src/audio/AudioService.cpp', 'uint16_t readLe16(')}
${method('src/audio/AudioService.cpp', 'uint32_t readLe32(')}
struct AudioService {
  FakeFile wavFile_;
  struct {
    uint32_t dataOffset = 0, dataSize = 0, dataRemaining = 0, sampleRate = 0;
    uint32_t outputFrames = 0, outputFramesTotal = 0;
    uint16_t channels = 0, bitsPerSample = 0;
  } wav_;
  bool openWav(const char*, uint32_t);
  unsigned checks = 0, cancelAfter = 1000, clockChanges = 0;
  bool requestIsCurrent(uint32_t seq) { return seq == 1 && ++checks < cancelAfter; }
  void closeWav() { wavFile_.close(); wav_ = {}; }
  bool reconfigureClock(uint32_t) { ++clockChanges; return true; }
  void primeSilence(uint8_t) {}
};
${method('src/audio/AudioService.cpp', 'bool AudioService::openWav(')}

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1;
struct i2s_std_clk_config_t { uint32_t rate; };
i2s_std_clk_config_t I2S_STD_CLK_DEFAULT_CONFIG(uint32_t rate) { return {rate}; }
bool fakeI2sEnabled = true;
esp_err_t i2s_channel_disable(void*) { fakeI2sEnabled = false; return ESP_OK; }
esp_err_t i2s_channel_reconfig_std_clock(void*, const i2s_std_clk_config_t*) { return ESP_OK; }
esp_err_t i2s_channel_enable(void*) { fakeI2sEnabled = true; return ESP_OK; }
esp_err_t i2s_del_channel(void*) { return ESP_OK; }
struct ClockAudioService {
  bool driverReady_ = true;
  void* txChannel_ = this;
  uint32_t sampleRate_ = 48000;
  uint8_t dmaBufferCount_ = 24;
  bool preloadOk = false;
  void settleToSilence() {}
  bool preloadSilence(uint8_t) { return preloadOk; }
  bool reconfigureClock(uint32_t sampleRate);
};
${method('src/audio/AudioService.cpp', 'bool AudioService::reconfigureClock(').replace('AudioService::reconfigureClock', 'ClockAudioService::reconfigureClock')}

// Keep the production worker loop; replace only RTOS, file and I2S endpoints.
constexpr bool pdTRUE = true;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
uint32_t pdMS_TO_TICKS(uint32_t n) { return n; }
void vTaskDelay(uint32_t ticks) { fakeNow += ticks; if (delayHook) delayHook(); }
void vTaskPrioritySet(void*, int) {}
void vTaskDelete(void*) { throw std::runtime_error("Task deleted"); }
const char* scenarioStem(SoundScenario) { return "test"; }
const char* pathLeaf(const char* p) { return p; }
struct WorkerAudioService;
WorkerAudioService* workerUnderTest = nullptr;
uint32_t ulTaskNotifyTake(bool, uint32_t);
struct WorkerAudioService {
  enum class RequestType { Stop, Tone, Wav, Scenario, RescanStorage };
  static constexpr int TaskPriority = 18, BootTaskPriority = 22, BalancedDmaBufferCount = 24, Coronet1DmaBufferCount = 48;
  enum class AudioDmaProfile { Balanced, Coronet1 };
  AudioDmaProfile profile_ = AudioDmaProfile::Balanced;
  bool driverReady_ = true;
  void* txChannel_ = this;
  bool taskInitOk_ = false, taskInitDone_ = false, playing_ = false, bootAudioActive_ = false;
  void* task_ = nullptr;
  uint32_t playbackStartedMs_ = 0, phase_ = 0, outputFrames_ = 0, writeFailures_ = 0, completedFiles_ = 0;
  struct { uint32_t outputFrames = 0, outputFramesTotal = 0; } wav_;
  uint32_t sequence = 0, notifications = 0, newTrackStarts = 0;
  void taskLoop();
  bool installDriver(int) { return true; }
  void uninstallDriver() { txChannel_ = nullptr; }
  uint32_t requestSequenceSnapshot() { return sequence; }
  void snapshotRequest(uint32_t& seq, RequestType& type, char* p, uint8_t& volume,
      bool& repeat, SoundScenario& scenario, bool& boot, uint32_t& duration) {
    seq = sequence; type = RequestType::Wav; strlcpy(p, seq == 1 ? "/old.wav" : "/new.wav", 65);
    volume = 75; repeat = false; scenario = SoundScenario::Start; boot = false; duration = 0;
  }
  void closeWav() { wav_ = {}; }
  void primeSilence() {}
  void markRequestCompleted(uint32_t) {}
  void setStorageReady(bool) {}
  bool mountStorage() { return true; }
  bool storageReadySnapshot() { return true; }
  bool resolveScenarioPathOnStorage(SoundScenario, char*) { return true; }
  bool requestIsCurrent(uint32_t seq) { return seq == sequence; }
  bool writeToneBuffer(uint32_t) { return true; }
  bool fadeToneToSilence() { return true; }
  bool fadeWavToSilence(uint8_t) { return true; }
  void finishPlayback(bool, bool) { closeWav(); }
  bool openWav(const char* p, uint32_t) {
    if (strcmp(p, "/new.wav") == 0 && ++newTrackStarts == 2)
      throw std::runtime_error("Unexpected repeated track");
    wav_.outputFramesTotal = 1; wav_.outputFrames = 0; return true;
  }
  bool writeWavBuffer(uint8_t) {
    ++wav_.outputFrames;
    if (sequence == 1) { sequence = 2; ++notifications; }
    return true;
  }
};
uint32_t ulTaskNotifyTake(bool, uint32_t timeout) {
  auto& worker = *workerUnderTest;
  if (!worker.sequence && timeout) { worker.sequence = 1; return 1; }
  if (!worker.notifications) {
    if (timeout) throw std::runtime_error("Worker correctly idle");
    return 0;
  }
  const uint32_t count = worker.notifications; worker.notifications = 0; return count;
}
${method('src/audio/AudioService.cpp', 'void AudioService::taskLoop()').replace('AudioService::taskLoop', 'WorkerAudioService::taskLoop')}

PandaBreathService* activePanda = nullptr;
PandaBreathService& pandaBreathService() { return *activePanda; }
struct { void cancelPreview() {} } fakeLed;
auto& ledService() { return fakeLed; }
struct { void applyNow() {} } fakeVent;
auto& ventService() { return fakeVent; }
struct { unsigned releases = 0; bool release() { ++releases; return true; } } fakeAudio;
auto& audioService() { return fakeAudio; }
constexpr int MALLOC_CAP_DMA = 1, MALLOC_CAP_INTERNAL = 2, MALLOC_CAP_8BIT = 4;
unsigned heap_caps_get_free_size(int) { return 100000; }
unsigned heap_caps_get_largest_free_block(int) { return 80000; }
struct OtaService {
  bool audioWasReady_ = false;
  OtaState result = OtaState::Idle;
  void setState(OtaState state, const char*) { result = state; }
  bool enterMaintenance();
};
${method('src/update/OtaService.cpp', 'bool OtaService::enterMaintenance()')}
}

int main() {
  using namespace coronet;
  int passed = 0;
  // Finish a normal preheat first, then enable manual tempering at 1h uptime.
  PandaBreathService panda;
  fakeSettings.value.pandaEnabled = true;
  fakeSettings.value.pandaMode = PandaBreathMode::PreheatHold;
  panda.configureFromSettings();
  panda.finishOneShotMode(PandaWorkflowPhase::Complete, "Preheat hold complete");
  panda.configureFromSettings();
  fakeSettings.value.pandaMode = PandaBreathMode::Tempering;
  fakeSettings.value.pandaEnabled = true;
  panda.configureFromSettings();
  currentState.printerConnected = currentState.printerTelemetryValid = true;
  currentState.lastPrinterUpdateMs = fakeNow;
  currentState.printerState = PrinterState::Idle;
  currentState.chamberTempC = 40.0f;
  panda.updateWorkflow(fakeNow);
  const bool cancelled = !fakeSettings.value.pandaEnabled &&
      currentState.pandaPhase == PandaWorkflowPhase::Complete;
  assert(!cancelled && fakeSettings.value.pandaEnabled);
  assert(panda.temperingInitialized_ && panda.phaseStartedMs_ == fakeNow);
  assert(currentState.pandaPhase == PandaWorkflowPhase::Tempering);
  std::cout << "PASS: manual tempering starts a fresh timer after Complete\n";
  ++passed;

  // A reconnect baseline may report Error without creating a new event.
  currentState = SystemState{};
  currentState.printerTelemetryValid = true;
  currentState.printerState = PrinterState::Error;
  currentState.lastTouchMs = 200; // User has already released the wake touch.
  DisplayService display;
  display.updateScreenSaver(fakeNow);
  const bool trapped = display.screenSaverActive_ && display.wakeCalls == 0;
  assert(!trapped && display.wakeCalls == 1);
  std::cout << "PASS: Error cannot trap a wake touch\n";
  ++passed;

  // The chunk length overflows uint32_t: offset 20 + 0xfffffff8 returns to 12.
  SD_MMC.input.bytes = {'R','I','F','F',12,0,0,0,'W','A','V','E','J','U','N','K',248,255,255,255};
  AudioService audio;
  bool looped = false;
  bool accepted = false;
  try { accepted = audio.openWav("/corrupt.wav", 1); }
  catch (const std::runtime_error&) { looped = audio.wavFile_.position() == 12; }
  assert(!looped && !accepted);
  std::cout << "PASS: overflowing WAV chunk rejected without looping\n";
  ++passed;

  auto makeWav = [](uint16_t bits, uint16_t channels, uint32_t rate) {
    std::vector<uint8_t> b{'R','I','F','F',0,0,0,0,'W','A','V','E'};
    auto word = [&](uint32_t n, int width) { for(int i=0;i<width;++i) b.push_back((n >> (8*i)) & 255); };
    auto tag = [&](const char* s) { b.insert(b.end(),s,s+4); };
    tag("JUNK"); word(1,4); b.push_back(42); b.push_back(0);
    tag("fmt "); word(16,4); word(1,2); word(channels,2); word(rate,4);
    word(rate*channels*bits/8,4); word(channels*bits/8,2); word(bits,2);
    tag("data"); word(48,4); b.insert(b.end(),48,0);
    uint32_t length = b.size()-8;
    for(int i=0;i<4;++i) b[4+i] = (length >> (8*i)) & 255;
    return b;
  };
  for (uint16_t bits : {8,16}) for (uint16_t channels : {1,2}) for (uint32_t rate : {8000,22050,44100,48000}) {
    SD_MMC.input = {}; SD_MMC.input.bytes = makeWav(bits,channels,rate);
    AudioService valid;
    assert(valid.openWav("/valid.wav",1));
    assert(valid.wav_.channels == channels && valid.wav_.bitsPerSample == bits);
    assert(valid.wav_.sampleRate == rate && valid.wav_.dataRemaining == 48);
  }
  std::cout << "PASS: 16 WAV format combinations and odd unknown-chunk padding\n";
  ++passed;
  for (int failure=0;failure<5;++failure) {
    SD_MMC.input = {}; SD_MMC.input.bytes = makeWav(16,2,22050);
    if (failure==0) SD_MMC.input.bytes.pop_back();
    if (failure==1) SD_MMC.input.failSeek = true;
    if (failure==2) SD_MMC.input.stalledSeek = true;
    AudioService bad;
    if (failure==3) bad.cancelAfter = 3;
    assert(!bad.openWav("/invalid-or-cancelled.wav",failure==4 ? 2 : 1));
    assert(bad.clockChanges == 0);
  }
  std::cout << "PASS: truncation, failed/stalled seeks and before/during-parse cancellation\n";
  ++passed;

  WorkerAudioService worker;
  workerUnderTest = &worker;
  try { worker.taskLoop(); } catch (const std::runtime_error&) {}
  assert(worker.newTrackStarts == 1);
  std::cout << "PASS: replacement WAV starts once despite pending wake notification\n";
  ++passed;

  PrinterService printer;
  printer.filterChamberTemperature(28.0f);
  fakeNow += 3600000U;
  const float invalidResult = printer.filterChamberTemperature(NAN);
  currentState.printerConnected = currentState.printerTelemetryValid = true;
  currentState.lastPrinterUpdateMs = fakeNow;
  currentState.chamberTempC = invalidResult;
  float controlTemperature = NAN;
  const bool staleAccepted = invalidResult == 28.0f && panda.controlTemperature(fakeNow, controlTemperature);
  assert(!staleAccepted && isnan(invalidResult));
  assert(!panda.controlTemperature(fakeNow, controlTemperature));
  assert(printer.filterChamberTemperature(35.0f) == 35.0f);
  std::cout << "PASS: invalid chamber measurement fails closed and fresh reading resets filter\n";
  ++passed;

  DeltaPrinter delta;
  JsonDocument packet;
  deserializeJson(packet, R"({"temperature_sensor chamber":{"target":40}})");
  delta.apply(packet.as<JsonVariantConst>());
  assert(delta.workerSnapshot_.chamberTempC == 25 && delta.workerSnapshot_.chamberSampleMs == 123);
  deserializeJson(packet, R"({"temperature_sensor chamber":{"temperature":null}})");
  delta.apply(packet.as<JsonVariantConst>());
  assert(isnan(delta.workerSnapshot_.chamberTempC));
  deserializeJson(packet, R"({"temperature_sensor chamber":{"temperature":30}})");
  delta.apply(packet.as<JsonVariantConst>());
  assert(delta.workerSnapshot_.chamberTempC == 30 && delta.workerSnapshot_.chamberSampleMs == fakeNow);
  std::cout << "PASS: actual WS chamber handling distinguishes omitted/null/fresh readings\n";
  ++passed;
  currentState.chamberTempC = 30; currentState.lastChamberUpdateMs = fakeNow;
  fakeNow += 64000; expireMeasurement(); assert(currentState.chamberTempC == 30);
  fakeNow += 1001; expireMeasurement(); assert(isnan(currentState.chamberTempC));
  currentState.chamberTempC = 30; currentState.lastChamberUpdateMs = UINT32_MAX-100;
  fakeNow = 100; expireMeasurement(); assert(currentState.chamberTempC == 30);
  fakeNow = 3600000;
  std::cout << "PASS: chamber sample age expires independently and survives millis wrap\n";
  ++passed;

  ClockAudioService clockAudio;
  const bool firstFailed = !clockAudio.reconfigureClock(24000);
  const bool secondAccepted = clockAudio.reconfigureClock(24000);
  assert(firstFailed && !secondAccepted && !clockAudio.driverReady_ && !fakeI2sEnabled);
  assert(clockAudio.sampleRate_ == 48000 && clockAudio.txChannel_ == nullptr);
  clockAudio.driverReady_ = true; clockAudio.txChannel_ = &clockAudio; clockAudio.preloadOk = true;
  assert(clockAudio.reconfigureClock(24000) && clockAudio.sampleRate_ == 24000 && fakeI2sEnabled);
  std::cout << "PASS: failed I2S transition does not commit rate; rebuild can recover\n";
  ++passed;
  currentState = SystemState{};
  fakeSettings.value.pandaEnabled = true;
  strlcpy(fakeSettings.value.pandaHost,"test-panda",sizeof(fakeSettings.value.pandaHost));
  panda.configureFromSettings();
  panda.resourcesReleased_.store(false);
  currentState.maintenanceMode = true;
  const int beforeLoops = socketLoops, beforeSends = desiredSends;
  panda.service();
  assert(panda.realtimeResourcesReleased() && socketLoops == beforeLoops && desiredSends == beforeSends);
  currentState.maintenanceMode = false;
  panda.resourcesReleased_.store(false);
  socketHook = [&] { assert(!panda.realtimeResourcesReleased()); currentState.maintenanceMode = true; };
  panda.service();
  assert(panda.realtimeResourcesReleased() && !panda.socketConfigured_ && desiredSends == beforeSends);
  socketHook = nullptr;
  std::cout << "PASS: Panda suspension before/during socket I/O prevents subsequent commands\n";
  ++passed;

  activePanda = &panda;
  panda.resourcesReleased_.store(false);
  currentState = SystemState{};
  OtaService ota;
  uint32_t startWait = fakeNow;
  assert(!ota.enterMaintenance());
  assert(ota.result == OtaState::Failed && !currentState.maintenanceMode && fakeAudio.releases == 0);
  assert(fakeNow - startWait == 8000);
  panda.resourcesReleased_.store(false);
  delayHook = [&] { panda.service(); };
  assert(ota.enterMaintenance());
  assert(currentState.maintenanceMode && fakeAudio.releases == 1 && panda.realtimeResourcesReleased());
  delayHook = nullptr;
  std::cout << "PASS: OTA refuses missing Panda ack and proceeds only after worker quiescence\n";
  ++passed;
  std::cout << "Host structure sizes: AppSettings=" << sizeof(AppSettings) << ", SystemState=" << sizeof(SystemState) << '\n';
  return passed == 12 ? 0 : 1;
}
`;
const output = path.join(root, '.pio/host-tests');
fs.mkdirSync(output, { recursive: true });
const cpp = path.join(output, 'runtime-regression.cpp');
const exe = path.join(output, process.platform === 'win32' ? 'runtime-regression.exe' : 'runtime-regression');
fs.writeFileSync(cpp, source);
const build = spawnSync(process.env.CXX || 'g++', ['-std=c++17', '-O0', '-Wall', '-Wextra', '-I', path.join(root,'.pio/libdeps/coronet_os2/ArduinoJson/src'), cpp, '-o', exe], { encoding: 'utf8' });
if (build.stdout) process.stdout.write(build.stdout);
if (build.stderr) process.stderr.write(build.stderr);
if (build.error) throw build.error;
if (build.status !== 0) process.exit(build.status || 1);
const run = spawnSync(exe, [], { encoding: 'utf8', timeout: 10000 });
if (run.stdout) process.stdout.write(run.stdout);
if (run.stderr) process.stderr.write(run.stderr);
if (run.error) throw run.error;
process.exit(run.status || 0);
