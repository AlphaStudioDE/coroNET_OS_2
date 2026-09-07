# Development Updates

## 0.5.0

### Touchscreen Interaction
- Added a shared gesture guard that distinguishes horizontal slider adjustment from vertical page scrolling before committing a setting.
- Restored the original value whenever a slider gesture becomes a scroll, preventing accidental changes on control-dense pages.
- Increased slider spacing, usable touch area, and right-edge clearance while keeping the visible knob inside each card.
- Added consistent slider depth and feedback styling across LED, Vent, Sound, and Settings controls.
- Preserved the Settings scroll position when changing a theme or returning from the screen saver.

### Compact And Consistent Layout
- Replaced per-page status headers with one compact header shared by Home, LED, Vent, Sound, and Settings.
- Kept WiFi, Bluetooth, and printer indicators in stable positions while returning more vertical space to page content.
- Reorganized the LED animation card so the inside-strip preview sits directly below the Animation heading and renderer changes remain immediately visible.
- Increased the visible sound-browser rows and refined card spacing across control pages.

### Home Temperature Presentation
- Enlarged and rebalanced the Active Tool, Bed, and Chamber cards while preserving prominent `T1` through `T4` identification.
- Made the active-tool temperature as legible as the bed and chamber values and moved filament color to the card's accent line.
- Added live bed heat coloring from green at or below 30 C through amber to red at 100 C.
- Added live chamber heat coloring from green at or below 30 C through amber to red at 60 C.

### Screen Saver And Retro Clock
- Made the first touch anywhere on the clock wake the display without activating the underlying page control.
- Restarted the inactivity countdown from the latest touch event instead of an earlier page transition.
- Restored the coroNET 1 seven-segment Retro clock with dim inactive segments, illuminated separators, glow, 12/24-hour handling, and AM/PM support.
- Linked the Retro clock segments, glow, suffix, and panel detail to the exact accent hue selected in Settings.

### Release Milestone
- Closed the planned visual and interaction refinement pass across the touchscreen, Android companion, and local browser panel.
- Moved the active release path to systematic physical workflow, integration, recovery, and long-duration stability testing before `1.0.0`.
- Built and installed the release firmware on the target JC3248W535 ESP32-S3 and coordinated the matching Android `0.5.0` package.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.5.0_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.4.7...v0.5.0

## 0.4.7

### NEW And LEGACY Animation Libraries
- Added a selectable `NEW`/`LEGACY` renderer switch to the coroNET touchscreen, Android companion, and local browser panel.
- Preserved all 336 original coroNET 1 renderers beside the 336 rebuilt coroNET OS 2 implementations while keeping one corresponding animation position for each printer-status category.
- Made renderer, category, and animation changes start the real ten-second hardware preview automatically, removing the separate play action from animation selection.
- Routed both libraries through the same brightness, DIMM, mirroring, ambient-inside, remix, physical calibration, and live-preview policies.

### LED Motion And Presentation
- Restored coroNET 1-style microsecond frame deadlines, unconditional 50 FPS latching, and fixed per-frame RGBW smoothing after calibration and perceptual conversion.
- Reworked continuous NEW-library movers and long color cycles with fractional LED positions and interpolated light fields while retaining intentional hard steps in digital scenes and warning flashes.
- Expanded the catalog validator so every one of the 336 positions must be implemented and reachable in both renderer libraries.

### Physical LED Color Defaults
- Set the factory RGBW calibration to 115% saturation and 100% brightness for all eight reference colors, with hue corrections of -20 degrees for orange and -30 degrees for yellow.
- Made per-color reset, full reset, Android fallbacks, and browser fallbacks restore the same measured defaults.
- Migrated only untouched legacy `0/100/100` profiles during OTA, preserving every user-adjusted calibration.
- Applied the same saved color calibration to full and daily boot animations instead of overriding full-boot saturation separately.

### Companion And Protocol Alignment
- Added the renderer selection and explicit preview mode to BLE, Wi-Fi, Android persistence, conflict resolution, and protocol tests.
- Kept the former legacy-animation array mirrored for backward compatibility while making the current shared selection authoritative.

### Validation
- Built and installed the release firmware on the target JC3248W535 ESP32-S3 and confirmed the local control panel returned after restart.
- Validated all 336 catalog entries in both renderer libraries and completed Android unit tests and lint.
- Verified the exact eight-color calibration through the running device API after installation.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.4.7_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.4.6...v0.4.7

## 0.4.6

### Native-Rate Stereo Audio
- Restored the proven coroNET 1 playback architecture: every PCM WAV now plays at its native sample rate from 8 through 48 kHz instead of being software-resampled to a fixed output clock.
- Preserved the original left and right channels for stereo files and duplicated mono files to both I2S slots, removing the tonal distortion introduced by the temporary mono downmix path.
- Reintroduced the coroNET 1 one-second musical fade at natural file boundaries and retained a separate 32 ms ramp for an interrupted track or rapid sound change.
- Preloaded the complete I2S DMA ring with digital silence before output starts and around serialized source-rate clock changes.

### Playback And SD Reliability
- Made the dedicated audio worker the sole owner of sound-file lookup, WAV opening, decoding, and SD library rescans while playback is active.
- Changed touchscreen, Android, browser, BLE, and printer-status actions to enqueue bounded audio requests instead of touching `SD_MMC` concurrently.
- Entered OTA maintenance and stopped the audio worker before reading an SD recovery image, preventing update and playback access from racing on the same card.
- Kept the latest requested sound when controls are pressed rapidly, with a short controlled fade for the interrupted file instead of overlapping playback operations.

### Project Gallery And Documentation
- Expanded the creator reference build and community showcase with complete installations, upper-frame status lighting, inside work lighting, and construction details.
- Updated the architecture, sound guide, OS 1 migration matrix, flashing guide, roadmap, Android metadata, and public development status for the coordinated release.

### Validation
- Built and installed the release candidate on the target JC3248W535 ESP32-S3 hardware.
- Completed a full 48 kHz stereo WAV, rapid mixed sound/stop requests, interrupted playback, and I2S release/recreation with zero restarts, zero I2S write failures, and zero partial-write retries.
- Confirmed clean physical playback and retained approximately 90.8 KB free DMA-capable memory with a 61.4 KB largest DMA block after the balanced stereo driver was recreated.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.4.6_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.4.5...v0.4.6

## 0.4.5

### LED And Audio Scheduling
- Raised normal LED and audio producer tasks above display and application workers while keeping WiFi, Bluetooth controller, and other protocol-critical tasks at higher priorities.
- Preserved the measured 50 FPS LED deadline during simultaneous display refresh, connectivity, Moonraker telemetry, and audio playback.
- Expanded the balanced I2S ring from 16 to 24 blocks of 128 mono frames, providing approximately 139 ms at 22.05 kHz and 64 ms at 48 kHz without returning to the substantially larger OS 1 profile.
- Completed partial I2S writes inside the original bounded producer deadline instead of discarding an unwritten buffer tail, and added separate retry diagnostics.
- Distinguished an intentional stop from a playback failure and stopped active audio cleanly when a selected scenario is suppressed or has no assigned file.

### Documentation And Release Alignment
- Updated the OS 1 migration matrix from its early planning language to the functionality that is now implemented, redesigned, intentionally omitted, or still awaiting hardware qualification.
- Refreshed the public development status, memory and concurrency documentation, roadmap, flashing examples, Android version metadata, and CI development version.
- Reaffirmed the local-network security model without describing the stable feature-complete firmware as an unfinished prototype.

### Validation
- Rebuilt and validated the firmware image, all 336 LED catalog entries, Android unit tests and lint, release packaging, and checksums.
- Retained zero observed LED deadline misses, zero I2S write failures, and zero partial-write retries during the combined target-hardware load profile documented in the architecture notes.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.4.5_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.4.4...v0.4.5

## 0.4.4

### True 50 FPS LED Motion
- Raised the physical LED render schedule from approximately 33 FPS to a true 50 FPS cadence without increasing the SPI clock or allocating additional DMA buffers.
- Reworked continuous waves to interpolate between time phases instead of holding each integer phase, removing visible brightness stepping at the higher refresh rate.
- Added Q8 subpixel positions and neighbouring-pixel light distribution to moving heads, scans, comets, bubbles, sparks, and related effects across every status category.
- Preserved the proven coroNET 1 smoothing rate while scaling it by bounded elapsed time, so a delayed frame cannot return as a large catch-up brightness flash.
- Added explicit missed-deadline accounting and advanced the task schedule past missed frames instead of rendering bursts back-to-back.

### Audio And Display Startup
- Extended the existing 45 ms gain ramp to manual WAV stops, rapid track changes, and interrupted test tones before digital silence is queued.
- Kept natural file endings and playback starts on the same click-resistant ramped path while retaining non-blocking SD playback.
- Forced the LCD backlight low at the first application instruction, rendered and flushed the initial LVGL boot frame in darkness, and only then applied the saved brightness.
- Removed the temporary 100% backlight pulse during display initialization, preventing uninitialized gray LCD memory from becoming visible at startup.

### Project Presentation
- Corrected the Bobby Morgan community-build description so the Snapmaker U1's standard four-tool architecture is not presented as a custom modification.
- Clarified that the current public firmware is stable and feature-complete in its intended scope, while ongoing 0.4.x and 0.5.x work focuses on tuning, optimization, and final refinement.

### Validation
- Rebuilt the complete firmware and validated all 336 documented LED animations against their names, enum entries, and reachable renderer cases.
- Re-ran Android unit tests and lint, release binary checks, Flash Tool package generation, and checksum validation for the coordinated 0.4.4 assets.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.4.4_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.4.2...v0.4.4

## 0.4.2

### Live LED Preview
- Replaced the Android placeholder and browser section diagram with a true 2 FPS view of the firmware-owned LED output over both WiFi and BLE.
- Unified RGBW conversion and physical left-to-right ordering across the coroNET display, Android companion, and local browser panel.
- Added a compact 192-byte frame protocol that reuses the existing PSRAM preview buffer and does not reserve additional DMA memory.
- Moved Android BLE scan start and stop calls off the UI thread to prevent device-specific Bluetooth waits from freezing or crashing the companion app.
- Added the native Android Bluetooth-enable prompt before discovery instead of showing a scan state while the adapter is only in system BLE mode.
- Replaced ambiguous discovery actions with a scalable nearby-device list showing each coroNET name, BLE address, and its own Pair or Connect action.
- Reworked the Android companion for a phone-native portrait layout, including a compact two-row status header and a single-column device manager on narrow screens.
- Removed redundant per-page headings and rebuilt the phone header as one stable row, with connection indicators grouped directly beside the fixed connected-device control.

### Local Temperature History
- Added a responsive two-hour temperature chart to Android Home and the local browser Home page for all four tools, the bed, and the chamber.
- Records every newly received telemetry revision in local phone or browser storage without adding a second Moonraker subscription or increasing ESP32 polling.
- Added an interactive legend for hiding individual series and automatic line emphasis for the currently active tool.
- Extended the existing REST and BLE snapshots with all four already-received tool temperatures while retaining compatibility with earlier V2 BLE packet sizes.

### Android Companion
- Reworked the companion into a phone-native portrait interface that preserves the touchscreen's information hierarchy while using the larger mobile canvas.
- Added a scalable nearby-device list with a distinct Pair or Connect action for every discovered coroNET.
- Corrected Connect for an already saved device so it preserves the paired API token and refreshes the BLE address instead of starting a new pairing session.
- Added the native Bluetooth-enable prompt before discovery and moved BLE scanner operations off the UI thread.
- Added an automatic reconnect after Android grants Bluetooth permissions on first launch.
- Replaced the constrained sound dialog with an inline, continuously loading carousel and clear directional affordances.
- Replaced rapid sound-selection traffic with a latest-choice-wins update and serialized sound-library requests so fast browsing cannot overload the ESP32 audio path.

### Browser Panel And Reliability
- Rebalanced Home and Settings into independent two-column desktop stacks, enlarged desktop typography, and made the coroNET logo accent follow the selected touchscreen theme.
- Removed redundant missing-default warnings from Sound and aligned live metric colors with the temperature chart.
- Kept every incoming chart sample while moving Android history serialization off the interactive state path and reducing persistent writes to a bounded 30-second cadence.
- Moved browser history writes into idle time, avoiding repeated large synchronous `localStorage` work during live updates.
- Made LED preview and audio code consume coherent firmware settings snapshots during concurrent remote changes.
- Re-ran the complete firmware build, Android unit tests and lint, JavaScript syntax validation, binary validation, and all 336 LED catalog checks.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.4.2_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.3.3...v0.4.2

## 0.3.3

### Cross-Project Reliability Audit
- Completed a coordinated audit of the coroNET firmware, Android companion, and local browser panel before physical release-candidate testing.
- Made settings updates from BLE and the browser transactional so rejected values cannot leave a partially applied configuration.
- Added strict animation-index, fan-range, and display-brightness validation across remote control paths.
- Stabilized the telemetry snapshot consumed by the LED task and reduced repeated settings copies inside each rendered frame.

### LED Engine Integrity
- Validated all 336 documented animations against their public names, enum entries, and renderer cases.
- Added an automated catalog validator to CI so missing, duplicated, or unreachable animations block future releases.
- Corrected the Print Running animation so its moving highlight remains inside the completed progress span.
- Added complete cleanup for partial LED buffer, SPI, or task-start failures without increasing steady-state DMA use.

### Android Companion Hardening
- Fixed malformed or obsolete device addresses so they produce a recoverable offline state instead of escaping network error handling.
- Loaded the LED animation catalog directly from the connected firmware, with the bundled catalog retained only as a compatibility fallback.
- Prevented pairing tokens from falling back to unencrypted Android preferences when secure storage is unavailable.
- Added clear recovery feedback when required Bluetooth permissions are denied.
- Added unit coverage for malformed hosts, partial settings responses, and incomplete sound-library entries.

### Security And Release Automation
- Documented the trusted-local-network security boundary of the browser panel and the application-level nature of the displayed BLE pairing code.
- Extended GitHub CI to test and lint Android on every change and validate the full LED catalog alongside the firmware build.
- Established the public path from the `0.3.3` audit baseline through physical testing in `0.4.x`, final interface polish in `0.5.x`, and long-duration stability qualification for `1.0.0`.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.3.3_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.3.2...v0.3.3

## 0.3.2

### Local Browser Control
- Added a complete responsive control panel served directly by coroNET at its local IP address or `http://coronet-xxxx.local/`.
- Mirrored live printer status and the main Home, LED, Vent, Sound, and Settings workflows without requiring a cloud account, external server, or Internet connection.
- Added browser control for LED animations, per-section brightness and DIMM, color calibration, ventilation safeguards, Panda Breath, status sounds, clocks, time zones, Quiet Mode, and firmware updates.

### Browser Security And Reliability
- Added a random per-boot browser session restricted to the device's own hostname or local IP address, keeping the persistent Android companion token private.
- Embedded the compressed panel in firmware with no SD-card, CDN, framework, or external asset dependency.
- Reused firmware-owned state revisions so changes from the touchscreen, Android app, and browser converge on the same saved settings.
- Kept browser access available in Auto and WiFi companion modes while deliberately stopping the HTTP service in BLE-only mode.

### Sound Interface Polish
- Removed technical `DEFAULT ... MISSING` warnings from the browser's status-sound rows.
- Kept assigned custom filenames visible while leaving unassigned rows clean and compact.

### Community Project Showcase
- Added @wlodeka's verified coroNET OS 2 installation beneath the original Snapmaker U1 Top Cover, including interior work-area lighting and touchscreen integration.
- Expanded Bobby Morgan's customized community build gallery and promoted complete-machine photographs that show the full scope of coroNET.
- Added a combined README feature image presenting both community installations and documented the AirGuard 300 Rev. 1 prototype planned for future coroNET integration.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.3.2_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.3.1...v0.3.2

## 0.3.1

### Android Companion Release
- Rebuilt the phone experience as an adaptive landscape console that follows the touchscreen's Home, LED, Vent, Sound, and Settings hierarchy while using the wider phone display effectively.
- Added a signed, directly installable Android APK to the same GitHub release as the matching firmware and Flash Tool package.
- Preserved multi-device management, confirmed pairing, printer Error and Finish notifications, encrypted per-device data, and offline state presentation.

### Reliable Two-Way Control
- Fixed controls that appeared to work only once by reconciling every command with fresh firmware state and settings revisions.
- Added bounded BLE reconnect recovery and WiFi hostname recovery without clearing healthy pairing data.
- Completed synchronized device naming, companion transport selection, UI styles, LED controls, ventilation controls, time settings, and other shared options across the phone and touchscreen.

### Phone Sound Library
- Added folder-based browsing of the microSD sound library without exposing unnecessary full paths.
- Added per-status sound assignment, preview playback, stop, and library rescan over both authenticated WiFi and framed BLE commands.
- Extended compact device snapshots with current audio, quiet-mode, fan, and flap state so the phone reflects real runtime behavior.

### Companion API Completion
- Added authenticated WiFi endpoints and BLE commands for sound-library browsing and playback control.
- Aligned LED calibration, color-remix, ventilation, flap, and Panda control ranges between firmware and Android.
- Updated the public companion protocol and status-sound documentation to match the implemented transports.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.3.1_Flash_Tool.zip` from the assets below and follow the included instructions.
- Android: download `coroNET_Companion.apk` from the assets below and allow installation from the selected browser or file manager.
- Verify downloaded assets with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.3.0...v0.3.1

## 0.3.0

### Companion Resilience
- Added bounded BLE recovery for failed service discovery, notification subscription, and command writes.
- Added WiFi recovery through the stable coroNET mDNS hostname when the previously saved IP address changes.
- Kept the last valid device state and settings in encrypted per-device storage and clearly identified cached data while offline.
- Added shared firmware settings revisions and field-level conflict handling so touchscreen and phone changes converge without unrelated controls overwriting each other.

### Time Zone Accuracy
- Split locations that share an offset but use different daylight-saving rules, including Adelaide/Darwin, Denver/Phoenix, Chicago/Mexico City, Auckland/Fiji, and Cairo/Helsinki.
- Corrected Almaty to UTC+5 and removed misleading location groupings from both the touchscreen and Android catalogs.

### Audio Output Polish
- Preloaded the I2S DMA ring with silence before enabling output.
- Added a controlled silence drain before I2S shutdown and sample-rate reconfiguration to reduce speaker clicks without consuming additional DMA memory.

### Clock And Time Zone Controls
- Added complete 12-hour and 24-hour clock selection on the touchscreen and in the Android companion app.
- Added a lightweight paged time-zone selector on the device and a matching scrollable list in the app.
- Synchronized clock format and POSIX time-zone rules through NVS, BLE, and the authenticated WiFi API.
- Applied time-zone changes immediately to the local clock, including while the device is temporarily offline, and added clear AM/PM presentation in 12-hour mode.

### Boot Color Presentation
- Initially set the full first-run LED Boot Experience to a fixed 150% saturation calibration while leaving daily boot and normal animation calibration under user control; 0.4.7 later unified both boot paths with the saved physical calibration.

### Moonraker Realtime Telemetry
- Added Moonraker WebSocket identification, object discovery, status subscription, stale-session detection, and automatic reconnect handling.
- Merged partial Moonraker notifications into complete telemetry snapshots without moving JSON parsing or network work into the display loop.
- Retained HTTP polling as a fallback and reduced it to a periodic integrity audit while complete realtime telemetry is active.
- Added dynamic subscriptions for available extruders, bed, chamber, progress, print state, and Snapmaker filament metadata.
- Restored chamber-temperature smoothing with a time-aware six-second EMA shared by the display, ventilation control, LED engine, BLE, and WiFi API.

### Network And OTA Stability
- Added the coroNET 1 non-blocking TCP probe pattern so an unavailable printer cannot stall the UI during WebSocket connection attempts.
- Decoupled printer configuration generations from unrelated settings changes, preventing LED or UI edits from invalidating valid printer telemetry.
- Released Moonraker and Panda WebSocket connections during OTA TLS windows to preserve contiguous internal memory for secure updates.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.3.0_Flash_Tool.zip` from the assets below and follow the included instructions.
- Verify downloaded binaries with `SHA256SUMS.txt`; OTA additionally validates `coronet_os2.bin.md5` before installation.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.2.2...v0.3.0

## 0.2.2

### Ventilation Hardware Control
- Kept the proven MCPWM servo-flap control path and completed the local flap calibration workflow.
- Kept the 25 kHz PWM fan-control path with manual and automatic ventilation behavior.
- Preserved hardware-safe output states while settings and connected services initialize.

### DIY Chamber Heater Integration
- Added optional DIY chamber-heater control as a persisted GPIO46 active-HIGH 3.3 V logic output for external driver hardware.
- Added startup and OTA safety handling that forces the optional heater-control output LOW.
- Exposed the heater setting without placing mains or heater current directly on the ESP32-S3 output.

### Panda Breath Integration
- Added on-device Panda Breath discovery through mDNS before falling back to manual configuration.
- Added a lightweight manual Panda Breath host editor in the Android companion app.
- Integrated Panda configuration with the shared coroNET settings model.

### Control Synchronization
- Synchronized ventilation and heater settings across the touchscreen, authenticated WiFi API, BLE, and Android companion app.
- Kept local hardware control functional independently of the companion connection.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.2.2_Flash_Tool.zip` from the assets below and follow the included instructions.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.2.1...v0.2.2

## 0.2.1

### SD Sound Library
- Added a PSRAM-backed SD sound index grouped by folders under `/sounds`.
- Added `GENERAL` and `SD ROOT` compatibility groups for shared and root-level WAV files.
- Kept `/boot.wav` reserved for the boot experience instead of exposing it as a status sound.

### Status Sound Selection
- Added separate sound selection for Print Start, Print Finish, Error, Pause, and Idle.
- Added per-status volume, repeat, playback, stop, and manual rescan controls.
- Added a compact folder-and-file browser that shows filenames without unnecessary full SD paths and keeps six choices visible at once.

### Interface And Runtime Efficiency
- Enlarged arrow-button touch targets and replaced redundant labels with one status-aware selector title.
- Avoided rescanning the SD card during routine UI refreshes.
- Moved manual sound-library rescanning onto the audio worker so card access does not freeze LVGL or the touchscreen.

### Installation
- Existing installations: open **Settings > Firmware update**, select **CHECK**, then **INSTALL**.
- New installations and recovery: download `coroNET_OS_2_0.2.1_Flash_Tool.zip` from the assets below and follow the included instructions.

**Full changelog:** https://github.com/AlphaStudioDE/coroNET_OS_2/compare/v0.2.0...v0.2.1

## 2026-09-03

### Status Sound Library

- Added an intuitive per-event sound selector for Print Start, Print Finish, Error, Pause, and Idle.
- Replaced single-step filename cycling with a lightweight paged picker that displays only five WAV entries at a time.
- Added explicit default-sound selection, alphabetical file ordering, missing-file feedback, playback testing, stopping, per-event volume, and repeat controls.
- Added bounded recursive indexing for `/sounds` subfolders while retaining direct root-level WAV support and reserving `/boot.wav` exclusively for the boot experience.
- Moved manual SD rescanning onto the audio worker task so card access does not block LVGL or make the touchscreen appear frozen.
- Added card-removal detection and a public status-sound setup guide.

### Rebuilt LED Animation Library

- Recreated the complete coroNET animation library for the OS 2 engine instead of copying legacy rendering code directly.
- Added 336 documented animations across Print, Pause, Error, Idle, Finish, and Other, including telemetry-aware effects driven by progress, filament, temperature, timing, connectivity, and ventilation state.
- Standardized all effects on logical Right, Center, Left, Inside, full-OUTER, and mirrored visual-path helpers.
- Added a public LED animation catalog describing every selectable effect and the live printer data it represents.

### Physical LED Color Calibration

- Added an on-device calibration workspace with fixed LCD references for red, orange, yellow, green, cyan, blue, violet, and magenta.
- Added independent hue, saturation, and brightness correction for every reference color, with smooth interpolation for all intermediate animation hues.
- Persisted calibration in device settings while supporting per-color reset, full reset, cancel, and explicit save actions.
- Decoupled the LCD preview from physical correction, allowing the screen to remain the visual target while the SK6812 strip is compensated for its spectrum and diffuser.
- Restored the proven coroNET 1 gamma-2 brightness curve, RGB hue-preserving luminance scaling, saturation-aware RGBW extraction, and animation headroom.
- Removed low-level brightness flooring, reduced pastel contamination during fades, corrected preview direction, and stabilized saturated colors during frame smoothing.
- Replaced Meteor's drifting hue increment with controlled spectrum anchors and a true pure-red pass.

### Discovery, Memory, And Distribution

- Made printer discovery fast and deterministic by trying saved endpoints and mDNS before a bounded network scan.
- Improved DMA headroom through PSRAM-backed LVGL allocation, reduced audio pressure, startup reservations, and expanded heap diagnostics.
- Added validated, versioned Espressif Flash Download Tool packages with a merged factory image, individual binaries, manifests, instructions, and checksums.
- Prepared firmware `0.2.0` for GitHub OTA delivery, same-version reinstall, SD recovery, and clean factory flashing.

## 2026-09-02

### Verified OTA Delivery

- Replaced insecure OTA transport with certificate-bundle-verified HTTPS and a trusted-clock prerequisite.
- Added semantic version comparison, downgrade rejection, explicit same-version reinstall, release asset size validation, ESP32 image-header validation, and mandatory MD5 verification.
- Added an OTA maintenance phase that flushes settings, stops nonessential web/BLE/Panda work, releases audio DMA, and preserves a clear full-screen update state.
- Added ESP32 bootloader rollback validation: a new image is marked valid only after 30 seconds of stable display, touch, and LED startup.
- Deferred Arduino's automatic early OTA acceptance so the full hardware validation window now runs before an image is marked valid.
- Reapplied certificate-bundle trust on every redirected TLS connection and verified a complete public GitHub OTA install on hardware.
- Reduced the OTA worker stack and added a short BLE resource window around secure GitHub requests, preserving the contiguous internal-RAM block required by TLS and eliminating intermittent `HTTP -1` checks. BLE resumes automatically after a version check or failed update attempt.
- Added complete firmware-update controls and confirmation dialogs to the Wi-Fi-connected Android companion.
- Added tag-derived firmware versions and automatic GitHub Release assets, checksums, and release publication.

### Boot Experience

- Added a full first-run coroNET Boot Experience synchronized to `boot.wav`, followed by a smooth handoff into setup.
- Added a quiet 3.5-second daily boot with a complete branded splash and no repetitive startup audio.
- Added coordinated screen and RGBW LED transitions into the live printer animation without a hard final-frame cut.
- Refined the quick LED signature so Right, Center, and Left each present a complete color spectrum with continuous tonal boundaries.
- Kept connectivity services active during the quick boot while protecting full-boot LED and audio timing from background work.

### Companion Settings Reliability

- Fixed controls that could appear to work only once when a BLE settings snapshot was delayed or missed.
- Added immediate local settings feedback so button labels, switches, and subsequent actions always use the latest selected value.
- Added a delayed BLE settings reconciliation after edits without allowing an older response to undo a newer choice.
- Serialized WiFi settings writes and guarded periodic refreshes against stale out-of-order responses.
- Fixed the Android companion rejecting every protocol V2 frame as if it were an obsolete V1 frame.
- Added automatic BLE reconnection and a verified WiFi-to-BLE fallback when the preferred local HTTP endpoint is unavailable.
- Kept BLE connected while probing WiFi and prevented failed WiFi polls from replacing a healthy BLE session with an Offline state.

### Printer Telemetry Contract

- Added explicit telemetry validity plus independent telemetry, connection, and printer-event revisions.
- Validated Moonraker object-query structure before accepting data; incomplete JSON can no longer silently become an Idle printer state.
- Made reconnect establish a clean baseline instead of synthesizing Start, Finish, Error, or Idle events.
- Moved audio, Error screen wake, Panda post-print tempering, BLE delivery, WiFi state, and Android notifications onto one shared printer-transition sequence.
- Extended companion protocol V2 and the WiFi API with telemetry freshness and transition metadata, preventing duplicate phone notifications after reconnect or transport switching.
- Revalidated firmware and Android builds, then verified hardware startup with zero dropped LED frames and stable DMA/PSRAM recovery.

## 2026-09-01

### Product UI

- Added complete Home, LED, Vent, Sound, and Settings surfaces using one-screen-at-a-time LVGL ownership.
- Added Coronet, Graphite, Aurora, and Minimal skins with dark, light, and automatic color modes.
- Added configurable accent hue, display brightness, temporary quiet mode, and seven low-cost clock styles.
- Added touch inactivity screen saver behavior and Error-only printer wake.
- Expanded Home telemetry with material, color, duration, ETA, fan, and flap state.

### Hardware Services

- Added indexed SD WAV playback with PSRAM staging and startup/status asset validation.
- Added the layered RGBW LED engine, logical four-section mapping, ambient inside mode, dimming, mirroring, and preview control.
- Added local PWM fan and servo flap control with 500-2500 us calibration, reverse, limits, and fail-safe outputs.
- Added the optional Panda Breath workflow service for automatic, preheat, tempering, forced-on, and filament-drying modes.

### Connectivity And Recovery

- Expanded the authenticated WiFi API to synchronize UI, LED, sound, vent, Panda, and quiet settings.
- Expanded BLE with framed settings groups and bounded operational settings patches.
- Added GitHub OTA check/install/reinstall, progress state, maintenance preparation, app-valid marking, factory reset, and `/firmware.bin` SD recovery.
- Fixed rapid clock transitions by clearing style-specific LVGL object references before every rebuild.

### Android Companion

- Added a native Android application with multi-device storage, BLE discovery, queued GATT operations, framed payload reassembly, and automatic first pairing.
- Added encrypted per-device token storage, preferred local WiFi control, bidirectional settings refresh, and Error/Finish notifications.
- Added mirrored Home, LED, Vent, Sound, and Settings screens and verified the debug APK on the minimum API 26 emulator.

### Reliability Pass

- Serialized Android MTU negotiation and GATT service discovery, with a bounded fallback when a phone does not return the MTU callback.
- Added notification reception compatible with Android 8 through Android 12 while retaining the Android 13+ callback path.
- Bounded BLE command queues and fragment assembly by source, type, message ID, count, payload size, and timeout; complete payloads are now assembled with one allocation.
- Isolated printer transition history per saved device and removed stale scan timers when switching or reconnecting devices.
- Disabled Android cloud backup for Keystore-bound pairing data and prevented stale GitHub release metadata from being reused by OTA.
- Revalidated firmware startup, stable heap/DMA recovery, OTA metadata checks, repeated BLE reconnects, framed settings transfer, and the API 26 app lifecycle.
