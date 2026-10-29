# coroNET OS 2 correctness and stability audit

Date: 2026-10-29. Baseline: `67a4cdb` on `main`, firmware and Android version `0.7.1`.

Scope: application firmware, BSP/display/touch, settings/NVS, boot/recovery,
LED engines, audio/SD/I2S, Moonraker HTTP/WebSocket, ventilation, Panda Breath,
Wi-Fi/mDNS/BLE/pairing, OTA, web panel, Android companion and build/release checks.
Three independent read-only Gemini reviews were checked against the source and
the installed library versions. Their unverified claims are not findings here.

**Baseline result: 17 confirmed issues.** The findings and line numbers below
describe the original `67a4cdb` baseline, not the repaired working tree.
The subsequent authorized repair pass is documented in [FIXES.md](FIXES.md).
All 17 findings now have local code corrections; physical qualification remains
outstanding. No flashing, live-device requests, release, commit or push was performed.

P1 means fix before release because the issue affects control validity, hangs or
unsafe concurrency. P2 means a functional/recovery defect. P3 means a smaller UI
defect. A source-confirmed failure path is not a claim that it has already caused
a physical reset on the user's device.

## Findings

### F01 [P1] Invalid chamber readings remain valid indefinitely

- Location: `src/printer/PrinterService.cpp:1238`, full-poll consumer at `:962`;
  `src/panda/PandaBreathService.cpp:480`; `src/vent/VentService.cpp:268`.
- The filter returns the previous finite value immediately when its input is
  non-finite. It never applies an age limit on this branch. A successful printer
  status poll without a valid chamber reading can therefore refresh the global
  telemetry timestamp while preserving an arbitrarily old temperature.
- Panda's freshness check and ventilation's NaN failsafe then see valid data.
  This is not merely a stale number on the screen: control decisions can use it.
- Reproduced using the actual filter and Panda control-temperature method: a
  missing reading one hour after 28 C is still accepted as 28 C.
- Repair: track validity/age of the chamber measurement independently of the
  whole telemetry packet, invalidate expired readings, and fail safe downstream.
  A missing field in a WS delta alone must not be confused with sensor failure.

### F02 [P1] Malformed WAV can loop forever in the audio worker

- Location: `src/audio/AudioService.cpp:934`, unchecked seek at `:951`.
- Chunk offset arithmetic uses `uint32_t` without bounds/overflow validation.
  A 20-byte RIFF/WAVE with a JUNK length of `0xfffffff8` makes offset 20 wrap to
  12, so the parser repeatedly reads the same chunk. Seek success and forward
  progress are unchecked; this loop does not observe a stop request.
- Reproduced with the actual `openWav` method and a bounded fake SD file.
  The worker cannot finish parsing or acknowledge stop/release. A corrupt boot
  WAV also threatens first-boot progress. Hardware starvation/reset is unmeasured.
- Repair: validate RIFF and chunk extents against actual file size using wider
  arithmetic, check every seek, require progress and support cancellation.

### F03 [P1] SD OTA can access the Panda socket concurrently

- Location: `src/update/OtaService.cpp:279`, `:505-511`;
  `src/panda/PandaBreathService.cpp:81`, `:151-156`.
- The OTA worker directly calls `pandaBreathService().disconnect()` while the
  main task can be inside `socket_.loop()` or a send. SD install does not use the
  TLS resource-window handshake. Setting `maintenanceMode` does not wait for an
  already-running Panda operation to finish; main can also call disconnect.
- In the pinned WebSockets library, disconnect can delete `client->tcp`
  (`WebSocketsClient.cpp:584`) while the other task still uses that pointer.
  This is a source-confirmed race, not a reproduced hardware panic.
- Repair: give the socket one owner; queue shutdown and wait for its acknowledgement
  before entering maintenance. Do not call its methods from two tasks.

### F04 [P2] Unreachable Panda can stall main-loop services for seconds

- Location: `src/panda/PandaBreathService.cpp:80-81`; `src/main.cpp:480`.
- Panda runs WebSocketsClient synchronously in the Arduino loop. The pinned
  WebSockets 2.7.2 implementation calls TCP connect with a 5000 ms timeout
  (`WebSocketsClient.cpp:331`, `WebSockets.h:125`). Retry interval is also 5 s.
- With Panda enabled and an unreachable host, display state updates, web request
  handling, printer-result application and ventilation scheduling can be delayed.
  LED/audio have separate workers; this is not evidence that those tasks stop.
- Repair: move Panda network I/O to a single-owner worker with bounded messages,
  as already done for printer networking. Measure actual latency on hardware.

### F05 [P2] Replacing an active sound can replay the replacement twice

- Location: `src/audio/AudioService.cpp:714-727`; request producer at `:596-617`.
- A new sequence noticed during playback bypasses `ulTaskNotifyTake`, leaving its
  task notification pending. After the replacement finishes, that notification
  wakes the worker and the unchanged request is executed again even with repeat
  disabled. Stop requests can also be processed more than once.
- Reproduced with the real worker loop and simulated request/notification endpoints.
- Repair: drain notifications consistently and execute only a sequence newer
  than the handled sequence. Keep driver release acknowledgement single-owner.

### F06 [P2] Failed sample-rate reconfiguration leaves inconsistent I2S state

- Location: `src/audio/AudioService.cpp:895-915`, particularly `:906-909`.
- After disabling I2S, a preload failure skips re-enabling it but leaves
  `driverReady_ == true` and commits the new `sampleRate_`. A retry with that
  same sample rate immediately reports success although the channel is disabled.
- Fault-injection reproduction executes the real method with preload returning
  failure. This does not claim preload normally fails or measure its likelihood.
- Repair: commit state only after full success and implement a defined recovery
  or teardown path. Restore/recreate the channel on failure; never advertise it ready.

### F07 [P2] Error state can prevent waking the screen saver

- Location: `src/display/DisplayService.cpp:307-329`, early return at `:321`.
- A reconnect baseline can already be Error without a new printer event. The
  Error early return then runs before checking the user's wake touch. Also, a
  new Error within the transition guard can fail to leave the saver without retry.
- Reproduced with a released wake touch and Error without a new event.
  A normal new Error outside that guard does have an automatic wake path.
- Repair: always process user wake first and retry Error-triggered wake until
  complete. Error should prevent entering a saver, not exiting it.

### F08 [P2] Display transport has unbounded waits without error recovery

- Location: `src/bsp/esp_bsp.c:186`; `src/bsp/lv_port.c:593-601`.
- The active TE path waits indefinitely for `te_v_sync_sem` while LVGL holds its
  mutex. Persistent TE loss therefore blocks rendering and touch processing.
  The flush loop also ignores the result of `esp_lcd_panel_draw_bitmap` and waits
  indefinitely for the transfer completion; a rejected transfer has no callback.
- Source-confirmed error paths, not reproduced on the physical panel. One missed
  TE pulse is not necessarily fatal: a later pulse can release the wait.
- Repair: bounded waits, checked submission status and controlled panel/transport
  recovery. Do not blindly reuse a DMA buffer still owned by an in-flight transfer.

### F09 [P2] A new tempering cycle can end immediately after Complete

- Location: `src/panda/PandaBreathService.cpp:173-182`, `:353-375`.
- Mode changes reset the timer/start temperature but leave the workflow phase.
  Manual tempering skips initialization for both Tempering and Complete. Starting
  it after a completed cycle can compare uptime against a zero start timestamp
  and immediately disable the new cycle.
- Reproduced: finish preheat, enable 30-minute tempering at one-hour uptime;
  the new cycle immediately returns to Complete with Panda disabled.
- Repair: explicitly initialize each new workflow independently of the previous
  visible phase; cover Automatic-to-manual tempering too.

### F10 [P2] Android rejects valid BLE messages over 64 fragments

- Location: `android/app/src/main/java/de/alphastudio/coronet2/transport/CoronetBleManager.kt:444`;
  firmware framing at `src/ble/BleService.cpp:1426-1432`.
- Firmware allows 1..255 chunks; Android allows only 1..64. At MTU 23 the payload
  per chunk is 12 bytes, so valid JSON over 768 bytes is silently rejected.
  Sound-library pages with eight long paths/names can exceed this size.
- Source-level wire-contract mismatch; not tested against a phone forced to MTU 23.
- Repair: align framing limits, bound allocation/assembly age and provide a
  recoverable error when a library response cannot be assembled.

### F11 [P2] Android BLE recovery does not cover stalled GATT operations

- Location: `CoronetBleManager.kt:245-266`, `:304`, `:335-370`;
  `CoronetViewModel.kt:759-775`, under the Android source directory above.
- Command writes set an in-flight flag cleared only by callbacks/reconnection.
  The service-discovery timeout is removed before CCCD subscriptions finish.
  A missing write/subscription callback has no deadline that restarts the session.
- The ViewModel telemetry watchdog marks the connection offline but does not
  tear down/reconnect GATT. If notifications resume, it can recover; if they never
  resume and Android emits no disconnect, manual recovery is needed.
- Source-confirmed missing recovery paths, not a measured radio-fault frequency.
- Repair: session-scoped deadlines for connect/discovery/CCCD/command operations,
  one serialized GATT queue, stale-callback rejection and reconnect on expiry.

### F12 [P2] A BLE command callback leaks on stack recreation

- Location: `src/ble/BleService.cpp:387`, teardown at `:459-482`.
- `new CommandCallbacks()` is passed to NimBLECharacteristic without retaining
  ownership. The pinned characteristic destructor deletes descriptors but not
  its callback (`NimBLECharacteristic.cpp:53-57`). Stack recreation leaks that
  allocation each time, including repeated BLE/TLS lifecycle operations.
- The server callback is different: NimBLEServer owns and deletes it by default.
  No exact bytes-per-cycle or measured heap exhaustion is claimed.
- Repair: static/member lifetime for the characteristic callback, or explicit
  ownership and deletion after the stack has fully stopped.

### F13 [P2] Android ignores integer booleans from firmware BLE settings

- Location: `CoronetWifiClient.kt:210-212`; `src/ble/BleService.cpp:1172-1181`.
- Firmware sends `setupDone`, `bleEnabled`, `apiPaired` as 0/1, while the parser
  uses `optBoolean`, which retains its previous/default value for numeric JSON.
- Reproduced on compiled production Kotlin classes: values 1/0/1 are read as
  default false/true/false. The pairing/settings UI can disagree with firmware.
- Repair: agree on JSON boolean types, or explicitly accept numeric aliases;
  add contract tests for both BLE and HTTP settings representations.

### F14 [P2] Late web catalogue responses overwrite the selected category

- Location: `web/index.html:114`, category interaction at `:129`.
- `loadLedCatalog` uses the category when issuing the request but unconditionally
  installs the response. An older IDLE response arriving after PRINT replaces
  the selected category's names/count, giving incorrect animation selection UI.
- Reproduced by executing the actual function with responses resolved out of order.
- Repair: capture category and request generation, reject stale responses or
  store catalogues by category. NEW/LEGACY share the same names by design.

### F15 [P2] OTA clock synchronization can override the user's time zone

- Location: `src/update/OtaService.cpp:326`; `src/display/DisplayService.cpp:280-303`.
- Early OTA check before trusted time calls `configTime(0,0,...)`, which changes
  global TZ in the pinned Arduino core. Display caches the configured zone and
  does not restore it when the setting is unchanged and NTP has made time ready.
- Result: clock and time-dependent theme can remain on UTC until zone change or
  reboot. Verified against `esp32-hal-time.c` in the actual Arduino 3.2.1 package;
  no live early-boot OTA test was performed.
- Repair: one time-service owner, preserving the configured zone during TLS sync.

### F16 [P3] Web renders hue zero as the default cyan

- Location: `web/index.html:109`.
- `settings.accentHueDegrees || 190` treats valid red hue 0 as missing.
- Reproduced with the actual `applyTheme` function. Use a null/missing-value
  fallback rather than truthiness.

### F17 [P3] Android optimistic transport update ignores its own patch key

- Location: `CoronetViewModel.kt:290-293`; `CoronetWifiClient.kt:213`.
- The outgoing patch uses `companionTransport`, but parsing recognizes only
  `transportValue` and `transport`. The immediate UI update is skipped and waits
  for a subsequent full response, unlike other optimistic setting updates.
- Reproduced on compiled production Kotlin classes with `companionTransport:2`.
- Repair: normalize aliases in the parser and pending-setting reconciliation.

## Baseline verification

| Check | Result |
| --- | --- |
| PlatformIO `coronet_os2` production build | PASS, 65 s, no upload |
| Static RAM / app flash from build | 70,460 / 327,680 B; 2,660,614 / 6,291,456 B |
| Release-artifact validation | PASS, image version 0.7.1, firmware.bin 2,661,040 B |
| LED catalogue validator | PASS, 336 entries each for NEW and LEGACY |
| Android `assembleDebug` | PASS |
| Android `testDebugUnitTest lintDebug --rerun-tasks` | PASS, 6 tests; lint 0 errors, 11 warnings |
| Cppcheck 2.11, warning/performance/portability across 38 source units | Completed; 5 format-promotion warnings in BLE settings; board-specific MemoryService checked again with board macros |
| Actual web inline JavaScript syntax compilation | PASS |
| `runtime-repro.mjs` | 6 faulty behaviors reproduced using extracted production C++ methods |
| `web-repro.mjs` | 2 faulty behaviors reproduced using production JS functions |
| `AndroidParserRepro.java` | 2 faulty behaviors reproduced using compiled production Kotlin |

The original reproducers asserted the **faulty baseline**. Their zero exit status
meant a bug was reproduced. They have now been converted to correctness assertions
in `tests/runtime-regression.mjs`, `tests/web-regression.mjs` and Android JUnit tests;
the obsolete Java reproducer was replaced by tests of the production Kotlin parser.
Hardware/RTOS endpoints in the C++ harness are stubs; it proves state-machine and
parser behavior, not real scheduler timing, driver failures or physical signals.

Commands from repository root:

```powershell
& C:/Users/damia/.platformio/penv/Scripts/pio.exe run -e coronet_os2
& C:/Users/damia/.platformio/penv/Scripts/python.exe scripts/validate_led_catalog.py
& C:/Users/damia/.platformio/penv/Scripts/python.exe scripts/validate_release_artifacts.py .pio/build/coronet_os2 0.7.1
node tests/runtime-regression.mjs
node tests/display-transfer-regression.mjs
node tests/web-regression.mjs
```

From `android/`: `./gradlew.bat testDebugUnitTest lintDebug assembleDebug --console=plain`.
Current repair results and limits are in [FIXES.md](FIXES.md).

## Claims checked and not accepted as bugs

- NEW/LEGACY selection is intentionally synchronized. Separate stored selections
  and reloading an identical catalogue would contradict the requested behavior.
- `notify_klippy_ready` is handled and requests the object list; non-ready Klippy
  at handshake is not by itself a permanently dead WS connection.
- `WL_CONNECTED` is set on GOT_IP in the pinned Arduino core, not merely on L2
  association. The claimed normal DHCP-before-mDNS failure was not established.
- SetupWizard cancels its connection test on reset/back/change. The claimed
  ordinary exit leaving Wi-Fi tests active was not established.
- Android sends small setting patches, not a full settings snapshot. The claim
  that every BLE settings change exceeds 384 bytes was rejected.
- NimBLEServer frees its callbacks by default; only the characteristic ownership
  issue above remains. Blanket claims that both leak were rejected.
- Audio `begin()` is only called at setup. An alleged normal release/begin double
  task leak is not reachable in the reviewed runtime call graph.
- LED output intentionally protects a roughly 2.4 ms polling SPI waveform from
  inter-chunk resets. Removing its critical section without another continuous
  transfer mechanism can reintroduce flicker. This requires measurement, not an
  automatic "fix" or an unsupported claim of a watchdog timeout.
- The printed static RAM percentage is not runtime free DMA or largest DMA block.
- DIO image headers with the configured boot flow passed artifact validation;
  this is not evidence of the historical OTA version/boot failure recurring.

## Original repair order

1. Fix chamber freshness, WAV bounds/cancellation, and single-owner maintenance
   transitions before distributing another build.
2. Fix audio request sequencing/recovery and display waits/wake behavior with
   regression tests and explicit fault injection.
3. Fix Panda workflow/nonblocking networking, BLE framing/lifetime/recovery and
   client parser/catalogue synchronization.
4. Repeat builds, lint, protocol tests and artifact checks on the resulting diff.
5. Hardware soak: simultaneous LED+audio+UI+HTTP+BLE, rapid animation/audio changes,
   AP/Panda/printer loss/recovery, SD failures, saver Error transitions and OTA
   interruption/recovery. Record frame-time distribution, audio underruns/errors,
   internal free/minimum/largest DMA memory, task stack high-water marks, reset
   reasons and core dumps. Test more than one SD card and phone/MTU combination.

This was a repository-wide code/build audit, not exhaustive proof over every
interleaving, all 336 visual effects or every physical fault. No 24-72 h soak,
oscilloscope/logic-analyzer verification, real panel fault injection, phone BLE
stress test or OTA power-cut test was performed. Historical OS1-parity notes were
context, not proof of the current implementation. Contest materials, credentials,
signing material, CrossDevice/synchronized folders and drive E were out of scope.
