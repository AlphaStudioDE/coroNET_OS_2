#include "BootScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../boot/BootExperience.h"
#include "UiTheme.h"

namespace coronet {

namespace {

uint8_t ramp(uint32_t elapsed, uint32_t start, uint32_t duration) {
    if (elapsed <= start) return 0;
    if (!duration || elapsed >= start + duration) return 255;
    return static_cast<uint8_t>((elapsed - start) * 255U / duration);
}

uint8_t triangle(uint32_t elapsed, uint32_t period) {
    if (!period) return 0;
    const uint32_t position = elapsed % period;
    const uint32_t half = period / 2U;
    return position < half
        ? static_cast<uint8_t>(position * 255U / half)
        : static_cast<uint8_t>((period - position) * 255U / half);
}

uint32_t blendRgb(uint32_t from, uint32_t to, uint8_t amount) {
    const uint16_t inverse = 255U - amount;
    const uint8_t r = static_cast<uint8_t>((((from >> 16U) & 0xFFU) * inverse +
                                             ((to >> 16U) & 0xFFU) * amount + 127U) / 255U);
    const uint8_t g = static_cast<uint8_t>((((from >> 8U) & 0xFFU) * inverse +
                                             ((to >> 8U) & 0xFFU) * amount + 127U) / 255U);
    const uint8_t b = static_cast<uint8_t>(((from & 0xFFU) * inverse +
                                             (to & 0xFFU) * amount + 127U) / 255U);
    return (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) | b;
}

}

void BootScreen::begin() {
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x071018), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);

    glowLine_ = lv_obj_create(root_);
    lv_obj_remove_style_all(glowLine_);
    lv_obj_set_pos(glowLine_, 228, 153);
    lv_obj_set_size(glowLine_, 226, 1);
    lv_obj_set_style_bg_color(glowLine_, lv_color_hex(0x17333D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_, LV_OPA_30, LV_PART_MAIN);

    arc_ = lv_arc_create(root_);
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_remove_style(arc_, nullptr, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(arc_, 55, 82);
    lv_obj_set_size(arc_, 128, 128);
    lv_arc_set_bg_angles(arc_, 44, 316);
    lv_obj_set_style_arc_width(arc_, 13, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_MAIN);

    horizon_ = lv_obj_create(root_);
    lv_obj_remove_style_all(horizon_);
    lv_obj_set_pos(horizon_, 119, 142);
    lv_obj_set_size(horizon_, 0, 8);
    lv_obj_set_style_radius(horizon_, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(horizon_, LV_OPA_COVER, LV_PART_MAIN);

    core_ = lv_obj_create(root_);
    lv_obj_remove_style_all(core_);
    lv_obj_set_pos(core_, 112, 139);
    lv_obj_set_size(core_, 14, 14);
    lv_obj_set_style_radius(core_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(core_, lv_color_hex(0xF4F8FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(core_, LV_OPA_COVER, LV_PART_MAIN);

    endpoint_ = lv_obj_create(root_);
    lv_obj_remove_style_all(endpoint_);
    lv_obj_set_pos(endpoint_, 194, 139);
    lv_obj_set_size(endpoint_, 14, 14);
    lv_obj_set_style_radius(endpoint_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(endpoint_, lv_color_hex(0xF1B84B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(endpoint_, LV_OPA_TRANSP, LV_PART_MAIN);

    coroLabel_ = lv_label_create(root_);
    lv_label_set_text(coroLabel_, "coro");
    lv_obj_set_pos(coroLabel_, 226, 105);
    lv_obj_set_style_text_font(coroLabel_, &lv_font_montserrat_38, LV_PART_MAIN);
    lv_obj_set_style_text_color(coroLabel_, lv_color_hex(0xF4F8FA), LV_PART_MAIN);

    netLabel_ = lv_label_create(root_);
    lv_label_set_text(netLabel_, "NET");
    lv_obj_set_style_text_font(netLabel_, &lv_font_montserrat_38, LV_PART_MAIN);
    lv_obj_set_style_text_color(netLabel_, lv_color_hex(0x27D3C2), LV_PART_MAIN);
    lv_obj_update_layout(coroLabel_);
    lv_obj_align_to(netLabel_, coroLabel_, LV_ALIGN_OUT_RIGHT_MID, -1, 0);

    descriptorLabel_ = lv_label_create(root_);
    lv_label_set_text(descriptorLabel_, "CONNECTED PRINT ENVIRONMENT");
    lv_obj_set_pos(descriptorLabel_, 228, 160);
    lv_obj_set_style_text_font(descriptorLabel_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(descriptorLabel_, lv_color_hex(0x8FAAB7), LV_PART_MAIN);

    editionLabel_ = lv_label_create(root_);
    lv_label_set_text(editionLabel_, "OS 2  |  OPEN SOURCE");
    lv_obj_set_pos(editionLabel_, 228, 188);
    lv_obj_set_style_text_font(editionLabel_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(editionLabel_, lv_color_hex(0xF1B84B), LV_PART_MAIN);

    setLogoColor(0x27D3C2);
    setOpacity(30, 0, 0);
    lv_scr_load_anim(root_, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

void BootScreen::update() {
    if (!root_) return;
    BootExperience& experience = bootExperience();
    if (!experience.performanceStarted()) {
        updatePrelude(experience.preludeMs());
    } else if (experience.full()) {
        updateFull(experience.timelineMs());
    } else {
        updateQuick(experience.timelineMs());
    }
}

void BootScreen::updatePrelude(uint32_t elapsedMs) {
    const uint8_t pulse = triangle(elapsedMs, 1800U);
    setLogoColor(0x1AAEA4);
    setOpacity(static_cast<uint8_t>(28U + pulse / 4U), 0, 0);
    lv_arc_set_bg_angles(arc_, 44, static_cast<uint16_t>(80U + pulse / 5U));
    lv_obj_set_width(horizon_, static_cast<lv_coord_t>(6U + pulse / 18U));
    lv_obj_set_style_bg_opa(endpoint_, LV_OPA_TRANSP, LV_PART_MAIN);
}

void BootScreen::updateFull(uint32_t elapsedMs) {
    const uint8_t coreReveal = ramp(elapsedMs, 0U, 900U);
    const uint8_t horizonReveal = ramp(elapsedMs, 700U, 3600U);
    const uint8_t coronaReveal = ramp(elapsedMs, 2200U, 5200U);
    const uint8_t wordReveal = ramp(elapsedMs, 4200U, 3000U);
    const uint8_t detailReveal = ramp(elapsedMs, 12500U, 3500U);
    const uint8_t pulse = triangle(elapsedMs, 1680U);

    uint32_t color = 0x27D3C2;
    if (elapsedMs >= 11800U && elapsedMs < 30500U) {
        const uint16_t hue = static_cast<uint16_t>(((elapsedMs - 11800U) / 13U) % 360U);
        const lv_color_t spectrum = lv_color_hsv_to_rgb(hue, 86, 96);
        color = lv_color_to32(spectrum);
    } else if (elapsedMs >= 30500U) {
        const uint8_t settle = ramp(elapsedMs, 30500U, 2100U);
        const uint16_t finalSpectrumHue = static_cast<uint16_t>(((30500U - 11800U) / 13U) % 360U);
        const uint32_t finalSpectrum = lv_color_to32(
            lv_color_hsv_to_rgb(finalSpectrumHue, 86, 96)) & 0xFFFFFFU;
        color = blendRgb(finalSpectrum, 0x27D3C2U, settle);
    }

    setLogoColor(color);
    setOpacity(static_cast<uint8_t>(70U + coronaReveal * 185U / 255U),
               wordReveal, detailReveal);
    lv_arc_set_bg_angles(arc_, 44,
        static_cast<uint16_t>(44U + static_cast<uint32_t>(coronaReveal) * 272U / 255U));
    lv_obj_set_style_bg_opa(core_, coreReveal, LV_PART_MAIN);
    lv_obj_set_width(horizon_, static_cast<lv_coord_t>(horizonReveal * 78U / 255U));
    lv_obj_set_style_bg_opa(endpoint_,
        elapsedMs > 4200U ? static_cast<uint8_t>(150U + pulse * 105U / 255U) : LV_OPA_TRANSP,
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_,
        static_cast<uint8_t>(20U + ramp(elapsedMs, 7000U, 11000U) / 5U), LV_PART_MAIN);

    // Once the horizon reaches the endpoint it remains mechanically locked.
    // The pre-climax contraction belongs to the light show, not the logo geometry.
    if (elapsedMs >= 22000U && elapsedMs < 30000U) {
        const uint8_t climax = triangle(elapsedMs - 22000U, 840U);
        lv_obj_set_style_arc_width(arc_, static_cast<lv_coord_t>(13U + climax / 64U), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(55U + climax / 3U), LV_PART_MAIN);
    } else {
        lv_obj_set_style_arc_width(arc_, 13, LV_PART_MAIN);
    }

    if (elapsedMs >= 32600U) {
        const uint8_t handoff = ramp(elapsedMs, 32600U, 2400U);
        const uint8_t remaining = 255U - handoff;
        setOpacity(static_cast<uint8_t>(80U + remaining * 175U / 255U),
                   static_cast<uint8_t>(80U + remaining * 175U / 255U),
                   static_cast<uint8_t>(remaining));
    }
}

void BootScreen::updateQuick(uint32_t elapsedMs) {
    const uint8_t coreReveal = ramp(elapsedMs, 0U, 260U);
    const uint8_t horizonReveal = ramp(elapsedMs, 170U, 650U);
    const uint8_t logoReveal = ramp(elapsedMs, 420U, 700U);
    const uint8_t wordReveal = ramp(elapsedMs, 720U, 580U);
    setLogoColor(0x27D3C2);
    setOpacity(static_cast<uint8_t>(45U + logoReveal * 210U / 255U), wordReveal, 0);
    lv_arc_set_bg_angles(arc_, 44,
        static_cast<uint16_t>(44U + static_cast<uint32_t>(logoReveal) * 272U / 255U));
    lv_obj_set_style_bg_opa(core_, coreReveal, LV_PART_MAIN);
    lv_obj_set_width(horizon_, static_cast<lv_coord_t>(horizonReveal * 78U / 255U));
    lv_obj_set_style_bg_opa(endpoint_, elapsedMs > 760U ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(20U + logoReveal / 6U), LV_PART_MAIN);

    if (elapsedMs >= 1950U) {
        const uint8_t handoff = ramp(elapsedMs, 1950U, 650U);
        const uint8_t remaining = 255U - handoff;
        setOpacity(static_cast<uint8_t>(70U + remaining * 185U / 255U),
                   static_cast<uint8_t>(70U + remaining * 185U / 255U), 0);
    }
}

void BootScreen::setLogoColor(uint32_t rgb) {
    const lv_color_t color = lv_color_hex(rgb & 0xFFFFFFU);
    lv_obj_set_style_arc_color(arc_, color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(horizon_, color, LV_PART_MAIN);
    lv_obj_set_style_text_color(netLabel_, color, LV_PART_MAIN);
}

void BootScreen::setOpacity(uint8_t logo, uint8_t wordmark, uint8_t detail) {
    lv_obj_set_style_arc_opa(arc_, logo, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(horizon_, logo, LV_PART_MAIN);
    lv_obj_set_style_text_opa(coroLabel_, wordmark, LV_PART_MAIN);
    lv_obj_set_style_text_opa(netLabel_, wordmark, LV_PART_MAIN);
    lv_obj_set_style_text_opa(descriptorLabel_, detail, LV_PART_MAIN);
    lv_obj_set_style_text_opa(editionLabel_, detail, LV_PART_MAIN);
}

}
