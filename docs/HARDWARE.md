# Hardware

coroNET OS 2 targets the same hardware and GPIO layout as coroNET 1.

## Board

- ESP32-S3 based `JC3248W535`
- 16 MB flash
- OPI PSRAM
- Integrated display, touch, microSD, and I2S audio amplifier

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
| DIY chamber heater | 46 | Optional heater output |

## LED Layout

- `LED_COUNT = 60`
- Outer LEDs: `0..41`
- Inside LEDs: `42..59`
- Physical section directions are inherited from coroNET 1 and will be expressed through logical mapping helpers in the OS 2 LED engine.
