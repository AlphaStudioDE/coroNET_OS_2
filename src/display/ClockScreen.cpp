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

}

void ClockScreen::begin(ClockStyle style) {
    style_ = style;
    lastSecond_ = UINT32_MAX;
    timeLabel_ = nullptr;
    secondsLabel_ = nullptr;
    dateLabel_ = nullptr;
    memset(bars_, 0, sizeof(bars_));
    memset(indicators_, 0, sizeof(indicators_));
    memset(circles_, 0, sizeof(circles_));
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(root_, touchEvent, LV_EVENT_PRESSED, nullptr);
    label(root_, "coroNET", ui::ColorMuted, &lv_font_montserrat_12, 18, 15);

    switch (style_) {
        case ClockStyle::Retro: buildDigital(true); break;
        case ClockStyle::Analog: buildAnalog(); break;
        case ClockStyle::LinearHorizon: buildLinear(); break;
        case ClockStyle::Bauhaus: buildBauhaus(); break;
        case ClockStyle::DotMatrix: buildMatrix(); break;
        case ClockStyle::Arc: buildArc(); break;
        case ClockStyle::Digital:
        default: buildDigital(false); break;
    }
    lv_scr_load_anim(root_, LV_SCR_LOAD_ANIM_FADE_ON, 120, 0, true);
    state().screenSaverActive = true;
    update();
}

void ClockScreen::buildDigital(bool retro) {
    const uint32_t color = retro ? ui::ColorAmber : ui::ColorText;
    timeLabel_ = label(root_, "--:--", color, &lv_font_montserrat_48, 42, 91, 396);
    lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    secondsLabel_ = label(root_, "--", retro ? ui::ColorRed : ui::ColorCyan,
                          &lv_font_montserrat_22, 392, 135, 48);
    dateLabel_ = label(root_, "Waiting for time", ui::ColorMuted,
                       &lv_font_montserrat_14, 42, 170, 396);
    lv_obj_set_style_text_align(dateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    if (retro) {
        lv_obj_set_style_bg_color(root_, lv_color_hex(0x120B05), LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(timeLabel_, 5, LV_PART_MAIN);
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
    char timeText[16] = "";
    snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, local.tm_min);
    char dateText[32] = "";
    strftime(dateText, sizeof(dateText), "%a, %d %b %Y", &local);
    if (timeLabel_) lv_label_set_text(timeLabel_, timeText);
    if (secondsLabel_) lv_label_set_text_fmt(secondsLabel_, "%02d", local.tm_sec);
    if (dateLabel_) lv_label_set_text(dateLabel_, dateText);

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
    if (lv_event_get_code(event) != LV_EVENT_PRESSED) return;
    state().touchCount++;
    state().lastTouchMs = millis();
}

}
