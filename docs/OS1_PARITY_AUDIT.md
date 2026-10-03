# coroNET OS 1 Parity And Reliability Audit

Audit date: 2026-10-04  
Audited release line: coroNET OS 2 `0.6.0`

This audit uses the final coroNET OS 1 `sketch_v20_spi.ino` backup as the
behavioural and reliability reference for coroNET OS 2. It is not a
line-for-line port review: OS 2 is modular, uses newer Arduino-ESP32 APIs, and
has intentional product differences documented in `OS1_FEATURE_SCOPE.md`.

The audit distinguishes three levels of evidence:

- **Implemented and statically reviewed**: the complete code path and its
  failure handling were inspected against OS 1.
- **Automatically verified**: a build, validator, unit test, lint task, or
  syntax check completed successfully.
- **Hardware qualification required**: timing, electrical, radio, audio, or
  third-party-device behaviour cannot be proved by source review.

## Scope reviewed

The review covered every item in `OS1_FEATURE_SCOPE.md` and traced the shared
state/settings paths used by the touchscreen, local web panel, Android Wi-Fi,
and Android BLE transports. The detailed review included:

- boot ordering, first-frame backlight handling, factory reset, and NVS
  migration/persistence;
- the 50 FPS physical LED scheduler, NEW and LEGACY renderers, preview changes,
  output smoothing, color calibration, section mapping, quiet mode, and print
  state transitions;
- WAV parsing and playback, I2S clock changes, DMA writes, fades, guard silence,
  request coalescing, SD indexing, rapid interruption, and maintenance release;
- Moonraker HTTP and WebSocket connection lifetimes, subscriptions, stale-state
  recovery, state ordering, partial telemetry merges, and chamber filtering;
- local fan/flap control, telemetry loss failsafe, DIY heater startup state, and
  Panda Breath mode transitions;
- LVGL scheduling, touch/scroll arbitration, slider hit areas, screen saver
  wake behaviour, theme rebuild position, and cached UI updates;
- Wi-Fi scan/reconnect, mDNS serialization, BLE allocation/crash recovery,
  pairing/authentication, framed messages, and bounded queues;
- OTA metadata, TLS, checksums, image/partition compatibility, service
  suspension, SD recovery, and rollback confirmation;
- web request authorization, JSON bounds, settings revisions, and Android model
  compatibility.

## Regressions corrected

| Area | Failure or lost OS 1 safeguard | Correction in OS 2 |
| --- | --- | --- |
| LED timing | A 20 ms frame period retained an OS 1 step sized for 30 ms, making transitions too fast and angular | Restored OS 1 response rates at 50 FPS: `12` for Idle/Other/Boot, `7` for Print/Pause/Finish, and `2` for Error |
| LED timing | Rendering and presentation time could drift together | Added a fixed 20 ms physical presentation deadline, late-frame accounting, and a stable latch on every frame |
| Legacy animations | Legacy rendering had been treated as a second selection catalog | NEW and LEGACY now use the same numeric selection, while the legacy renderer retains its native 30 ms logical cadence |
| Boot LEDs | Boot output bypassed smoothing for more than the initial physical frame | Only the first boot frame is presented immediately; all later boot frames use the normal OS 1-style channel slew |
| LED concurrency | UI/web preview readers could observe a frame while it was being replaced | Added synchronized frame and preview access plus coalesced preview requests |
| Web LED control | Rapid browser changes overlapped persistence and preview requests | Added serialized/coalesced selection updates and automatic preview after every category, animation, or NEW/LEGACY change |
| Moonraker state | An Error state could remain visible after a new print | Added persistent WebSocket header storage, monotonic result/state sequences, stale-result rejection, and a five-second HTTP recovery audit for Error/Unknown |
| Moonraker compatibility | HTTP fallback knew fewer chamber sensor aliases than WebSocket discovery | Unified cavity, chamber, enclosure, `chamber_temp`, and `enclosure_temp` handling |
| Audio switching | Rapid track changes could race file/driver operations and restart the ESP32 | Moved playback ownership to one Core 0 task, coalesced requests by sequence, and kept open/close/fade/I2S work inside that task |
| Audio quality | OS 2 did not reproduce the complete OS 1 click/pop protection | Restored native WAV clocks, one-second edge ramps, the separate interrupted-playback ramp, zero/guard silence, complete partial writes, and bounded stalls |
| Audio startup | Partial PSRAM allocation or task creation failure leaked earlier allocations | Added complete startup buffer cleanup without freeing buffers still owned by a delayed task |
| OTA/audio | OTA failed when audio had no task, even though there was nothing unsafe to stop | Treat an absent audio task as already stopped; a real running-task timeout still aborts the update |
| Settings | Frequent control changes caused one flash commit per field | Replaced them with a debounced single NVS transaction and exact settings revisions |
| Settings concurrency | Whole-setting writes from different transports could overwrite a newer change | Added atomic snapshots and compare-and-replace by revision |
| Settings ranges | An old NVS clock brightness could survive below every UI's minimum | Unified persisted and transport validation to `5..100` |
| Wi-Fi | Modem sleep and overlapping scan/reconnect work could stall realtime traffic | Disable sleep through Arduino and IDF APIs, reapply after connection, and serialize bounded scan/test/reconnect phases |
| BLE | Failed startup could repeatedly consume memory or enter a reset loop | Added contiguous-memory gates, clean one-boot disable after failure, and an RTC crash guard |
| BLE security | Settings/state commands could be accepted before the link was fully trusted | Require both pairing and an authenticated encrypted connection before protected commands |
| Ventilation | Automatic outputs could keep using stale printer temperatures | Added telemetry age checks and the configured fan/flap failsafe after an observed print loses telemetry |
| Ventilation startup | Partial MCPWM construction leaked driver handles | Added reverse-order cleanup for every timer/operator/comparator/generator failure |
| Printer startup | Partial queue/mutex/PSRAM or task allocation leaked resources | Added complete startup cleanup and a usable offline state |
| Display | A touch intended to scroll could activate a control beneath it | Added press/move/release gesture arbitration; the first screen-saver wake gesture is consumed |
| Display | Theme changes rebuilt the page at the top | Preserve active page and scroll position across theme reconstruction |
| Display boot | Uninitialized panel memory was visible as grey noise | Keep the backlight off until LVGL has produced and settled the first frame |
| Web input | Small JSON endpoints did not share the settings endpoint's input limit | Apply the same 4096-byte limit to settings, LED preview/calibration, and audio-play JSON |

## Verified subsystem properties

| Subsystem | Static review result | Automated evidence |
| --- | --- | --- |
| LED engine | Dedicated Core 1 task, 20 ms presentation deadline, physical smoothing, calibrated shared output path, bounded 60-pixel buffers | Catalog validator confirms 336 NEW and 336 LEGACY entries |
| Audio | Dedicated Core 0 owner, bounded PSRAM buffers and DMA ring, complete writes, bounded write/stall waits, scenario sequencing | Firmware compiles and links with the audio failure paths enabled |
| Printer | Dedicated low-priority worker, bounded HTTP waits, persistent WS headers, subscription discovery, ordered merge, reconnect and HTTP recovery | Firmware build covers both HTTP and WebSocket implementations |
| Settings | Versioned migration, validated ranges, atomic snapshots, revision conflict detection, one-commit persistence | Firmware build plus Android transport tests |
| Display/touch | Backlight gate, saver first-touch consumption, last-touch timeout, scroll-safe sliders, cached refreshes | Firmware build; visual/touch behaviour still requires hardware |
| Vent/Panda | Startup LOW heater output, local failsafe, output smoothing, bounded Panda commands and explicit mode state | Firmware build; Panda stock-firmware qualification remains open |
| OTA | Trusted CA bundle, release size and MD5 validation, two 6 MiB OTA slots, rollback confirmation, maintenance cleanup | Built image is valid ESP32-S3 DIO/16 MB/80 MHz and 2,653,104 bytes; the binary partition table matches all required offsets, sizes, types, and subtypes |
| Web panel | Same-origin browser session, API-token path, settings revisions, bounded JSON parsing, current LED/audio controls | Inline JavaScript syntax check passes |
| Android | Wi-Fi and BLE transports, framed BLE reassembly, saved credentials, all current settings and five product pages | `testDebugUnitTest` and `lintDebug` pass |

## Intentional differences from OS 1

These are explicit product decisions rather than missing migration work:

- no activation/license-key system because coroNET OS 2 is MIT licensed;
- no MIDI engine, which was already disabled in OS 1;
- no on-device manual animation creator or preset editor;
- no Philips Hue, WLED, Home Assistant, Nanoleaf, or Shelly providers;
- no simulated printer telemetry in release firmware;
- no separate on-device `What's New` popup;
- realtime Moonraker events plus integrity polls replace OS 1's three fixed
  polling groups.

## Security boundary

The Android/API path uses the per-device token, and BLE settings/state commands
require a paired authenticated encrypted link. The browser panel deliberately
bootstraps a reboot-scoped session for a request whose Host/Origin identifies
the device. This prevents ordinary cross-site browser calls, but it is not a
password boundary against a malicious client already on the same LAN. A future
optional web login/pairing mode would be required for hostile or guest networks.

Online OTA uses TLS plus the published MD5 asset. SD recovery is a physically
controlled recovery path and validates image magic and size, but does not have
a separate signature or checksum file. These are documented trust boundaries,
not claims of cryptographic secure boot.

## Hardware qualification still required

Source review cannot honestly certify the following. They are the release gate
between the current 0.6.x validation phase and 1.0.0:

1. Run at least a 24-hour soak with Wi-Fi, Moonraker WebSocket, BLE, audio, web
   polling, touch, and 50 FPS LEDs active together. Require zero unexpected
   restarts, audio write failures, and sustained LED dropped-frame growth.
2. Repeat rapid animation/category/NEW/LEGACY changes from touchscreen, browser,
   and Android while long WAV audio is playing.
3. Disconnect and restore the access point and Moonraker independently during
   Idle, Printing, Pause, Error, Complete, and a subsequent new print. Confirm
   current state recovery and exactly one correct sound/state event.
4. Test malformed, missing, very short, mono, stereo, and mixed-sample-rate WAV
   files; remove/reinsert SD; repeatedly interrupt playback near both edges.
5. Qualify fan, flap, heater LOW-on-boot/maintenance, stale telemetry failsafe,
   and every Panda Breath workflow on the real connected hardware.
6. Exercise display-off and every clock style, scroll every dense card, drag
   slider knobs at both edges, change themes mid-page, and run repeated factory
   resets.
7. Perform OTA check/install/reinstall, interrupted download, rollback, and SD
   recovery on a sacrificial test unit.

The LVGL render watchdog remains deliberately deferred. Existing lock timeouts,
cached updates, task diagnostics, and health counters should first be measured
during the soak. Adding an unproven watchdog could turn one recoverable slow
frame into an unnecessary reboot.

## Automated verification record

At the end of this audit:

- `py -m platformio run`: passed; RAM 69,596 / 327,680 bytes (21.2%), firmware
  code 2,652,686 / 6,291,456 bytes (42.2%);
- `py scripts/validate_led_catalog.py`: passed; 336 animations in each renderer;
- `py scripts/validate_release_artifacts.py .pio/build/coronet_os2 0.6.0`:
  passed; OTA image 2,653,104 bytes, DIO header, exact 16 MB partition layout;
- `gradlew testDebugUnitTest lintDebug assembleDebug --rerun-tasks`: passed;
  6 tests, 0 failures, and 0 lint errors. Lint retains 11 non-blocking warnings
  for available dependency updates, the API-gated notification permission,
  and the intentionally portrait-only activity; Gradle also reports the known
  Android Gradle plugin/`compileSdk 36` compatibility warning;
- inline web JavaScript syntax check: passed;
- `git diff --check`: no whitespace errors (line-ending conversion warnings may
  still be printed on Windows).

The bundled `cppcheck` executable could not be used because its installation
contains a broken compiled-in data-directory path. No static-analyzer result is
claimed in this report.
