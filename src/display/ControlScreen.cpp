#include "ControlScreen.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "../audio/AudioService.h"
#include "../config/HardwareConfig.h"
#include "../core/SystemState.h"
#include "../led/LedBrightnessCurve.h"
#include "../led/LedService.h"
#include "../panda/PandaBreathService.h"
#include "../settings/SettingsService.h"
#include "../vent/VentService.h"
#include "UiTheme.h"

namespace coronet {

namespace {

constexpr int kCanvasWidth = 420;
constexpr int kCanvasHeight = 52;

const char* kCategoryNames[] = {"IDLE", "PRINT", "PAUSE", "ERROR", "FINISH", "OTHER"};
const char* kSectionNames[] = {"RIGHT", "CENTER", "LEFT", "INSIDE"};
const char* kScenarioNames[] = {"START", "FINISH", "ERROR", "PAUSE", "IDLE"};
const char* kPandaModeNames[] = {"OFF", "AUTO", "PREHEAT", "TEMPER", "FORCED", "DRYING"};

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, uint32_t color,
                    const lv_font_t* font, int x, int y, int width = LV_SIZE_CONTENT) {
    lv_obj_t* label = lv_label_create(parent);
    styleText(label, color, font);
    if (width != LV_SIZE_CONTENT) lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

void styleButton(lv_obj_t* button, bool accent = false) {
    lv_obj_set_style_radius(button, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(accent ? ui::ColorCyanDark : ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(accent ? ui::ColorCyan : ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
}

void markTouch() {
    state().touchCount++;
    state().lastTouchMs = millis();
}

void rootTouch(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) markTouch();
}

uint8_t previewDegamma(uint8_t value) {
    return ledcurve::decode(value);
}

uint32_t rgbwHex(const RgbwColor& color, bool linearWhite = false) {
    // Physical output applies gamma to RGB luminance while preserving channel
    // ratios. Undoing gamma per channel would lift weaker channels too much and
    // make saturated colours look pastel in the LCD preview.
    const uint8_t peak = max(color.r, max(color.g, color.b));
    const uint8_t linearPeak = previewDegamma(peak);
    const uint8_t rgbR = peak ? static_cast<uint8_t>(
        (static_cast<uint16_t>(color.r) * linearPeak + peak / 2U) / peak) : 0U;
    const uint8_t rgbG = peak ? static_cast<uint8_t>(
        (static_cast<uint16_t>(color.g) * linearPeak + peak / 2U) / peak) : 0U;
    const uint8_t rgbB = peak ? static_cast<uint8_t>(
        (static_cast<uint16_t>(color.b) * linearPeak + peak / 2U) / peak) : 0U;
    const uint8_t white = linearWhite ? color.w : previewDegamma(color.w);
    const uint8_t r = static_cast<uint8_t>(min<uint16_t>(255U, rgbR + white));
    const uint8_t g = static_cast<uint8_t>(min<uint16_t>(255U, rgbG + white));
    const uint8_t b = static_cast<uint8_t>(min<uint16_t>(255U, rgbB + white));
    return (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) | b;
}

}

void ControlScreen::begin(ui::Page page, ui::Navigation::Callback navigationCallback,
                          void* callbackContext, bool animate) {
    page_ = page;
    bindingCount_ = 0;
    settingsRevisionSeen_ = 0;
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(root_, rootTouch, LV_EVENT_PRESSED, nullptr);
    buildHeader();
    if (page == ui::Page::Led) buildLedPage();
    else if (page == ui::Page::Vent) buildVentPage();
    else buildSoundPage();
    navigation_.build(root_, page, navigationCallback, callbackContext);
    lv_scr_load_anim(root_, animate ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_NONE,
                     animate ? 180 : 0, 0, true);
    update();
}

void ControlScreen::buildHeader() {
    makeLabel(root_, "coroNET", ui::ColorText, &lv_font_montserrat_22, 18, 11);
    const char* pageName = page_ == ui::Page::Led ? "LED" : page_ == ui::Page::Vent ? "VENT" : "SOUND";
    makeLabel(root_, pageName, ui::ColorCyan, &lv_font_montserrat_10, 127, 20);
    wifiLabel_ = makeLabel(root_, LV_SYMBOL_WIFI, ui::ColorMuted, &lv_font_montserrat_16, 425, 14, 30);
    lv_obj_set_style_text_align(wifiLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t* divider = lv_obj_create(root_);
    lv_obj_set_size(divider, 444, 1);
    lv_obj_set_pos(divider, 18, 49);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(divider, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
}

lv_obj_t* ControlScreen::makeContent() {
    lv_obj_t* content = lv_obj_create(root_);
    lv_obj_set_size(content, 464, 196);
    lv_obj_set_pos(content, 8, 54);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 4, LV_PART_MAIN);
    return content;
}

lv_obj_t* ControlScreen::makeCard(lv_obj_t* parent, int y, int height, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, height);
    lv_obj_set_pos(card, 0, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(ui::ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
    makeLabel(card, title, ui::ColorCyan, &lv_font_montserrat_10, 14, 11);
    return card;
}

lv_obj_t* ControlScreen::makeButton(lv_obj_t* parent, int x, int y, int width, int height,
                                    const char* text, Action action) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    styleButton(button);
    lv_obj_t* label = lv_label_create(button);
    styleText(label, ui::ColorText, &lv_font_montserrat_12);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    if (bindingCount_ < 48) {
        bindings_[bindingCount_] = {this, action};
        lv_obj_add_event_cb(button, eventHandler, LV_EVENT_CLICKED, &bindings_[bindingCount_++]);
    }
    return label;
}

lv_obj_t* ControlScreen::makeSlider(lv_obj_t* parent, int x, int y, int width,
                                    int minimum, int maximum, int value, Action action) {
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, width, 18);
    lv_obj_set_pos(slider, x, y);
    lv_slider_set_range(slider, minimum, maximum);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorCyan), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorText), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    if (bindingCount_ < 48) {
        bindings_[bindingCount_] = {this, action};
        lv_obj_add_event_cb(slider, eventHandler, LV_EVENT_VALUE_CHANGED, &bindings_[bindingCount_]);
        lv_obj_add_event_cb(slider, eventHandler, LV_EVENT_RELEASED, &bindings_[bindingCount_]);
        lv_obj_add_event_cb(slider, eventHandler, LV_EVENT_PRESS_LOST, &bindings_[bindingCount_++]);
    }
    return slider;
}

void ControlScreen::buildLedPage() {
    lv_obj_t* content = makeContent();
    lv_obj_t* selection = makeCard(content, 0, 154, "ANIMATION");
    makeButton(selection, 14, 31, 42, 34, LV_SYMBOL_LEFT, Action::CategoryPrev);
    categoryLabel_ = makeLabel(selection, "IDLE", ui::ColorText, &lv_font_montserrat_14, 64, 40, 316);
    lv_obj_set_style_text_align(categoryLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    makeButton(selection, 392, 31, 42, 34, LV_SYMBOL_RIGHT, Action::CategoryNext);
    makeButton(selection, 14, 70, 42, 34, LV_SYMBOL_LEFT, Action::AnimationPrev);
    animationLabel_ = makeLabel(selection, "Slow Orbit", ui::ColorText, &lv_font_montserrat_14, 64, 79, 264);
    lv_obj_set_style_text_align(animationLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    makeButton(selection, 336, 70, 42, 34, LV_SYMBOL_RIGHT, Action::AnimationNext);
    makeButton(selection, 386, 70, 48, 34, LV_SYMBOL_PLAY, Action::Preview);

    if (!previewBuffer_) previewBuffer_ = heap_caps_calloc(
        kCanvasWidth * kCanvasHeight, sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (previewBuffer_) {
        previewCanvas_ = lv_canvas_create(selection);
        lv_canvas_set_buffer(previewCanvas_, previewBuffer_, kCanvasWidth, kCanvasHeight, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(previewCanvas_, 14, 105);
        lv_obj_set_size(previewCanvas_, kCanvasWidth, 40);
    }

    lv_obj_t* control = makeCard(content, 162, 250, "LED CONTROL");
    insideButtonLabel_ = makeButton(control, 14, 33, 126, 36, "INSIDE: WHITE", Action::InsideStyle);
    mirrorButtonLabel_ = makeButton(control, 150, 33, 126, 36, "MIRROR: OFF", Action::Mirror);
    sectionButtonLabel_ = makeButton(control, 286, 33, 148, 36, "SECTION: RIGHT", Action::SectionNext);
    brightnessLabel_ = makeLabel(control, "BRIGHTNESS 70%", ui::ColorMuted, &lv_font_montserrat_10, 14, 82, 130);
    brightnessSlider_ = makeSlider(control, 150, 80, 284, 0, 100, 70, Action::Brightness);
    dimmButtonLabel_ = makeButton(control, 14, 116, 126, 36, "DIMM: OFF", Action::DimmToggle);
    dimmLabel_ = makeLabel(control, "AFTER 5 MIN 20%", ui::ColorMuted, &lv_font_montserrat_10, 150, 124, 130);
    dimmSlider_ = makeSlider(control, 286, 120, 148, 0, 100, 20, Action::DimmPercent);
    makeButton(control, 14, 164, 126, 36, "DEFAULT", Action::RemixDefault);
    remixLabel_ = makeLabel(control, "COLOR REMIX 0", ui::ColorMuted, &lv_font_montserrat_10, 150, 172, 130);
    remixSlider_ = makeSlider(control, 286, 168, 148, 0, 359, 0, Action::Remix);
    makeLabel(control, "Remix changes decorative hues only; data colors keep their meaning.",
              ui::ColorMuted, &lv_font_montserrat_10, 14, 216, 420);
    makeLabel(content, "", ui::ColorMuted, &lv_font_montserrat_10, 0, 420);
}

void ControlScreen::buildVentPage() {
    lv_obj_t* content = makeContent();
    lv_obj_t* local = makeCard(content, 0, 224, "LOCAL VENTILATION");
    ventModeLabels_[0] = makeButton(local, 14, 32, 126, 36, "AUTO", Action::VentAuto);
    ventModeLabels_[1] = makeButton(local, 151, 32, 126, 36, "TARGET", Action::VentTarget);
    ventModeLabels_[2] = makeButton(local, 288, 32, 146, 36, "MANUAL", Action::VentManual);
    ventStatusLabel_ = makeLabel(local, "", ui::ColorMuted, &lv_font_montserrat_10, 14, 77, 420);
    ventTargetLabel_ = makeLabel(local, "TARGET 40 C", ui::ColorMuted, &lv_font_montserrat_10, 14, 103, 120);
    ventTargetSlider_ = makeSlider(local, 150, 101, 284, 20, 80, 40, Action::VentTargetTemp);
    fanLabel_ = makeLabel(local, "FAN 0%", ui::ColorMuted, &lv_font_montserrat_10, 14, 139, 120);
    fanSlider_ = makeSlider(local, 150, 137, 284, 0, 100, 0, Action::ManualFan);
    flapLabel_ = makeLabel(local, "FLAP 0%", ui::ColorMuted, &lv_font_montserrat_10, 14, 175, 120);
    flapSlider_ = makeSlider(local, 150, 173, 284, 0, 100, 0, Action::ManualFlap);

    lv_obj_t* calibration = makeCard(content, 232, 190, "HARDWARE CALIBRATION");
    servoClosedLabel_ = makeLabel(calibration, "CLOSED 1000 us", ui::ColorMuted, &lv_font_montserrat_10, 14, 38, 150);
    servoClosedSlider_ = makeSlider(calibration, 170, 34, 264, 500, 2500, 1000, Action::ServoClosed);
    servoOpenLabel_ = makeLabel(calibration, "OPEN 2000 us", ui::ColorMuted, &lv_font_montserrat_10, 14, 74, 150);
    servoOpenSlider_ = makeSlider(calibration, 170, 70, 264, 500, 2500, 2000, Action::ServoOpen);
    fanMinLabel_ = makeLabel(calibration, "FAN MIN 30%", ui::ColorMuted, &lv_font_montserrat_10, 14, 110, 150);
    fanMinSlider_ = makeSlider(calibration, 170, 106, 264, 0, 100, 30, Action::FanMinimum);
    fanMaxLabel_ = makeLabel(calibration, "FAN MAX 100%", ui::ColorMuted, &lv_font_montserrat_10, 14, 146, 150);
    fanMaxSlider_ = makeSlider(calibration, 170, 142, 264, 0, 100, 100, Action::FanMaximum);
    servoReverseLabel_ = makeButton(calibration, 14, 160, 146, 26, "SERVO: NORMAL", Action::ServoReverse);

    lv_obj_t* panda = makeCard(content, 430, 226, "PANDA BREATH");
    pandaEnabledLabel_ = makeButton(panda, 14, 33, 146, 36, "PANDA: OFF", Action::PandaEnabled);
    pandaModeLabel_ = makeButton(panda, 170, 33, 264, 36, "MODE: OFF", Action::PandaMode);
    pandaStatusLabel_ = makeLabel(panda, "Panda disabled", ui::ColorMuted,
                                  &lv_font_montserrat_10, 14, 80, 420);
    pandaTargetLabel_ = makeLabel(panda, "TARGET 40 C", ui::ColorMuted, &lv_font_montserrat_10, 14, 113, 128);
    pandaTargetSlider_ = makeSlider(panda, 150, 109, 284, 30, 60, 40, Action::PandaTarget);
    pandaPresetLabel_ = makeButton(panda, 14, 146, 188, 34, "DRY: PLA", Action::PandaPreset);
    pandaHoursLabel_ = makeLabel(panda, "12 H", ui::ColorMuted, &lv_font_montserrat_10, 214, 157, 55);
    pandaHoursSlider_ = makeSlider(panda, 272, 153, 162, 1, 24, 12, Action::PandaHours);
    makeLabel(panda, settingsService().settings().pandaHost[0] ? settingsService().settings().pandaHost : "Panda address not configured",
              ui::ColorMuted, &lv_font_montserrat_10, 14, 196, 420);
    makeLabel(content, "", ui::ColorMuted, &lv_font_montserrat_10, 0, 664);
}

void ControlScreen::buildSoundPage() {
    lv_obj_t* content = makeContent();
    lv_obj_t* sound = makeCard(content, 0, 238, "SOUND SCENARIOS");
    makeButton(sound, 14, 32, 42, 36, LV_SYMBOL_LEFT, Action::SoundPrev);
    soundScenarioLabel_ = makeLabel(sound, "START", ui::ColorText, &lv_font_montserrat_16, 64, 42, 316);
    lv_obj_set_style_text_align(soundScenarioLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    makeButton(sound, 392, 32, 42, 36, LV_SYMBOL_RIGHT, Action::SoundNext);
    makeButton(sound, 14, 76, 42, 30, LV_SYMBOL_LEFT, Action::SoundFilePrev);
    soundPathLabel_ = makeButton(sound, 64, 76, 316, 30, "DEFAULT FILE", Action::SoundFileNext);
    makeButton(sound, 392, 76, 42, 30, LV_SYMBOL_RIGHT, Action::SoundFileNext);
    soundVolumeLabel_ = makeLabel(sound, "VOLUME 75%", ui::ColorMuted,
                                  &lv_font_montserrat_10, 14, 120, 120);
    soundVolumeSlider_ = makeSlider(sound, 150, 118, 284, 0, 100, 75, Action::SoundVolume);
    soundRepeatLabel_ = makeButton(sound, 14, 156, 126, 36, "REPEAT: OFF", Action::SoundRepeat);
    makeButton(sound, 150, 156, 136, 36, LV_SYMBOL_PLAY " TEST", Action::SoundPlay);
    makeButton(sound, 296, 156, 138, 36, LV_SYMBOL_STOP " STOP", Action::SoundStop);
    soundRuntimeLabel_ = makeLabel(sound, "", ui::ColorMuted, &lv_font_montserrat_10, 14, 207, 420);

    lv_obj_t* storage = makeCard(content, 246, 126, "AUDIO STORAGE");
    makeLabel(storage, "SD CARD", ui::ColorMuted, &lv_font_montserrat_10, 14, 38);
    soundStorageLabel_ = makeLabel(storage, "", state().sdReady ? ui::ColorGreen : ui::ColorRed,
                                   &lv_font_montserrat_12, 100, 35, 330);
    makeLabel(storage, "PCM WAV: mono/stereo, 8/16-bit, 8-48 kHz. Files are streamed from SD through PSRAM.",
              ui::ColorMuted, &lv_font_montserrat_10, 14, 69, 420);
    makeLabel(content, "", ui::ColorMuted, &lv_font_montserrat_10, 0, 378);
}

void ControlScreen::update() {
    if (!root_) return;
    lv_obj_set_style_text_color(wifiLabel_, lv_color_hex(state().wifiConnected ? ui::ColorCyan : ui::ColorMuted), LV_PART_MAIN);
    if (page_ == ui::Page::Led) refreshLed();
    else if (page_ == ui::Page::Vent) refreshVent();
    else refreshSound();
    settingsRevisionSeen_ = settingsService().revision();
}

void ControlScreen::refreshLed() {
    const AppSettings& settings = settingsService().settings();
    const LedCategory category = static_cast<LedCategory>(selectedCategory_);
    const uint8_t animation = normalizeLedAnimation(category, settings.ledAnimation[selectedCategory_]);
    lv_label_set_text(categoryLabel_, kCategoryNames[selectedCategory_]);
    lv_label_set_text(animationLabel_, ledAnimationName(category, animation));
    lv_label_set_text(insideButtonLabel_, settings.insideColorStyle == InsideColorStyle::White ? "INSIDE: WHITE" : "INSIDE: AMBIENT");
    lv_label_set_text(mirrorButtonLabel_, settings.mirrorLedLayout ? "MIRROR: ON" : "MIRROR: OFF");
    lv_label_set_text_fmt(sectionButtonLabel_, "SECTION: %s", kSectionNames[selectedSection_]);
    lv_label_set_text_fmt(brightnessLabel_, "BRIGHTNESS %u%%", settings.ledBrightness[selectedSection_]);
    if (!lv_obj_has_state(brightnessSlider_, LV_STATE_PRESSED)) lv_slider_set_value(brightnessSlider_, settings.ledBrightness[selectedSection_], LV_ANIM_OFF);
    lv_label_set_text(dimmButtonLabel_, settings.ledDimmEnabled[selectedSection_] ? "DIMM: ON" : "DIMM: OFF");
    lv_label_set_text_fmt(dimmLabel_, "AFTER 5 MIN %u%%", settings.ledDimmPercent[selectedSection_]);
    if (!lv_obj_has_state(dimmSlider_, LV_STATE_PRESSED)) lv_slider_set_value(dimmSlider_, settings.ledDimmPercent[selectedSection_], LV_ANIM_OFF);
    lv_label_set_text_fmt(remixLabel_, "COLOR REMIX %d", settings.ledColorRemixDegrees[selectedCategory_]);
    if (!lv_obj_has_state(remixSlider_, LV_STATE_PRESSED)) lv_slider_set_value(remixSlider_, settings.ledColorRemixDegrees[selectedCategory_], LV_ANIM_OFF);
    refreshLedCanvas();
}

void ControlScreen::refreshLedCanvas() {
    if (!previewBuffer_ || !previewCanvas_ || millis() - lastCanvasUpdateMs_ < 100U) return;
    lastCanvasUpdateMs_ = millis();
    lv_color_t* pixels = static_cast<lv_color_t*>(previewBuffer_);
    for (int i = 0; i < kCanvasWidth * kCanvasHeight; ++i) pixels[i] = lv_color_hex(ui::ColorBackground);
    RgbwColor frame[hw::LedCount] = {};
    if (!ledService().copyFrame(frame, hw::LedCount)) return;
    auto draw = [&](int x, int y, int width, int height, uint32_t color) {
        const lv_color_t pixel = lv_color_hex(color);
        for (int py = y; py < y + height && py < kCanvasHeight; ++py)
            for (int px = x; px < x + width && px < kCanvasWidth; ++px)
                if (px >= 0 && py >= 0) pixels[py * kCanvasWidth + px] = pixel;
    };
    // Physical outer indices run from the device's right edge to its left edge.
    // Draw them in visual left-to-right order so motion matches the real strip.
    for (uint16_t i = 0; i < hw::OuterCount; ++i) {
        draw(2 + i * 10, 4, 8, 8, rgbwHex(frame[hw::OuterEnd - i]));
    }
    const bool linearInsideWhite =
        settingsService().settings().insideColorStyle == InsideColorStyle::White;
    for (uint16_t i = 0; i < hw::InsideCount; ++i) {
        draw(2 + i * 23, 25, 18, 8,
             rgbwHex(frame[hw::InsideStart + i], linearInsideWhite));
    }
    lv_obj_invalidate(previewCanvas_);
}

void ControlScreen::refreshVent() {
    const AppSettings& settings = settingsService().settings();
    lv_label_set_text_fmt(ventStatusLabel_, "%s  |  output %u%% / flap %u%%",
                          state().ventStatusText, state().fanPercent, state().flapPercent);
    for (uint8_t i = 0; i < 3; ++i) {
        lv_obj_t* button = lv_obj_get_parent(ventModeLabels_[i]);
        const bool active = static_cast<uint8_t>(settings.ventMode) == i;
        lv_obj_set_style_border_color(button, lv_color_hex(active ? ui::ColorCyan : ui::ColorBorder), LV_PART_MAIN);
    }
    lv_label_set_text_fmt(ventTargetLabel_, "TARGET %u C", settings.ventTargetTempC);
    lv_label_set_text_fmt(fanLabel_, "FAN %u%%", settings.manualFanPercent);
    lv_label_set_text_fmt(flapLabel_, "FLAP %u%%", settings.manualFlapPercent);
    if (!lv_obj_has_state(ventTargetSlider_, LV_STATE_PRESSED)) lv_slider_set_value(ventTargetSlider_, settings.ventTargetTempC, LV_ANIM_OFF);
    if (!lv_obj_has_state(fanSlider_, LV_STATE_PRESSED)) lv_slider_set_value(fanSlider_, settings.manualFanPercent, LV_ANIM_OFF);
    if (!lv_obj_has_state(flapSlider_, LV_STATE_PRESSED)) lv_slider_set_value(flapSlider_, settings.manualFlapPercent, LV_ANIM_OFF);
    lv_label_set_text_fmt(servoClosedLabel_, "CLOSED %u us", settings.servoClosedUs);
    lv_label_set_text_fmt(servoOpenLabel_, "OPEN %u us", settings.servoOpenUs);
    lv_label_set_text(servoReverseLabel_, settings.servoReverse ? "SERVO: REVERSE" : "SERVO: NORMAL");
    lv_label_set_text_fmt(fanMinLabel_, "FAN MIN %u%%", settings.fanMinPercent);
    lv_label_set_text_fmt(fanMaxLabel_, "FAN MAX %u%%", settings.fanMaxPercent);
    if (!lv_obj_has_state(servoClosedSlider_, LV_STATE_PRESSED)) lv_slider_set_value(servoClosedSlider_, settings.servoClosedUs, LV_ANIM_OFF);
    if (!lv_obj_has_state(servoOpenSlider_, LV_STATE_PRESSED)) lv_slider_set_value(servoOpenSlider_, settings.servoOpenUs, LV_ANIM_OFF);
    if (!lv_obj_has_state(fanMinSlider_, LV_STATE_PRESSED)) lv_slider_set_value(fanMinSlider_, settings.fanMinPercent, LV_ANIM_OFF);
    if (!lv_obj_has_state(fanMaxSlider_, LV_STATE_PRESSED)) lv_slider_set_value(fanMaxSlider_, settings.fanMaxPercent, LV_ANIM_OFF);
    lv_label_set_text(pandaEnabledLabel_, settings.pandaEnabled ? "PANDA: ON" : "PANDA: OFF");
    lv_label_set_text_fmt(pandaModeLabel_, "MODE: %s", kPandaModeNames[static_cast<uint8_t>(settings.pandaMode)]);
    static const char* const presets[] = {"PLA", "PETG", "ABS/ASA", "TPU", "NYLON/PA", "PC", "CUSTOM"};
    lv_label_set_text_fmt(pandaTargetLabel_, "TARGET %u C", settings.pandaTargetTempC);
    lv_label_set_text_fmt(pandaPresetLabel_, "DRY: %s", presets[static_cast<uint8_t>(settings.pandaDryPreset)]);
    lv_label_set_text_fmt(pandaHoursLabel_, "%u H", settings.pandaDryHours);
    if (!lv_obj_has_state(pandaTargetSlider_, LV_STATE_PRESSED)) lv_slider_set_value(pandaTargetSlider_, settings.pandaTargetTempC, LV_ANIM_OFF);
    if (!lv_obj_has_state(pandaHoursSlider_, LV_STATE_PRESSED)) lv_slider_set_value(pandaHoursSlider_, settings.pandaDryHours, LV_ANIM_OFF);
    char pandaTemperature[16] = "--.-";
    if (!isnan(state().pandaCurrentTempC)) {
        const int16_t tenths = static_cast<int16_t>(lroundf(state().pandaCurrentTempC * 10.0f));
        snprintf(pandaTemperature, sizeof(pandaTemperature), "%d.%d",
                 static_cast<int>(tenths / 10), static_cast<int>(abs(tenths % 10)));
    }
    lv_label_set_text_fmt(pandaStatusLabel_, "%s  |  %s C  |  %s",
                          state().pandaStatusText, pandaTemperature,
                          state().pandaConnected ? "CONNECTED" : "OFFLINE");
}

void ControlScreen::refreshSound() {
    const AppSettings& settings = settingsService().settings();
    lv_label_set_text(soundScenarioLabel_, kScenarioNames[selectedSound_]);
    const char* custom = settings.soundPath[selectedSound_];
    if (custom[0]) lv_label_set_text(soundPathLabel_, custom);
    else lv_label_set_text(soundPathLabel_, "DEFAULT FILE");
    lv_label_set_text_fmt(soundVolumeLabel_, "VOLUME %u%%", settings.soundVolume[selectedSound_]);
    if (!lv_obj_has_state(soundVolumeSlider_, LV_STATE_PRESSED)) lv_slider_set_value(soundVolumeSlider_, settings.soundVolume[selectedSound_], LV_ANIM_OFF);
    lv_label_set_text(soundRepeatLabel_, settings.soundRepeat[selectedSound_] ? "REPEAT: ON" : "REPEAT: OFF");
    lv_label_set_text_fmt(soundRuntimeLabel_, "%s%s",
                          state().audioPlaying ? "PLAYING  " : "READY  ",
                          state().activeSoundPath[0] ? state().activeSoundPath : "No active file");
    lv_label_set_text_fmt(soundStorageLabel_, "%s  |  %u WAV  |  %s",
                          state().sdReady ? "READY" : "UNAVAILABLE",
                          static_cast<unsigned>(state().audioFileCount), state().audioAssetStatus);
    lv_obj_set_style_text_color(soundStorageLabel_,
                                lv_color_hex(state().audioAssetsValid ? ui::ColorGreen : ui::ColorAmber),
                                LV_PART_MAIN);
}

void ControlScreen::handleAction(Action action, lv_event_t* event) {
    AppSettings& settings = settingsService().mutableSettings();
    const lv_event_code_t code = lv_event_get_code(event);
    const bool commit = code == LV_EVENT_CLICKED || code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST;
    switch (action) {
        case Action::CategoryPrev: selectedCategory_ = (selectedCategory_ + 5U) % 6U; break;
        case Action::CategoryNext: selectedCategory_ = (selectedCategory_ + 1U) % 6U; break;
        case Action::AnimationPrev: {
            const LedCategory category = static_cast<LedCategory>(selectedCategory_);
            const uint8_t count = ledAnimationCount(category);
            const uint8_t current = normalizeLedAnimation(category, settings.ledAnimation[selectedCategory_]);
            settings.ledAnimation[selectedCategory_] = static_cast<uint8_t>((current + count - 1U) % count);
            settingsService().save();
            ledService().requestPreview(category, settings.ledAnimation[selectedCategory_]);
            break;
        }
        case Action::AnimationNext: {
            const LedCategory category = static_cast<LedCategory>(selectedCategory_);
            const uint8_t count = ledAnimationCount(category);
            const uint8_t current = normalizeLedAnimation(category, settings.ledAnimation[selectedCategory_]);
            settings.ledAnimation[selectedCategory_] = static_cast<uint8_t>((current + 1U) % count);
            settingsService().save();
            ledService().requestPreview(category, settings.ledAnimation[selectedCategory_]);
            break;
        }
        case Action::Preview: ledService().requestPreview(static_cast<LedCategory>(selectedCategory_), settings.ledAnimation[selectedCategory_]); break;
        case Action::InsideStyle: settings.insideColorStyle = settings.insideColorStyle == InsideColorStyle::White ? InsideColorStyle::Ambient : InsideColorStyle::White; settingsService().save(); break;
        case Action::Mirror: settings.mirrorLedLayout = !settings.mirrorLedLayout; settingsService().save(); break;
        case Action::SectionNext: selectedSection_ = (selectedSection_ + 1U) % 4U; break;
        case Action::Brightness: settings.ledBrightness[selectedSection_] = lv_slider_get_value(brightnessSlider_); if (commit) settingsService().save(); break;
        case Action::DimmToggle: settings.ledDimmEnabled[selectedSection_] = !settings.ledDimmEnabled[selectedSection_]; settingsService().save(); break;
        case Action::DimmPercent: settings.ledDimmPercent[selectedSection_] = lv_slider_get_value(dimmSlider_); if (commit) settingsService().save(); break;
        case Action::RemixDefault: settings.ledColorRemixDegrees[selectedCategory_] = 0; settingsService().save(); break;
        case Action::Remix: settings.ledColorRemixDegrees[selectedCategory_] = lv_slider_get_value(remixSlider_); if (commit) settingsService().save(); break;
        case Action::VentAuto: settings.ventMode = VentMode::Automatic; settingsService().save(); ventService().applyNow(); break;
        case Action::VentTarget: settings.ventMode = VentMode::CavityTarget; settingsService().save(); ventService().applyNow(); break;
        case Action::VentManual: settings.ventMode = VentMode::Manual; settingsService().save(); ventService().applyNow(); break;
        case Action::VentTargetTemp: settings.ventTargetTempC = lv_slider_get_value(ventTargetSlider_); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::ManualFan: settings.manualFanPercent = lv_slider_get_value(fanSlider_); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::ManualFlap: settings.manualFlapPercent = lv_slider_get_value(flapSlider_); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::ServoClosed: settings.servoClosedUs = lv_slider_get_value(servoClosedSlider_); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::ServoOpen: settings.servoOpenUs = lv_slider_get_value(servoOpenSlider_); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::ServoReverse: settings.servoReverse = !settings.servoReverse; settingsService().save(); ventService().applyNow(); break;
        case Action::FanMinimum: settings.fanMinPercent = min<int>(lv_slider_get_value(fanMinSlider_), settings.fanMaxPercent); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::FanMaximum: settings.fanMaxPercent = max<int>(lv_slider_get_value(fanMaxSlider_), settings.fanMinPercent); ventService().applyNow(); if (commit) settingsService().save(); break;
        case Action::PandaEnabled: settings.pandaEnabled = !settings.pandaEnabled; settingsService().save(); pandaBreathService().applyNow(); break;
        case Action::PandaMode: settings.pandaMode = static_cast<PandaBreathMode>((static_cast<uint8_t>(settings.pandaMode) + 1U) % static_cast<uint8_t>(PandaBreathMode::Count)); settingsService().save(); pandaBreathService().applyNow(); break;
        case Action::PandaTarget: settings.pandaTargetTempC = lv_slider_get_value(pandaTargetSlider_); pandaBreathService().applyNow(); if (commit) settingsService().save(); break;
        case Action::PandaPreset: settings.pandaDryPreset = static_cast<PandaDryPreset>((static_cast<uint8_t>(settings.pandaDryPreset) + 1U) % static_cast<uint8_t>(PandaDryPreset::Count)); settingsService().save(); pandaBreathService().applyNow(); break;
        case Action::PandaHours: settings.pandaDryHours = lv_slider_get_value(pandaHoursSlider_); pandaBreathService().applyNow(); if (commit) settingsService().save(); break;
        case Action::SoundPrev: selectedSound_ = (selectedSound_ + 4U) % 5U; break;
        case Action::SoundNext: selectedSound_ = (selectedSound_ + 1U) % 5U; break;
        case Action::SoundFilePrev:
        case Action::SoundFileNext: {
            if (audioService().fileCount() == 0) audioService().refreshFileIndex();
            const uint8_t count = audioService().fileCount();
            int current = 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (strcmp(settings.soundPath[selectedSound_], audioService().filePath(i)) == 0) current = i + 1;
            }
            const int choices = count + 1;
            current = action == Action::SoundFileNext ? (current + 1) % choices
                                                      : (current + choices - 1) % choices;
            if (current == 0) settings.soundPath[selectedSound_][0] = '\0';
            else strlcpy(settings.soundPath[selectedSound_], audioService().filePath(current - 1),
                         sizeof(settings.soundPath[selectedSound_]));
            settingsService().save();
            break;
        }
        case Action::SoundVolume: settings.soundVolume[selectedSound_] = lv_slider_get_value(soundVolumeSlider_); if (commit) settingsService().save(); break;
        case Action::SoundRepeat: settings.soundRepeat[selectedSound_] = !settings.soundRepeat[selectedSound_]; settingsService().save(); break;
        case Action::SoundPlay: audioService().playScenario(static_cast<SoundScenario>(selectedSound_)); break;
        case Action::SoundStop: audioService().stop(); break;
    }
    update();
}

void ControlScreen::eventHandler(lv_event_t* event) {
    Binding* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding && binding->owner) binding->owner->handleAction(binding->action, event);
}

}
