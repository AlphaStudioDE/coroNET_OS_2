#include "UiTheme.h"

namespace coronet::ui {

ThemeColor ColorBackground{0x071018};
ThemeColor ColorSurface{0x0D1A24};
ThemeColor ColorSurfaceRaised{0x132531};
ThemeColor ColorBorder{0x29414F};
ThemeColor ColorText{0xF4F8FA};
ThemeColor ColorMuted{0x8FAAB7};
ThemeColor ColorCyan{0x27D3C2};
ThemeColor ColorCyanDark{0x0F665F};
ThemeColor ColorAmber{0xF1B84B};
ThemeColor ColorRed{0xFF6B6B};
ThemeColor ColorGreen{0x55D88A};

namespace {

uint32_t hsv(uint16_t hue, uint8_t saturation, uint8_t value) {
    hue %= 360;
    const uint8_t region = hue / 60;
    const uint16_t remainder = (hue % 60) * 255 / 60;
    const uint8_t p = static_cast<uint8_t>(value * (255 - saturation) / 255);
    const uint8_t q = static_cast<uint8_t>(value * (255 - saturation * remainder / 255) / 255);
    const uint8_t t = static_cast<uint8_t>(value * (255 - saturation * (255 - remainder) / 255) / 255);
    uint8_t r = value, g = t, b = p;
    switch (region) {
        case 1: r = q; g = value; b = p; break;
        case 2: r = p; g = value; b = t; break;
        case 3: r = p; g = q; b = value; break;
        case 4: r = t; g = p; b = value; break;
        case 5: r = value; g = p; b = q; break;
        default: break;
    }
    return (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) | b;
}

}

void applyTheme(uint8_t skin, uint8_t colorMode, uint16_t accentHueDegrees, bool daytime) {
    const bool light = colorMode == 1U || (colorMode == 2U && daytime);
    if (light) {
        if (skin == 1U) {
            ColorBackground.value = 0xF0F1F2; ColorSurface.value = 0xFFFFFF;
            ColorSurfaceRaised.value = 0xE4E7E9; ColorBorder.value = 0xB8C0C5;
        } else if (skin == 2U) {
            ColorBackground.value = 0xF1F8F5; ColorSurface.value = 0xFFFFFF;
            ColorSurfaceRaised.value = 0xDDEFE8; ColorBorder.value = 0xA8C7BC;
        } else if (skin == 3U) {
            ColorBackground.value = 0xFAFAFA; ColorSurface.value = 0xFFFFFF;
            ColorSurfaceRaised.value = 0xEEEEEE; ColorBorder.value = 0xCCCCCC;
        } else {
            ColorBackground.value = 0xEFF7F8; ColorSurface.value = 0xFFFFFF;
            ColorSurfaceRaised.value = 0xDDEDEF; ColorBorder.value = 0xAEC8CD;
        }
        ColorText.value = 0x142027;
        ColorMuted.value = 0x607780;
    } else {
        if (skin == 1U) {
            ColorBackground.value = 0x0B0C0E; ColorSurface.value = 0x15171A;
            ColorSurfaceRaised.value = 0x202329; ColorBorder.value = 0x3B4148;
        } else if (skin == 2U) {
            ColorBackground.value = 0x07110D; ColorSurface.value = 0x0E1C17;
            ColorSurfaceRaised.value = 0x173029; ColorBorder.value = 0x2C5145;
        } else if (skin == 3U) {
            ColorBackground.value = 0x050607; ColorSurface.value = 0x101214;
            ColorSurfaceRaised.value = 0x1A1D20; ColorBorder.value = 0x33383D;
        } else {
            ColorBackground.value = 0x071018; ColorSurface.value = 0x0D1A24;
            ColorSurfaceRaised.value = 0x132531; ColorBorder.value = 0x29414F;
        }
        ColorText.value = 0xF4F8FA;
        ColorMuted.value = skin == 2U ? 0x92B5A7 : 0x8FAAB7;
    }
    const uint8_t accentValue = light ? 190 : 230;
    ColorCyan.value = hsv(accentHueDegrees, light ? 210 : 205, accentValue);
    ColorCyanDark.value = hsv(accentHueDegrees, 210, light ? 150 : 105);
    ColorAmber.value = 0xF1B84B;
    ColorRed.value = light ? 0xD94343 : 0xFF6B6B;
    ColorGreen.value = light ? 0x289C5B : 0x55D88A;
}

}
