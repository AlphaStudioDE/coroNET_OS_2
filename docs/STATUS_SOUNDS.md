# Status Sounds

coroNET OS 2 can play a separate WAV file when a print starts, completes, pauses, reports an error, or returns to idle. Every event keeps its own selected file, volume, and repeat setting.

## Prepare The SD Card

Place WAV files either in the card root or in `/sounds`. Create folders directly inside `/sounds` to organize the on-device picker. Deeper subfolders are supported up to three levels and are grouped under their top-level `/sounds` folder. Files placed directly in `/sounds` appear under **General**, while legacy root files appear under **SD Root**. The selectable library is bounded to 64 files and sorted alphabetically, keeping startup time and PSRAM use predictable.

Recommended layout:

```text
/
|-- boot.wav
`-- sounds/
    |-- start.wav
    |-- finish.wav
    |-- error.wav
    |-- pause.wav
    |-- idle.wav
    `-- custom/
        |-- soft-finish.wav
        `-- workshop-warning.wav
```

`/boot.wav` is reserved for the full first-run Boot Experience and is intentionally hidden from the status-sound picker.

## Supported WAV Format

- uncompressed PCM WAV;
- mono or stereo input, mixed to the single coroNET speaker;
- 8-bit or 16-bit samples;
- sample rates from 8 kHz through 48 kHz.

For consistent quality and modest SD bandwidth, 16-bit mono at 22.05 kHz is recommended. File paths must be shorter than 65 characters.

## Assign A Sound

1. Open **Sound** on the coroNET touchscreen.
2. Use the left and right arrows to select **Print Start**, **Print Finish**, **Error**, **Pause**, or **Idle**.
3. Press the large **Selected Sound** button.
4. Select a folder with the upper arrow pair, then choose a WAV by its filename. Full SD paths remain hidden.
5. Set volume and repeat behavior, then press **Test**.
6. Touch the screen or press **Stop** to end playback.

The default option resolves `/sounds/start.wav`, `/sounds/finish.wav`, `/sounds/error.wav`, `/sounds/pause.wav`, or `/sounds/idle.wav`, with the same filenames in the SD root as fallback. If a custom file is removed, the screen marks it as missing and the matching default file is used when available.

Use **Rescan** after inserting a card or changing its files while coroNET is running. Scanning runs in the audio worker rather than the display task, so the interface remains responsive.

## Runtime Behavior

- Printer transitions are consumed from the shared, revisioned event stream; reconnecting establishes a baseline and does not generate a false status sound.
- Error can repeat until the user touches the display, presses Stop, or another audio request replaces it.
- Quiet mode suppresses configured sounds. Error can bypass Quiet mode when **Allow Error** is enabled.
- The Snake print animation delays its Finish sound until the completion burst has ended.
- WAV data is read in small blocks through a PSRAM staging buffer. Only the I2S descriptor ring uses DMA-capable internal memory.
