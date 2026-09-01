# coroNET OS 2 Bill Of Materials

This BOM covers the hardware shared by coroNET OS 1 and OS 2. Verify the selected variant, ratings, dimensions, shipping, and availability before ordering. Supplier links are examples from the prototype build, not endorsements or guaranteed permanent sources.

For wiring and assembly, read [ASSEMBLY.md](ASSEMBLY.md).

## Required Parts

| Category | Part / variant | Qty | Prototype price | Source |
| --- | --- | ---: | ---: | --- |
| Controller | JC3248W535 ESP32-S3 touchscreen, 16 MB flash and OPI PSRAM | 1 | 21.99 EUR | [AliExpress 1005007566315926](https://de.aliexpress.com/item/1005007566315926.html) |
| Power supply | Regulated 5 V / 10 A supply | 1 | 11.99 EUR | [AliExpress 1005004121728138](https://de.aliexpress.com/item/1005004121728138.html) |
| Power cable | 5.5 x 2.5 mm male-to-female extension, 20 cm | 1 | 1.79 EUR | [AliExpress 1005010208980172](https://de.aliexpress.com/item/1005010208980172.html) |
| LEDs | SK6812 RGBNW, 1 m, 60 LEDs, IP20 | 1 | 5.29 EUR | [AliExpress 1005005824057524](https://de.aliexpress.com/item/1005005824057524.html) |
| Speaker | 4 ohm / 3 W with 1.25 mm connector | 1 | 1.69 EUR | [AliExpress 1005008267900755](https://de.aliexpress.com/item/1005008267900755.html) |
| Servo | Micro servo for ventilation flap | 1 | 1.95 EUR | [AliExpress 1005008315780030](https://de.aliexpress.com/item/1005008315780030.html) |
| Fan | Noctua NF-A4x10 5 V PWM or compatible 5 V PWM fan | 1 | about 20 EUR | [AliExpress 1005005402810322](https://de.aliexpress.com/item/1005005402810322.html) |
| Capacitor | 1000 uF / 10 V electrolytic, placed at LED input | 1+ | 2.55 EUR | [AliExpress 1005002075527957](https://de.aliexpress.com/item/1005002075527957.html) |
| Wire | 24 AWG to 28 AWG wire set | 1 set | 8.99 EUR | [AliExpress 1005007670937847](https://de.aliexpress.com/item/1005007670937847.html) |
| Hardware | M2.5 screw / spacer kit | 1 set | 7.69 EUR | [AliExpress 1005009682333826](https://de.aliexpress.com/item/1005009682333826.html) |
| FFC/FPC | 8P, same direction, 15-20 cm | 1 | 4.79 EUR | [AliExpress 1005004462513465](https://de.aliexpress.com/item/1005004462513465.html) |
| FFC/FPC | 4P, same direction, 20-30 cm | 1 | 4.19 EUR | [AliExpress 1005004462513465](https://de.aliexpress.com/item/1005004462513465.html) |

The required-parts subtotal shown above is approximately 92.91 EUR before shipping and optional parts. Prices change frequently. Wire, screws, connectors, and common electronics may already be available in a workshop and can reduce the real cost.

## Recommended Protection And Signal Integrity

| Part | Qty | Notes | Source |
| --- | ---: | --- | --- |
| 10 A fuse / inline protection | 0-1 | Strongly recommended. Omitting it is the builder's responsibility. | [AliExpress 1005009895179310](https://de.aliexpress.com/item/1005009895179310.html) |
| SN74AHCT125N level shifter / buffer | 0-1 | Recommended between 3.3 V ESP32 data and 5 V LEDs. | [AliExpress 1005010466137824](https://de.aliexpress.com/item/1005010466137824.html) |
| Solderless DC connector set | 0-1 set | Optional serviceable power connections; verify current rating. | [AliExpress 1005008713574522](https://de.aliexpress.com/item/1005008713574522.html) |

The project can be assembled with properly soldered, insulated, and strain-relieved wiring instead of optional connector kits.

## Optional Build Aids

- [Cable terminal assortment](https://de.aliexpress.com/item/1005006963063019.html)
- [Connector and crimp assortment](https://de.aliexpress.com/item/1005005961638278.html)
- [Inline cable connectors](https://de.aliexpress.com/item/1005008599216565.html)

## Integrated Parts

The standard JC3248W535 board already provides:

- the ESP32-S3, display, and touch controller;
- a microSD slot;
- I2S audio output/amplifier path.

Do not add a separate I2S amplifier or external SD module unless building a different hardware revision.

The controller kit should include a 500 MB microSD card. If it does not, use a tested 500 MB to 2 GB card. Larger cards are not recommended for the standard build until they have been validated with this exact board and firmware.

## Mechanical Parts

The standard printable project is [hardware/print/coroNET.3mf](hardware/print/coroNET.3mf). It contains the enclosure parts and slicer settings inherited from the coroNET hardware platform.

Additional items depend on the print and mounting method:

- LED diffuser or light-guide material;
- heat-set inserts, spacers, or mounting screws;
- cable clips, sleeving, and strain relief;
- appropriate magnets or brackets for optional mounting designs.

## Hardware Values To Validate Before Release

- exact servo model, torque, and safe pulse range under load;
- fan replacement compatibility and minimum reliable PWM;
- final wire gauge for each current path;
- final fuse holder and power-distribution layout;
- final diffuser material and enclosure revision;
- SD card filesystems and capacities tested on the target board.
