#pragma once

#include <stdint.h>

namespace coronet::ui {

struct ThemeColor {
    uint32_t value = 0;
    constexpr operator uint32_t() const { return value; }
};

extern ThemeColor ColorBackground;
extern ThemeColor ColorSurface;
extern ThemeColor ColorSurfaceRaised;
extern ThemeColor ColorBorder;
extern ThemeColor ColorText;
extern ThemeColor ColorMuted;
extern ThemeColor ColorCyan;
extern ThemeColor ColorCyanDark;
extern ThemeColor ColorAmber;
extern ThemeColor ColorRed;
extern ThemeColor ColorGreen;

void applyTheme(uint8_t skin, uint8_t colorMode, uint16_t accentHueDegrees, bool daytime);

static constexpr int ScreenWidth = 480;
static constexpr int ScreenHeight = 320;
static constexpr int CornerRadius = 6;

}
