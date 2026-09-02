#include "BootScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../boot/BootExperience.h"
#include "UiTheme.h"

namespace coronet {

namespace {

constexpr const char* BootFeatures[] = {
    "INTELLIGENT LED STATUS BAR",
    "LIVE PRINTER TELEMETRY",
    "ADAPTIVE VENTILATION CONTROL",
    "PANDA BREATH INTEGRATION",
    "SMARTPHONE COMPANION APP",
    "WI-FI & BLUETOOTH CONTROL",
    "AUDIO STATUS & ALERTS",
    "OVER-THE-AIR UPDATES",
};

constexpr uint32_t FeatureStartMs = 5100U;
constexpr uint32_t FeatureSlotMs = 3125U;
constexpr uint32_t FeatureFadeInMs = 420U;
constexpr uint32_t FeatureFadeOutMs = 550U;
constexpr uint32_t LogoSpectrumStartMs = 4000U;
constexpr uint32_t LogoSpectrumBlendMs = 650U;
constexpr uint16_t LogoSpectrumBaseHue = 174U;
constexpr uint32_t QuickHandoffStartMs = 2800U;

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

uint8_t smoothPulse(uint8_t value) {
    const uint32_t x = value;
    return static_cast<uint8_t>((x * x * (765U - 2U * x) + 32512U) / 65025U);
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
    lv_obj_set_pos(glowLine_, 228, 113);
    lv_obj_set_size(glowLine_, 226, 1);
    lv_obj_set_style_bg_color(glowLine_, lv_color_hex(0x17333D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_, LV_OPA_TRANSP, LV_PART_MAIN);

    arc_ = lv_arc_create(root_);
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_remove_style(arc_, nullptr, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(arc_, 55, 42);
    lv_obj_set_size(arc_, 128, 128);
    lv_arc_set_bg_angles(arc_, 44, 46);
    lv_obj_set_style_arc_width(arc_, 13, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(arc_, 64, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(arc_, 64, LV_PART_MAIN);
    lv_obj_set_style_transform_zoom(arc_, 256, LV_PART_MAIN);

    horizon_ = lv_obj_create(root_);
    lv_obj_remove_style_all(horizon_);
    lv_obj_set_pos(horizon_, 119, 102);
    lv_obj_set_size(horizon_, 0, 8);
    lv_obj_set_style_radius(horizon_, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(horizon_, LV_OPA_COVER, LV_PART_MAIN);

    core_ = lv_obj_create(root_);
    lv_obj_remove_style_all(core_);
    lv_obj_set_pos(core_, 112, 99);
    lv_obj_set_size(core_, 14, 14);
    lv_obj_set_style_radius(core_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(core_, lv_color_hex(0xF4F8FA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(core_, LV_OPA_COVER, LV_PART_MAIN);

    endpoint_ = lv_obj_create(root_);
    lv_obj_remove_style_all(endpoint_);
    lv_obj_set_pos(endpoint_, 194, 99);
    lv_obj_set_size(endpoint_, 14, 14);
    lv_obj_set_style_radius(endpoint_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(endpoint_, lv_color_hex(0xF1B84B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(endpoint_, LV_OPA_TRANSP, LV_PART_MAIN);

    coroLabel_ = lv_label_create(root_);
    lv_label_set_text(coroLabel_, "coro");
    lv_obj_set_pos(coroLabel_, 226, 65);
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
    lv_obj_set_pos(descriptorLabel_, 228, 120);
    lv_obj_set_style_text_font(descriptorLabel_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(descriptorLabel_, lv_color_hex(0x8FAAB7), LV_PART_MAIN);

    editionLabel_ = lv_label_create(root_);
    lv_label_set_text(editionLabel_, "OS 2  |  OPEN SOURCE");
    lv_obj_set_pos(editionLabel_, 228, 148);
    lv_obj_set_style_text_font(editionLabel_, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(editionLabel_, lv_color_hex(0xF1B84B), LV_PART_MAIN);

    featureLabel_ = lv_label_create(root_);
    lv_label_set_text(featureLabel_, "");
    lv_obj_set_pos(featureLabel_, 20, 226);
    lv_obj_set_width(featureLabel_, 440);
    lv_obj_set_style_text_align(featureLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(featureLabel_, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(featureLabel_, lv_color_hex(0xF4F8FA), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(featureLabel_, 1, LV_PART_MAIN);
    lv_obj_set_style_text_opa(featureLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
    featureIndex_ = -1;

    authorLabel_ = lv_label_create(root_);
    lv_label_set_text_static(authorLabel_, "Created by Damian Borkowski");
    lv_obj_set_pos(authorLabel_, 218, 298);
    lv_obj_set_width(authorLabel_, 250);
    lv_obj_set_style_text_align(authorLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_font(authorLabel_, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(authorLabel_, lv_color_hex(0x78909C), LV_PART_MAIN);
    lv_obj_set_style_text_opa(authorLabel_, LV_OPA_TRANSP, LV_PART_MAIN);

    setLogoColor(0x27D3C2);
    setOpacity(20, 0, 0);
    lv_obj_set_style_bg_opa(core_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_scr_load_anim(root_, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

void BootScreen::update() {
    if (!root_) return;
    BootExperience& experience = bootExperience();
    if (!experience.active()) {
        if (experience.full()) updateFull(BootExperience::FullDurationMs);
        else updateQuick(BootExperience::QuickDurationMs);
        return;
    }
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
    setLogoColor(0x27D3C2);
    setOpacity(static_cast<uint8_t>(18U + pulse / 12U), 0, 0);
    lv_arc_set_bg_angles(arc_, 44, 46);
    lv_obj_set_width(horizon_, 0);
    lv_obj_set_style_bg_opa(core_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(endpoint_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_opa(featureLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_opa(authorLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
}

void BootScreen::updateFull(uint32_t elapsedMs) {
    const uint8_t coronaReveal = ramp(elapsedMs, 0U, 1750U);
    const uint8_t coreReveal = ramp(elapsedMs, 1850U, 220U);
    const uint8_t horizonReveal = ramp(elapsedMs, 2160U, 900U);
    const uint8_t endpointReveal = ramp(elapsedMs, 3060U, 200U);
    const uint8_t wordReveal = ramp(elapsedMs, 3260U, 580U);
    const uint8_t detailReveal = ramp(elapsedMs, 3920U, 700U);
    const uint8_t authorReveal = ramp(elapsedMs, 4650U, 350U);
    const uint8_t pulse = triangle(elapsedMs, 1680U);

    uint32_t color = 0x27D3C2;
    if (elapsedMs >= LogoSpectrumStartMs && elapsedMs < 30500U) {
        const uint16_t hue = static_cast<uint16_t>(
            (LogoSpectrumBaseHue + (elapsedMs - LogoSpectrumStartMs) / 13U) % 360U);
        const lv_color_t spectrum = lv_color_hsv_to_rgb(hue, 86, 96);
        const uint8_t spectrumBlend = ramp(elapsedMs, LogoSpectrumStartMs, LogoSpectrumBlendMs);
        color = blendRgb(0x27D3C2U, lv_color_to32(spectrum) & 0xFFFFFFU, spectrumBlend);
    } else if (elapsedMs >= 30500U) {
        const uint8_t settle = ramp(elapsedMs, 30500U, 2100U);
        const uint16_t finalSpectrumHue = static_cast<uint16_t>(
            (LogoSpectrumBaseHue + (30500U - LogoSpectrumStartMs) / 13U) % 360U);
        const uint32_t finalSpectrum = lv_color_to32(
            lv_color_hsv_to_rgb(finalSpectrumHue, 86, 96)) & 0xFFFFFFU;
        color = blendRgb(finalSpectrum, 0x27D3C2U, settle);
    }

    setLogoColor(color);
    const uint8_t logoOpacity = static_cast<uint8_t>(20U + coronaReveal * 235U / 255U);
    setOpacity(logoOpacity, wordReveal, detailReveal);
    const uint8_t breathReveal = ramp(elapsedMs, LogoSpectrumStartMs, LogoSpectrumBlendMs);
    const uint32_t breathElapsed = elapsedMs >= LogoSpectrumStartMs ? elapsedMs - LogoSpectrumStartMs : 0U;
    const uint8_t breath = smoothPulse(triangle(breathElapsed, 2100U));
    const lv_coord_t breathZoom = static_cast<lv_coord_t>(
        256U + static_cast<uint32_t>(breath) * breathReveal * 5U / 65025U);
    lv_obj_set_style_transform_zoom(arc_, breathZoom, LV_PART_MAIN);
    lv_arc_set_bg_angles(arc_, 44,
        static_cast<uint16_t>(46U + static_cast<uint32_t>(coronaReveal) * 270U / 255U));
    lv_obj_set_style_bg_opa(core_, coreReveal, LV_PART_MAIN);
    lv_obj_set_width(horizon_, static_cast<lv_coord_t>(horizonReveal * 78U / 255U));
    const uint8_t endpointOpacity = static_cast<uint8_t>(static_cast<uint16_t>(endpointReveal) *
        (150U + static_cast<uint16_t>(pulse) * 105U / 255U) / 255U);
    lv_obj_set_style_bg_opa(endpoint_, endpointOpacity, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_,
        static_cast<uint8_t>(detailReveal / 5U), LV_PART_MAIN);
    lv_obj_set_style_text_opa(authorLabel_, authorReveal, LV_PART_MAIN);

    // Keep the completed logo geometry fixed; later energy comes from light only.
    if (elapsedMs >= 22000U && elapsedMs < 30000U) {
        const uint8_t climax = triangle(elapsedMs - 22000U, 840U);
        lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(55U + climax / 3U), LV_PART_MAIN);
    }

    constexpr uint8_t FeatureCount = sizeof(BootFeatures) / sizeof(BootFeatures[0]);
    const uint32_t featureTimeline = elapsedMs >= FeatureStartMs ? elapsedMs - FeatureStartMs : 0U;
    const uint8_t feature = static_cast<uint8_t>(featureTimeline / FeatureSlotMs);
    if (elapsedMs >= FeatureStartMs && feature < FeatureCount) {
        if (featureIndex_ != static_cast<int8_t>(feature)) {
            featureIndex_ = static_cast<int8_t>(feature);
            lv_label_set_text_static(featureLabel_, BootFeatures[feature]);
        }
        const uint32_t local = featureTimeline % FeatureSlotMs;
        uint8_t opacity = 255U;
        if (local < FeatureFadeInMs) opacity = ramp(local, 0U, FeatureFadeInMs);
        else if (local > FeatureSlotMs - FeatureFadeOutMs) {
            opacity = static_cast<uint8_t>(255U - ramp(local, FeatureSlotMs - FeatureFadeOutMs, FeatureFadeOutMs));
        }
        lv_obj_set_style_text_opa(featureLabel_, opacity, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_opa(featureLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
    }

    if (elapsedMs >= 32600U) {
        const uint8_t handoff = ramp(elapsedMs, 32600U, 2400U);
        const uint8_t remaining = 255U - handoff;
        setOpacity(remaining, remaining, remaining);
        lv_obj_set_style_bg_opa(core_, remaining, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(endpoint_,
            static_cast<uint8_t>(static_cast<uint16_t>(endpointOpacity) * remaining / 255U), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(remaining / 5U), LV_PART_MAIN);
        lv_obj_set_style_text_opa(authorLabel_,
            static_cast<uint8_t>(static_cast<uint16_t>(authorReveal) * remaining / 255U), LV_PART_MAIN);
    }
}

void BootScreen::updateQuick(uint32_t elapsedMs) {
    const uint8_t reveal = ramp(elapsedMs, 0U, 500U);
    setLogoColor(0x27D3C2);
    setOpacity(reveal, reveal, reveal);
    lv_arc_set_bg_angles(arc_, 44, 316);
    lv_obj_set_style_bg_opa(core_, reveal, LV_PART_MAIN);
    lv_obj_set_width(horizon_, 78);
    lv_obj_set_style_bg_opa(endpoint_, reveal, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(reveal / 6U), LV_PART_MAIN);
    lv_obj_set_style_text_opa(featureLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_opa(authorLabel_, reveal, LV_PART_MAIN);

    if (elapsedMs >= QuickHandoffStartMs) {
        const uint8_t handoff = ramp(elapsedMs, QuickHandoffStartMs,
            BootExperience::QuickDurationMs - QuickHandoffStartMs);
        const uint8_t remaining = 255U - handoff;
        setOpacity(remaining, remaining, remaining);
        lv_obj_set_style_bg_opa(core_, remaining, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(endpoint_, remaining, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(glowLine_, static_cast<uint8_t>(remaining / 6U), LV_PART_MAIN);
        lv_obj_set_style_text_opa(authorLabel_, remaining, LV_PART_MAIN);
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
