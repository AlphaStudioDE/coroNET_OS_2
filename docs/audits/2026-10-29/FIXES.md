# Local repair pass

Date: 2026-10-29. Base commit: `67a4cdb`. Firmware and Android remain `0.7.1`.
This records the local repair pass before release preparation, not hardware sign-off.

Publication follow-up: the user subsequently authorized release **0.9.9** with
these corrections and the matching Android app. The original 0.7.1 measurements
below describe the repair baseline; physical qualification is still outstanding.

Before editing, a 210-file recovery ZIP was verified in the local Codex backups
directory and a local safety ref was created. No existing user changes were reverted.
No device was flashed, no phone was installed, and no GitHub content was published.

## Corrections

| Finding | Correction | Local evidence |
| --- | --- | --- |
| F01 | Invalid chamber readings reset the filter; measurement age is independent of packet age. Omitted WS delta fields stay unchanged, explicit null is invalid. Unrefreshed samples expire after 65 s, allowing two normal 30 s full-poll intervals. | Actual filter/control and WS chamber block tests; expiry and millis-wrap tests. |
| F02 | WAV offsets use 64-bit bounds against RIFF/file extent, enforce 32-bit seek range, verify seek/progress, limit chunk count and observe the owning request sequence. | Overflow, truncation, failed/stalled seek, cancellation; 16 valid format combinations with padded unknown chunk. |
| F03 | Only the Panda worker accesses its socket. OTA requests suspension and waits for an acknowledgement; missing acknowledgement aborts preparation. | Actual Panda service and OTA entry tests with suspension before/during I/O and an 8 s timeout. |
| F04 | Blocking Panda networking moves off the main application loop to a lazily created worker. Commands/discovery are signaled atomically. | Call-path review and build; physical scheduling and stack headroom still required. |
| F05 | Audio notifications are wake hints; unchanged request sequences cannot replay a file. Stop acknowledgement no longer primes I2S, so a duplicate stop cannot write into a released driver. | Actual worker-loop test replacing a playing WAV with a pending notification. |
| F06 | Commit the sample rate only after preload/enable succeeds; invalidate failed drivers and rebuild on the audio worker. Serialize release/profile changes; retain a handle if deletion fails. | Actual clock-transition failure and recovery test; build. |
| F07 | Active saver wake handling precedes the current-Error guard. | Actual saver method test with an Error baseline and released wake touch. |
| F08 | TE and DMA waits are bounded. A timed-out DMA buffer stays quarantined until completion. Submission errors propagate; redraw is requested outside the render callback. | Actual BSP callbacks: normal transfers, timeout ownership, late completion, submission failure/retry, rotations, missing TE. |
| F09 | A newly selected manual tempering cycle initializes its timer even after the previous Complete state. Existing restart-after-Fault behavior is preserved. | Actual workflow/configuration methods at one hour uptime. |
| F10 | BLE reassembly accepts the firmware's 1..255 chunk range, with bounded 4096-byte messages and eight in-flight assemblies. | Production Kotlin assembler tests. |
| F11 | Connect, discovery/subscription and command-write phases have deadlines. GATT callbacks/lifecycle changes share the main queue; old sessions cannot cancel new deadlines or send queued commands to another connection. Stale BLE can reconnect without replacing a working Wi-Fi link. | Android build/lint and code review; real GATT fault injection remains required. |
| F12 | Characteristic callbacks have static lifetime instead of leaking one allocation per BLE restart. Server callback ownership is unchanged. | Checked against pinned NimBLE ownership contract; build. |
| F13 | Settings booleans accept JSON booleans and numeric 0/1 while preserving existing string-boolean support and defaults for invalid/missing values. | Production parser JUnit tests. |
| F14 | LED catalogue requests carry category and generation guards. | Actual web function tests with out-of-order responses across and within categories. |
| F15 | OTA secure-clock synchronization uses the configured POSIX time zone. | Source review/build; physical NTP/clock check outstanding. |
| F16 | Accent hue 0 uses a nullish fallback rather than a truthiness fallback. | Actual theme function test. |
| F17 | Transport setting aliases are accepted by parsing and pending-setting acknowledgement. | Parser tests and reconciliation review. |

## Verification

- PlatformIO production firmware build: PASS; no upload.
- Android debug APK, unit tests and lint: PASS. 19 tests, zero failures/errors;
  lint has zero errors and 11 pre-existing warnings.
- Runtime harness: 12 scenario groups PASS, using production functions/blocks
  against simulated endpoints rather than copied algorithms.
- Display transfer harness: six scenario groups PASS.
- Web inline syntax and three regression cases: PASS.
- LED catalogue validator: 336 entries in each of NEW and LEGACY, unchanged.
- Targeted Cppcheck of eight modified firmware/BSP units: no new findings;
  three existing assertion-side-effect warnings in a BSP macro configuration.
- Final binary/header/partition validation: PASS, `0.7.1`, DIO,
  `firmware.bin` 2,663,152 bytes. Static RAM: 70,508 / 327,680 bytes;
  app flash: 2,662,730 / 6,291,456 bytes. Static RAM grew by 48 bytes against
  the baseline; this is not a runtime free/largest DMA measurement.
- `git diff --check`: PASS (only the existing Windows line-ending notices).

The optional Panda worker has an 8192-byte stack plus its RTOS control block,
allocated only after enabling Panda or requesting discovery. It stays allocated
once started. This is a deliberate runtime memory cost for main-loop isolation;
stack high-water and largest DMA must be measured under load.

LED renderers, output frequency, calibration, I2S buffer sizes, volume/gain,
musical fades, partition table, settings schema and saved-user settings were not
changed. No dependency/toolchain upgrades or broad UI redesign were performed.
Gemini Bridge contributed bounded Android/audio edits, which were reviewed and
corrected locally; a further independent review could not run due to its quota.
Unverified model output is not counted as test evidence.

## Hardware gate before release

1. Run LED + WAV + touch + web + BLE simultaneously. Measure LED frame-time
   distribution, audio errors/underruns, free/minimum/largest internal/DMA memory,
   Panda/audio/display stack headroom and reset reason over a long soak.
2. Exercise missing/invalid chamber telemetry and printer/Panda/AP recovery.
   Verify real ventilation/heater failsafes; mocks do not validate physical outputs.
3. Test connect/discovery/CCCD/write loss and MTU 23/247 on the phone, including
   Wi-Fi/BLE handoff and rapid device changes. Desktop unit tests are not GATT tests.
4. Test valid and corrupt SD WAVs, stop/replace/repeat, changing sample rates,
   saver Error wake, and the screen under repeated interaction.
5. Test SD/online OTA with Panda enabled and unreachable, missing suspension ack,
   timezone retention and recovery after interruption. Only then publish an OTA.

A permanently missing LCD DMA completion is bounded and does not reuse the
owned buffer, but it is not magically repaired: rendering can recover after a
late completion; a persistent hardware/driver fault may still need a restart.
