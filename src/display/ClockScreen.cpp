#include "ClockScreen.h"

#include <Arduino.h>
#include <time.h>

#include "../core/SystemState.h"
#include "../settings/SettingsService.h"
#include "UiTheme.h"

namespace coronet {

namespace {

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent, const char* text, uint32_t color,
                const lv_font_t* font, int x, int y, int width = LV_SIZE_CONTENT) {
    lv_obj_t* object = lv_label_create(parent);
    styleText(object, color, font);
    if (width != LV_SIZE_CONTENT) lv_obj_set_width(object, width);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    return object;
}

void styleTrack(lv_obj_t* object, uint32_t color) {
    lv_obj_set_style_radius(object, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* createRetroSegment(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                             lv_coord_t width, lv_coord_t height) {
    lv_obj_t* segment = lv_obj_create(parent);
    lv_obj_remove_style_all(segment);
    lv_obj_set_pos(segment, x, y);
    lv_obj_set_size(segment, width, height);
    lv_obj_set_style_radius(segment, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(segment, 0, LV_PART_MAIN);
    return segment;
}

void createRetroDigit(lv_obj_t* parent, lv_obj_t** segments, lv_coord_t x, lv_coord_t y) {
    segments[0] = createRetroSegment(parent, x + 12, y, 40, 8);
    segments[1] = createRetroSegment(parent, x + 52, y + 8, 8, 36);
    segments[2] = createRetroSegment(parent, x + 52, y + 54, 8, 36);
    segments[3] = createRetroSegment(parent, x + 12, y + 90, 40, 8);
    segments[4] = createRetroSegment(parent, x, y + 54, 8, 36);
    segments[5] = createRetroSegment(parent, x, y + 8, 8, 36);
    segments[6] = createRetroSegment(parent, x + 12, y + 45, 40, 8);
}

uint8_t retroMaskForCharacter(char character) {
    switch (character) {
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case '-': return 0x40;
        default: return 0x00;
    }
}

void styleRetroSegment(lv_obj_t* segment, bool active) {
    if (!segment) return;
    const lv_color_t activeColor = lv_color_hex(ui::ColorCyan);
    const lv_color_t inactiveColor = lv_color_hex(ui::ColorCyanDark);
    lv_obj_set_style_bg_color(segment, active ? activeColor : inactiveColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(segment, active ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(segment, active ? 8 : 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(segment, activeColor, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(segment, active ? LV_OPA_40 : LV_OPA_0, LV_PART_MAIN);
}

}

void ClockScreen::begin(ClockStyle style) {
    style_ = style;
    lastSecond_ = UINT32_MAX;
    timeLabel_ = nullptr;
    secondsLabel_ = nullptr;
    dateLabel_ = nullptr;
    statusLabel_ = nullptr;
    retroPanel_ = nullptr;
    retroSuffixLabel_ = nullptr;
    retroColonVisible_ = false;
    memset(retroSegments_, 0, sizeof(retroSegments_));
    memset(retroColon_, 0, sizeof(retroColon_));
    memset(retroLastDigits_, 0, sizeof(retroLastDigits_));
    memset(retroLastSuffix_, 0, sizeof(retroLastSuffix_));
    memset(bars_, 0, sizeof(bars_));
    memset(indicators_, 0, sizeof(indicators_));
    memset(circles_, 0, sizeof(circles_));
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    label(root_, "coroNET", ui::ColorMuted, &lv_font_montserrat_12, 18, 15);

    switch (style_) {
        case ClockStyle::Retro: buildRetro(); break;
        case ClockStyle::Analog: buildAnalog(); break;
        case ClockStyle::LinearHorizon: buildLinear(); break;
        case ClockStyle::Bauhaus: buildBauhaus(); break;
        case ClockStyle::DotMatrix: buildMatrix(); break;
        case ClockStyle::Arc: buildArc(); break;
        case ClockStyle::Digital:
        default: buildDigital(); break;
    }

    lv_obj_t* wakeLayer = lv_obj_create(root_);
    lv_obj_remove_style_all(wakeLayer);
    lv_obj_set_size(wakeLayer, ui::ScreenWidth, ui::ScreenHeight);
    lv_obj_set_pos(wakeLayer, 0, 0);
    lv_obj_add_flag(wakeLayer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(wakeLayer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(wakeLayer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(wakeLayer, touchEvent, LV_EVENT_RELEASED, nullptr);

    lv_scr_load_anim(root_, LV_SCR_LOAD_ANIM_FADE_ON, 120, 0, true);
    state().screenSaverActive = true;
    update();
}

void ClockScreen::buildDigital() {
    timeLabel_ = label(root_, "--:--", ui::ColorText, &lv_font_montserrat_48, 42, 91, 396);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    secondsLabel_ = label(root_, "--", ui::ColorCyan,
                          &lv_font_montserrat_22, 392, 135, 48);
    dateLabel_ = label(root_, "Waiting for time", ui::ColorMuted,
                       &lv_font_montserrat_14, 42, 170, 396);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void ClockScreen::buildRetro() {
    lv_obj_set_style_bg_color(root_, lv_color_black(), LV_PART_MAIN);

    retroPanel_ = lv_obj_create(root_);
    lv_obj_remove_style_all(retroPanel_);
    lv_obj_set_size(retroPanel_, 432, 110);
    lv_obj_align(retroPanel_, LV_ALIGN_CENTER, 0, -22);
    lv_obj_clear_flag(retroPanel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(retroPanel_, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(retroPanel_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(retroPanel_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(retroPanel_, lv_color_hex(ui::ColorCyanDark), LV_PART_MAIN);
    lv_obj_set_style_border_opa(retroPanel_, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_radius(retroPanel_, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(retroPanel_, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(retroPanel_, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(retroPanel_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(retroPanel_, LV_OPA_20, LV_PART_MAIN);

    const lv_coord_t digitX[4] = {58, 134, 222, 298};
    for (uint8_t digit = 0; digit < 4; ++digit) {
        createRetroDigit(retroPanel_, retroSegments_[digit], digitX[digit], 8);
    }
    retroColon_[0] = createRetroSegment(retroPanel_, 200, 34, 10, 10);
    retroColon_[1] = createRetroSegment(retroPanel_, 200, 66, 10, 10);

    retroSuffixLabel_ = label(retroPanel_, "", ui::ColorCyan,
                              &lv_font_montserrat_20, 354, 38, 60);
    lv_obj_set_style_text_align(retroSuffixLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    dateLabel_ = label(root_, "Waiting for Wi-Fi / NTP", 0xFFFFFF,
                       &lv_font_montserrat_20, 20, 0, 440);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(dateLabel_, LV_ALIGN_CENTER, 0, 58);

    statusLabel_ = label(root_, "Tap to wake", 0x9CA3AF,
                         &lv_font_montserrat_16, 20, 0, 440);
    lv_obj_set_style_text_align(statusLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(statusLabel_, LV_ALIGN_CENTER, 0, 86);

    updateRetro(nullptr);
}

void ClockScreen::updateRetro(const struct tm* local) {
    char digits[5] = {'-', '-', '-', '-', '\0'};
    const char* suffix = "";
    if (local) {
        int hour = local->tm_hour;
        if (!settingsService().settings().clock24Hour) {
            const int hour12 = hour % 12 ? hour % 12 : 12;
            digits[0] = hour12 >= 10 ? static_cast<char>('0' + hour12 / 10) : ' ';
            digits[1] = static_cast<char>('0' + hour12 % 10);
            suffix = hour >= 12 ? "PM" : "AM";
        } else {
            digits[0] = static_cast<char>('0' + hour / 10);
            digits[1] = static_cast<char>('0' + hour % 10);
        }
        digits[2] = static_cast<char>('0' + local->tm_min / 10);
        digits[3] = static_cast<char>('0' + local->tm_min % 10);
    }

    for (uint8_t digit = 0; digit < 4; ++digit) {
        if (retroLastDigits_[digit] == digits[digit]) continue;
        retroLastDigits_[digit] = digits[digit];
        const uint8_t mask = retroMaskForCharacter(digits[digit]);
        for (uint8_t segment = 0; segment < 7; ++segment) {
            styleRetroSegment(retroSegments_[digit][segment], (mask & (1U << segment)) != 0);
        }
    }

    const bool colonVisible = local != nullptr;
    if (retroLastDigits_[4] == '\0' || retroColonVisible_ != colonVisible) {
        retroColonVisible_ = colonVisible;
        retroLastDigits_[4] = '!';
        styleRetroSegment(retroColon_[0], colonVisible);
        styleRetroSegment(retroColon_[1], colonVisible);
    }

    if (strncmp(retroLastSuffix_, suffix, sizeof(retroLastSuffix_)) != 0) {
        strlcpy(retroLastSuffix_, suffix, sizeof(retroLastSuffix_));
        lv_label_set_text(retroSuffixLabel_, retroLastSuffix_);
    }
}

void ClockScreen::buildAnalog() {
    lv_obj_t* ring = lv_obj_create(root_);
    lv_obj_set_size(ring, 224, 224);
    lv_obj_set_pos(ring, 128, 47);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    for (uint8_t i = 0; i < 3; ++i) {
        circles_[i] = lv_line_create(root_);
        lv_line_set_points(circles_[i], handPoints_[i], 2);
        lv_obj_set_style_line_width(circles_[i], i == 2 ? 2 : 5, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(circles_[i], true, LV_PART_MAIN);
        lv_obj_set_style_line_color(circles_[i], lv_color_hex(i == 2 ? ui::ColorAmber : i == 1 ? ui::ColorCyan : ui::ColorText), LV_PART_MAIN);
    }
    lv_obj_t* center = lv_obj_create(root_);
    lv_obj_set_size(center, 12, 12);
    lv_obj_set_pos(center, 234, 153);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    styleTrack(center, ui::ColorCyan);
    dateLabel_ = label(root_, "", ui::ColorMuted, &lv_font_montserrat_12, 150, 282, 180);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void ClockScreen::buildLinear() {
    timeLabel_ = label(root_, "--:--", ui::ColorText, &lv_font_montserrat_32, 24, 46, 432);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    const uint32_t colors[3] = {ui::ColorAmber, ui::ColorCyan, ui::ColorGreen};
    static const char* captions[3] = {"HOUR", "MINUTE", "SECOND"};
    for (uint8_t i = 0; i < 3; ++i) {
        label(root_, captions[i], ui::ColorMuted, &lv_font_montserrat_10, 28, 113 + i * 48);
        bars_[i] = lv_obj_create(root_);
        lv_obj_set_size(bars_[i], 370, 10);
        lv_obj_set_pos(bars_[i], 82, 115 + i * 48);
        styleTrack(bars_[i], ui::ColorSurfaceRaised);
        indicators_[i] = lv_obj_create(bars_[i]);
        lv_obj_set_height(indicators_[i], 10);
        lv_obj_set_pos(indicators_[i], 0, 0);
        styleTrack(indicators_[i], colors[i]);
    }
    dateLabel_ = label(root_, "", ui::ColorMuted, &lv_font_montserrat_12, 28, 274, 424);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
}

void ClockScreen::buildBauhaus() {
    static const int sizes[3] = {200, 142, 82};
    const uint32_t colors[3] = {ui::ColorAmber, ui::ColorCyan, ui::ColorRed};
    for (uint8_t i = 0; i < 3; ++i) {
        circles_[i] = lv_obj_create(root_);
        lv_obj_set_size(circles_[i], sizes[i], sizes[i]);
        lv_obj_set_style_radius(circles_[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(circles_[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(circles_[i], lv_color_hex(colors[i]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(circles_[i], i == 0 ? LV_OPA_50 : i == 1 ? LV_OPA_70 : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(circles_[i], 0, LV_PART_MAIN);
    }
    timeLabel_ = label(root_, "--:--", ui::ColorText, &lv_font_montserrat_32, 260, 238, 190);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
}

void ClockScreen::buildMatrix() {
    timeLabel_ = label(root_, "--:--", ui::ColorGreen, &lv_font_montserrat_22, 40, 96, 400);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(timeLabel_, 10, LV_PART_MAIN);
    lv_obj_set_style_transform_zoom(timeLabel_, 420, LV_PART_MAIN);
    secondsLabel_ = label(root_, "", ui::ColorAmber, &lv_font_montserrat_16, 40, 198, 400);
    lv_obj_set_style_text_align(secondsLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    dateLabel_ = label(root_, "", ui::ColorMuted, &lv_font_montserrat_12, 40, 264, 400);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void ClockScreen::buildArc() {
    static const int sizes[3] = {230, 182, 134};
    const uint32_t colors[3] = {ui::ColorAmber, ui::ColorCyan, ui::ColorGreen};
    for (uint8_t i = 0; i < 3; ++i) {
        bars_[i] = lv_arc_create(root_);
        lv_obj_set_size(bars_[i], sizes[i], sizes[i]);
        lv_obj_set_pos(bars_[i], 240 - sizes[i] / 2, 158 - sizes[i] / 2);
        lv_arc_set_rotation(bars_[i], 270);
        lv_arc_set_bg_angles(bars_[i], 0, 360);
        lv_arc_set_range(bars_[i], 0, i == 0 ? 12 : 60);
        lv_obj_remove_style(bars_[i], nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(bars_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(bars_[i], 7, LV_PART_MAIN);
        lv_obj_set_style_arc_color(bars_[i], lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
        lv_obj_set_style_arc_width(bars_[i], 7, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(bars_[i], lv_color_hex(colors[i]), LV_PART_INDICATOR);
    }
    timeLabel_ = label(root_, "--:--", ui::ColorText, &lv_font_montserrat_28, 160, 137, 160);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    dateLabel_ = label(root_, "", ui::ColorMuted, &lv_font_montserrat_12, 120, 285, 240);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void ClockScreen::update() {
    if (!root_) return;
    time_t now = time(nullptr);
    struct tm local = {};
    if (now < 1700000000 || !localtime_r(&now, &local)) {
        if (dateLabel_) lv_label_set_text(dateLabel_, state().wifiConnected ? "Synchronizing time" : "Waiting for Wi-Fi");
        return;
    }
    if (lastSecond_ == static_cast<uint32_t>(local.tm_sec)) return;
    lastSecond_ = local.tm_sec;
    const AppSettings& settings = settingsService().settings();
    int hour = local.tm_hour;
    if (!settings.clock24Hour) {
        hour %= 12;
        if (!hour) hour = 12;
    }
    const char* period = local.tm_hour < 12 ? "AM" : "PM";
    char timeText[16] = "";
    const bool periodInTime = !settings.clock24Hour && style_ == ClockStyle::Bauhaus;
    if (periodInTime) {
        snprintf(timeText, sizeof(timeText), "%02d:%02d %s", hour, local.tm_min, period);
    } else {
        snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, local.tm_min);
    }
    char dateText[32] = "";
    char dateOnly[24] = "";
    strftime(dateOnly, sizeof(dateOnly), "%a, %d %b %Y", &local);
    if (!settings.clock24Hour && !periodInTime) {
        snprintf(dateText, sizeof(dateText), "%s  |  %s", period, dateOnly);
    } else {
        strlcpy(dateText, dateOnly, sizeof(dateText));
    }
    if (timeLabel_) lv_label_set_text(timeLabel_, timeText);
    if (secondsLabel_) lv_label_set_text_fmt(secondsLabel_, "%02d", local.tm_sec);
    if (dateLabel_) lv_label_set_text(dateLabel_, dateText);

    if (style_ == ClockStyle::Retro) {
        updateRetro(&local);
    }

    if (style_ == ClockStyle::Analog) {
        const float angles[3] = {
            (local.tm_hour % 12 + local.tm_min / 60.0f) * 30.0f,
            (local.tm_min + local.tm_sec / 60.0f) * 6.0f,
            local.tm_sec * 6.0f,
        };
        const int lengths[3] = {58, 82, 92};
        for (uint8_t i = 0; i < 3; ++i) {
            const float radians = (angles[i] - 90.0f) * DEG_TO_RAD;
            handPoints_[i][0] = {240, 159};
            handPoints_[i][1] = {static_cast<lv_coord_t>(240 + cosf(radians) * lengths[i]),
                                 static_cast<lv_coord_t>(159 + sinf(radians) * lengths[i])};
            lv_line_set_points(circles_[i], handPoints_[i], 2);
        }
    } else if (style_ == ClockStyle::LinearHorizon) {
        const int values[3] = {local.tm_hour % 12, local.tm_min, local.tm_sec};
        const int maximum[3] = {12, 60, 60};
        for (uint8_t i = 0; i < 3; ++i) lv_obj_set_width(indicators_[i], max(3, 370 * values[i] / maximum[i]));
    } else if (style_ == ClockStyle::Bauhaus) {
        const int values[3] = {local.tm_hour % 12, local.tm_min, local.tm_sec};
        const int radii[3] = {62, 86, 106};
        const int maximum[3] = {12, 60, 60};
        const int sizes[3] = {200, 142, 82};
        for (uint8_t i = 0; i < 3; ++i) {
            const float angle = (360.0f * values[i] / maximum[i] - 90.0f) * DEG_TO_RAD;
            lv_obj_set_pos(circles_[i], 240 + cosf(angle) * radii[i] - sizes[i] / 2,
                           158 + sinf(angle) * radii[i] - sizes[i] / 2);
        }
    } else if (style_ == ClockStyle::Arc) {
        lv_arc_set_value(bars_[0], local.tm_hour % 12);
        lv_arc_set_value(bars_[1], local.tm_min);
        lv_arc_set_value(bars_[2], local.tm_sec);
    }
}

void ClockScreen::touchEvent(lv_event_t* event) {
    // Keep the wake layer active until release so the first gesture cannot reach the restored UI.
    if (lv_event_get_code(event) != LV_EVENT_RELEASED) return;
    state().lastTouchMs = millis();
}

}
