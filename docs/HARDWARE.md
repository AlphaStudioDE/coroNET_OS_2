# Hardware

coroNET OS 2 targets the same hardware and GPIO layout as coroNET 1.

## Board

- ESP32-S3 based `JC3248W535`
- dual-core CPU at 240 MHz
- 16 MB QIO flash at 80 MHz
- 8 MB OPI PSRAM at 80 MHz
- Integrated display, touch, microSD, and I2S audio amplifier

The 80 MHz OPI PSRAM profile is intentional. [ESP-IDF 5.4 marks 120 MHz
Octal PSRAM as experimental](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/kconfig.html#config-spiram-speed)
and warns about temperature-dependent instability, which is unsuitable for a
controller mounted near a 3D printer. The custom PlatformIO board definition
and compile-time guards keep this profile fixed.

PlatformIO's build summary reports the ESP32-S3 internal static RAM limit
(320 KB); it does not include the external PSRAM. At runtime, `MemoryService`
checks that all 8 MB of PSRAM are visible, and `SystemHealth` reports free and
largest blocks independently for internal RAM, DMA-capable RAM, and PSRAM.

## Display And Touch

- LCD controller: AXS15231B over QSPI
- Touch controller: AXS15231B-compatible I2C touch path from the original JC3248W535 BSP
- Logical display rotation: 90 degrees
- Backlight: PWM on GPIO 1 through the local BSP
- LVGL draw canvas: PSRAM
- LCD transfer windows: DMA-capable internal RAM, intentionally kept small

## GPIO Map

| Function | GPIO | Notes |
| --- | ---: | --- |
| SK6812 data | 15 | LED data output |
| SK6812 SPI SCK dummy | 16 | Dummy/unconnected clock for SPI-encoded LED output |
| I2S BCK | 42 | Built-in audio amplifier |
| I2S LRCK | 2 | Built-in audio amplifier |
| I2S DOUT | 41 | Built-in audio amplifier |
| SDMMC CLK | 12 | 1-bit SD mode |
| SDMMC CMD | 11 | 1-bit SD mode |
| SDMMC D0 | 13 | 1-bit SD mode |
| Servo flap PWM | 9 | Servo-controlled air flap |
| Fan PWM | 14 | 5 V PWM fan |
| DIY chamber heater | 46 | Optional active-HIGH 3.3 V logic output; external driver required; strapping pin must not be driven during boot |

## LED Layout

- `LED_COUNT = 60`
- Outer LEDs: `0..41`
- Inside LEDs: `42..59`
- Physical section directions are inherited from coroNET 1 and normalized through the OS 2 engine's logical mapping helpers.

## Panda Breath Network Setup

Panda Breath does not require an additional GPIO connection. Use `DISCOVER` on the VENT page to resolve `PandaBreath.local` through mDNS. If the device uses a custom hostname or mDNS is unavailable, enter its hostname or IPv4 address in the Android companion app. A failed discovery does not erase a previously saved address.
