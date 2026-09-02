#include "SettingsScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../config/AppConfig.h"
#include "../companion/PairingService.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"
#include "../update/OtaService.h"
#include "UiTheme.h"

namespace coronet {

namespace {

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    uint32_t color,
                    const lv_font_t* font,
                    int x,
                    int y,
                    int width = LV_SIZE_CONTENT) {
    lv_obj_t* label = lv_label_create(parent);
    styleText(label, color, font);
    if (width != LV_SIZE_CONTENT) lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

void stylePanel(lv_obj_t* panel) {
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(ui::ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN);
}

void styleSmallButton(lv_obj_t* button) {
    lv_obj_set_style_radius(button, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
}

void markTouch() {
    SystemState& system = state();
    system.touchCount++;
    system.lastTouchMs = millis();
}

void rootTouchEvent(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) markTouch();
}

const char* transportDetail(CompanionTransport transport, bool wifiConnected, bool bleConnected) {
    switch (transport) {
        case CompanionTransport::Ble:
            return bleConnected ? "Phone connected directly over Bluetooth LE"
                                : "Waiting for a Bluetooth LE app connection";
        case CompanionTransport::Wifi:
            return wifiConnected ? "Local Wi-Fi control is available"
                                 : "Wi-Fi unavailable; BLE recovery can start automatically";
        case CompanionTransport::Auto:
        default:
            return wifiConnected ? "Wi-Fi preferred with Bluetooth LE fallback"
                                 : "Bluetooth LE remains available while Wi-Fi reconnects";
    }
}

const char* skinName(UiSkin skin) {
    static const char* const names[] = {"CORONET", "GRAPHITE", "AURORA", "MINIMAL"};
    return names[constrain(static_cast<uint8_t>(skin), 0U, 3U)];
}

const char* colorModeName(UiColorMode mode) {
    static const char* const names[] = {"DARK", "LIGHT", "AUTO"};
    return names[constrain(static_cast<uint8_t>(mode), 0U, 2U)];
}

const char* saverModeName(ScreenSaverMode mode) {
    static const char* const names[] = {"DISABLED", "DISPLAY OFF", "CLOCK"};
    return names[constrain(static_cast<uint8_t>(mode), 0U, 2U)];
}

const char* clockStyleName(ClockStyle style) {
    static const char* const names[] = {"DIGITAL", "RETRO", "ANALOG", "LINHO", "BAUHAUS", "MATRIX", "ARC"};
    return names[constrain(static_cast<uint8_t>(style), 0U, 6U)];
}

const char* quietTargetName(QuietTarget target) {
    static const char* const names[] = {"OFF", "SOUND", "LEDS", "SOUND + LEDS"};
    return names[constrain(static_cast<uint8_t>(target), 0U, 3U)];
}

lv_obj_t* makeActionButton(lv_obj_t* parent, int x, int y, int width,
                           lv_obj_t** labelOut) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_size(button, width, 28);
    lv_obj_set_pos(button, x, y);
    styleSmallButton(button);
    lv_obj_t* text = lv_label_create(button);
    styleText(text, ui::ColorText, &lv_font_montserrat_10);
    lv_obj_center(text);
    if (labelOut) *labelOut = text;
    return button;
}

void styleSlider(lv_obj_t* slider) {
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorCyan), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ui::ColorText), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
}

}

void SettingsScreen::begin(ui::Navigation::Callback navigationCallback,
                           SetupCallback setupCallback,
                           void* callbackContext,
                           bool animate) {
    setupCallback_ = setupCallback;
    callbackContext_ = callbackContext;
    cacheValid_ = false;

    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(root_, rootTouchEvent, LV_EVENT_PRESSED, nullptr);

    buildHeader();
    buildContent();
    navigation_.build(root_, ui::Page::Settings, navigationCallback, callbackContext);

    lv_scr_load_anim(root_,
                     animate ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_NONE,
                     animate ? 180 : 0,
                     0,
                     true);
    update();
}

void SettingsScreen::buildHeader() {
    makeLabel(root_, "coroNET", ui::ColorText, &lv_font_montserrat_22, 18, 11);
    makeLabel(root_, "SETTINGS", ui::ColorCyan, &lv_font_montserrat_10, 127, 20);

    wifiLabel_ = makeLabel(root_, LV_SYMBOL_WIFI, ui::ColorMuted,
                           &lv_font_montserrat_16, 389, 14, 24);
    lv_obj_set_style_text_align(wifiLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    bleLabel_ = makeLabel(root_, "BT", ui::ColorMuted,
                          &lv_font_montserrat_12, 425, 17, 32);
    lv_obj_set_style_text_align(bleLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* divider = lv_obj_create(root_);
    lv_obj_set_size(divider, 444, 1);
    lv_obj_set_pos(divider, 18, 49);
    lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(divider, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
}

void SettingsScreen::buildContent() {
    lv_obj_t* content = lv_obj_create(root_);
    lv_obj_set_size(content, 464, 196);
    lv_obj_set_pos(content, 8, 54);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(content, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(content, 0, LV_PART_MAIN);

    buildConnectionCard(content, 0);
    buildDeviceCard(content, 176);
    buildSetupCard(content, 320);
    buildAppearanceCard(content, 458);
    buildQuietCard(content, 752);
    buildSystemCard(content, 892);

    lv_obj_t* endSpacer = lv_obj_create(content);
    lv_obj_set_size(endSpacer, 1, 1);
    lv_obj_set_pos(endSpacer, 0, 1104);
    lv_obj_set_style_bg_opa(endSpacer, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(endSpacer, 0, LV_PART_MAIN);
}

void SettingsScreen::buildConnectionCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 168);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "COMPANION CONNECTION", ui::ColorCyan,
              &lv_font_montserrat_10, 14, 12);

    static const char* const labels[3] = {"AUTO", "BLE", "WI-FI"};
    static const Action actions[3] = {
        Action::TransportAuto, Action::TransportBle, Action::TransportWifi,
    };
    for (uint8_t index = 0; index < 3; ++index) {
        lv_obj_t* button = lv_btn_create(card);
        lv_obj_set_size(button, 128, 36);
        lv_obj_set_pos(button, 14 + index * 138, 34);
        styleSmallButton(button);
        actionBindings_[index] = {this, actions[index]};
        lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[index]);
        transportButtons_[index] = button;

        lv_obj_t* label = lv_label_create(button);
        styleText(label, ui::ColorText, &lv_font_montserrat_12);
        lv_label_set_text(label, labels[index]);
        lv_obj_center(label);
    }
    connectionDetailLabel_ = makeLabel(card, "", ui::ColorMuted,
                                       &lv_font_montserrat_10, 14, 86, 416);
    makeLabel(card, "PHONE LINK", ui::ColorMuted, &lv_font_montserrat_10, 14, 128);
    lv_obj_t* pairingButton = makeActionButton(card, 250, 112, 180,
                                               &pairingButtonLabel_);
    lv_obj_set_style_border_color(pairingButton, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    actionBindings_[20] = {this, Action::PairingStart};
    lv_obj_add_event_cb(pairingButton, actionEvent, LV_EVENT_CLICKED, &actionBindings_[20]);
}

void SettingsScreen::buildDeviceCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 136);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "DEVICE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    makeLabel(card, "NAME", ui::ColorMuted, &lv_font_montserrat_10, 14, 34);
    deviceNameLabel_ = makeLabel(card, "coroNET", ui::ColorText,
                                 &lv_font_montserrat_16, 14, 50, 240);
    lv_label_set_long_mode(deviceNameLabel_, LV_LABEL_LONG_DOT);

    brightnessLabel_ = makeLabel(card, "DISPLAY 80%", ui::ColorMuted,
                                 &lv_font_montserrat_10, 14, 82, 120);
    brightnessSlider_ = lv_slider_create(card);
    lv_obj_set_size(brightnessSlider_, 282, 18);
    lv_obj_set_pos(brightnessSlider_, 148, 86);
    lv_slider_set_range(brightnessSlider_, 10, 100);
    lv_slider_set_value(brightnessSlider_, settingsService().settings().displayBrightness,
                        LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(ui::ColorSurfaceRaised),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(brightnessSlider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(ui::ColorCyan),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(ui::ColorText), LV_PART_KNOB);
    lv_obj_set_style_pad_all(brightnessSlider_, 4, LV_PART_KNOB);
    actionBindings_[3] = {this, Action::Brightness};
    lv_obj_add_event_cb(brightnessSlider_, actionEvent, LV_EVENT_VALUE_CHANGED,
                        &actionBindings_[3]);
    lv_obj_add_event_cb(brightnessSlider_, actionEvent, LV_EVENT_RELEASED,
                        &actionBindings_[3]);
    lv_obj_add_event_cb(brightnessSlider_, actionEvent, LV_EVENT_PRESS_LOST,
                        &actionBindings_[3]);
}

void SettingsScreen::buildSetupCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 130);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "SETUP", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);

    makeLabel(card, "NETWORK", ui::ColorMuted, &lv_font_montserrat_10, 14, 35);
    networkValueLabel_ = makeLabel(card, "Not configured", ui::ColorText,
                                   &lv_font_montserrat_14, 14, 51, 250);
    lv_label_set_long_mode(networkValueLabel_, LV_LABEL_LONG_DOT);
    makeLabel(card, "PRINTER", ui::ColorMuted, &lv_font_montserrat_10, 14, 79);
    printerValueLabel_ = makeLabel(card, "Not configured", ui::ColorText,
                                   &lv_font_montserrat_14, 14, 95, 250);
    lv_label_set_long_mode(printerValueLabel_, LV_LABEL_LONG_DOT);

    lv_obj_t* button = lv_btn_create(card);
    lv_obj_set_size(button, 142, 48);
    lv_obj_set_pos(button, 288, 50);
    styleSmallButton(button);
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    actionBindings_[4] = {this, Action::Reconfigure};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[4]);
    lv_obj_t* label = lv_label_create(button);
    styleText(label, ui::ColorCyan, &lv_font_montserrat_12);
    lv_label_set_text(label, LV_SYMBOL_REFRESH "  RUN SETUP");
    lv_obj_center(label);
}

void SettingsScreen::buildAppearanceCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 286);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "APPEARANCE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    static const char* const rowNames[] = {
        "UI STYLE", "COLOR MODE", "ACCENT", "SCREEN SAVER", "CLOCK STYLE",
        "INACTIVITY", "CLOCK BRIGHTNESS"
    };
    for (uint8_t i = 0; i < 7; ++i) {
        makeLabel(card, rowNames[i], ui::ColorMuted, &lv_font_montserrat_10,
                  14, 39 + i * 36, 180);
    }

    lv_obj_t* button = makeActionButton(card, 250, 30, 180, &skinButtonLabel_);
    actionBindings_[5] = {this, Action::SkinNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[5]);
    button = makeActionButton(card, 250, 66, 180, &colorModeButtonLabel_);
    actionBindings_[6] = {this, Action::ColorModeNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[6]);

    accentLabel_ = makeLabel(card, "190 deg", ui::ColorText, &lv_font_montserrat_10, 168, 111, 72);
    lv_obj_set_style_text_align(accentLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    accentSlider_ = lv_slider_create(card);
    lv_obj_set_size(accentSlider_, 180, 16);
    lv_obj_set_pos(accentSlider_, 250, 103);
    lv_slider_set_range(accentSlider_, 0, 359);
    styleSlider(accentSlider_);
    actionBindings_[7] = {this, Action::AccentHue};
    lv_obj_add_event_cb(accentSlider_, actionEvent, LV_EVENT_VALUE_CHANGED, &actionBindings_[7]);
    lv_obj_add_event_cb(accentSlider_, actionEvent, LV_EVENT_RELEASED, &actionBindings_[7]);

    button = makeActionButton(card, 250, 138, 180, &saverModeButtonLabel_);
    actionBindings_[8] = {this, Action::SaverModeNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[8]);
    button = makeActionButton(card, 250, 174, 180, &clockStyleButtonLabel_);
    actionBindings_[9] = {this, Action::ClockStyleNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[9]);

    saverDelayLabel_ = makeLabel(card, "5 min", ui::ColorText, &lv_font_montserrat_10, 168, 219, 72);
    lv_obj_set_style_text_align(saverDelayLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    saverDelaySlider_ = lv_slider_create(card);
    lv_obj_set_size(saverDelaySlider_, 180, 16);
    lv_obj_set_pos(saverDelaySlider_, 250, 211);
    lv_slider_set_range(saverDelaySlider_, 1, 60);
    styleSlider(saverDelaySlider_);
    actionBindings_[10] = {this, Action::SaverDelay};
    lv_obj_add_event_cb(saverDelaySlider_, actionEvent, LV_EVENT_VALUE_CHANGED, &actionBindings_[10]);
    lv_obj_add_event_cb(saverDelaySlider_, actionEvent, LV_EVENT_RELEASED, &actionBindings_[10]);

    clockBrightnessLabel_ = makeLabel(card, "35%", ui::ColorText, &lv_font_montserrat_10, 168, 255, 72);
    lv_obj_set_style_text_align(clockBrightnessLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    clockBrightnessSlider_ = lv_slider_create(card);
    lv_obj_set_size(clockBrightnessSlider_, 180, 16);
    lv_obj_set_pos(clockBrightnessSlider_, 250, 247);
    lv_slider_set_range(clockBrightnessSlider_, 5, 100);
    styleSlider(clockBrightnessSlider_);
    actionBindings_[11] = {this, Action::ClockBrightness};
    lv_obj_add_event_cb(clockBrightnessSlider_, actionEvent, LV_EVENT_VALUE_CHANGED, &actionBindings_[11]);
    lv_obj_add_event_cb(clockBrightnessSlider_, actionEvent, LV_EVENT_RELEASED, &actionBindings_[11]);
}

void SettingsScreen::buildQuietCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 132);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "QUIET MODE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    makeLabel(card, "TARGET", ui::ColorMuted, &lv_font_montserrat_10, 14, 43);
    makeLabel(card, "DURATION", ui::ColorMuted, &lv_font_montserrat_10, 14, 79);
    makeLabel(card, "ERROR ALERTS", ui::ColorMuted, &lv_font_montserrat_10, 14, 111);

    lv_obj_t* button = makeActionButton(card, 250, 30, 180, &quietTargetButtonLabel_);
    actionBindings_[12] = {this, Action::QuietTargetNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[12]);
    quietDurationLabel_ = makeLabel(card, "60 min", ui::ColorText, &lv_font_montserrat_10, 168, 84, 72);
    lv_obj_set_style_text_align(quietDurationLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    quietDurationSlider_ = lv_slider_create(card);
    lv_obj_set_size(quietDurationSlider_, 180, 16);
    lv_obj_set_pos(quietDurationSlider_, 250, 76);
    lv_slider_set_range(quietDurationSlider_, 5, 240);
    styleSlider(quietDurationSlider_);
    actionBindings_[13] = {this, Action::QuietDuration};
    lv_obj_add_event_cb(quietDurationSlider_, actionEvent, LV_EVENT_VALUE_CHANGED, &actionBindings_[13]);
    lv_obj_add_event_cb(quietDurationSlider_, actionEvent, LV_EVENT_RELEASED, &actionBindings_[13]);
    button = makeActionButton(card, 250, 101, 180, &quietErrorsButtonLabel_);
    actionBindings_[14] = {this, Action::QuietErrorsBypass};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[14]);
}

void SettingsScreen::buildSystemCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 204);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "FIRMWARE & RECOVERY", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    otaStatusLabel_ = makeLabel(card, "Ready", ui::ColorText, &lv_font_montserrat_12, 14, 34, 416);
    lv_label_set_long_mode(otaStatusLabel_, LV_LABEL_LONG_DOT);
    otaVersionLabel_ = makeLabel(card, "", ui::ColorMuted, &lv_font_montserrat_10, 14, 54, 416);

    lv_obj_t* button = makeActionButton(card, 14, 76, 128, nullptr);
    lv_label_set_text(lv_obj_get_child(button, 0), "CHECK");
    actionBindings_[15] = {this, Action::OtaCheck};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[15]);
    otaInstallButton_ = makeActionButton(card, 160, 76, 128, nullptr);
    lv_label_set_text(lv_obj_get_child(otaInstallButton_, 0), "INSTALL");
    actionBindings_[16] = {this, Action::OtaInstall};
    lv_obj_add_event_cb(otaInstallButton_, actionEvent, LV_EVENT_CLICKED, &actionBindings_[16]);
    button = makeActionButton(card, 306, 76, 124, nullptr);
    lv_label_set_text(lv_obj_get_child(button, 0), "REINSTALL");
    actionBindings_[17] = {this, Action::OtaReinstall};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[17]);

    button = makeActionButton(card, 14, 116, 202, nullptr);
    lv_label_set_text(lv_obj_get_child(button, 0), "SD RECOVERY");
    actionBindings_[18] = {this, Action::OtaSdRecovery};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[18]);
    button = makeActionButton(card, 228, 116, 202, &factoryResetButtonLabel_);
    lv_label_set_text(factoryResetButtonLabel_, "FACTORY RESET");
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorRed), LV_PART_MAIN);
    actionBindings_[19] = {this, Action::FactoryReset};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[19]);

    makeLabel(card, "SD recovery expects /firmware.bin and renames it after a successful install.",
              ui::ColorMuted, &lv_font_montserrat_10, 14, 160, 416);
}

void SettingsScreen::update() {
    if (!root_) return;
    if (pairingOverlay_) updatePairingWizard();
    const SystemState& system = state();
    const uint32_t revision = settingsService().revision();
    if (cacheValid_ && revision == settingsRevisionSeen_ &&
        system.wifiConnected == wifiConnectedSeen_ &&
        system.bleConnected == bleConnectedSeen_ &&
        system.printerConnected == printerConnectedSeen_ &&
        system.otaState == otaStateSeen_ && system.otaProgress == otaProgressSeen_ &&
        !(factoryConfirmUntilMs_ && millis() >= factoryConfirmUntilMs_)) {
        return;
    }

    cacheValid_ = true;
    settingsRevisionSeen_ = revision;
    wifiConnectedSeen_ = system.wifiConnected;
    bleConnectedSeen_ = system.bleConnected;
    printerConnectedSeen_ = system.printerConnected;
    otaStateSeen_ = system.otaState;
    otaProgressSeen_ = system.otaProgress;
    const AppSettings& settings = settingsService().settings();

    lv_label_set_text(pairingButtonLabel_, settings.apiPaired ? "PAIR NEW PHONE" : "PAIR PHONE");

    lv_obj_set_style_text_color(wifiLabel_,
                                lv_color_hex(system.wifiConnected ? ui::ColorCyan
                                                                 : ui::ColorMuted),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(bleLabel_,
                                lv_color_hex(system.bleConnected ? ui::ColorCyan
                                                                : ui::ColorMuted),
                                LV_PART_MAIN);
    lv_label_set_text(connectionDetailLabel_,
                      transportDetail(settings.companionTransport,
                                      system.wifiConnected,
                                      system.bleConnected));
    refreshTransportButtons();

    char effectiveName[32] = "";
    deviceIdentity().effectiveName(settings.deviceName, effectiveName, sizeof(effectiveName));
    lv_label_set_text(deviceNameLabel_, effectiveName);
    lv_label_set_text_fmt(brightnessLabel_, "DISPLAY %u%%",
                          static_cast<unsigned>(settings.displayBrightness));
    if (!lv_obj_has_state(brightnessSlider_, LV_STATE_PRESSED)) {
        lv_slider_set_value(brightnessSlider_, settings.displayBrightness, LV_ANIM_OFF);
    }

    lv_label_set_text(networkValueLabel_, settings.wifiSsid[0] ? settings.wifiSsid
                                                               : "Not configured");
    lv_obj_set_style_text_color(networkValueLabel_,
                                lv_color_hex(system.wifiConnected ? ui::ColorGreen
                                                                 : ui::ColorText),
                                LV_PART_MAIN);
    lv_label_set_text(printerValueLabel_, settings.printerHost[0] ? settings.printerHost
                                                                  : "Not configured");
    lv_obj_set_style_text_color(printerValueLabel_,
                                lv_color_hex(system.printerConnected ? ui::ColorGreen
                                                                    : ui::ColorText),
                                LV_PART_MAIN);

    lv_label_set_text(skinButtonLabel_, skinName(settings.uiSkin));
    lv_label_set_text(colorModeButtonLabel_, colorModeName(settings.uiColorMode));
    lv_label_set_text_fmt(accentLabel_, "%u deg", static_cast<unsigned>(settings.accentHueDegrees));
    if (!lv_obj_has_state(accentSlider_, LV_STATE_PRESSED))
        lv_slider_set_value(accentSlider_, settings.accentHueDegrees, LV_ANIM_OFF);
    lv_label_set_text(saverModeButtonLabel_, saverModeName(settings.screenSaverMode));
    lv_label_set_text(clockStyleButtonLabel_, clockStyleName(settings.clockStyle));
    lv_label_set_text_fmt(saverDelayLabel_, "%u min", static_cast<unsigned>(settings.screenSaverDelayMinutes));
    if (!lv_obj_has_state(saverDelaySlider_, LV_STATE_PRESSED))
        lv_slider_set_value(saverDelaySlider_, settings.screenSaverDelayMinutes, LV_ANIM_OFF);
    lv_label_set_text_fmt(clockBrightnessLabel_, "%u%%", static_cast<unsigned>(settings.clockBrightness));
    if (!lv_obj_has_state(clockBrightnessSlider_, LV_STATE_PRESSED))
        lv_slider_set_value(clockBrightnessSlider_, settings.clockBrightness, LV_ANIM_OFF);
    lv_label_set_text(quietTargetButtonLabel_, quietTargetName(settings.quietTarget));
    lv_label_set_text_fmt(quietDurationLabel_, "%u min", static_cast<unsigned>(settings.quietDurationMinutes));
    if (!lv_obj_has_state(quietDurationSlider_, LV_STATE_PRESSED))
        lv_slider_set_value(quietDurationSlider_, settings.quietDurationMinutes, LV_ANIM_OFF);
    lv_label_set_text(quietErrorsButtonLabel_, settings.quietErrorsBypass ? "ALWAYS ALERT" : "MUTED");

    lv_label_set_text(otaStatusLabel_, system.otaStatusText);
    if (system.otaAvailableVersion[0]) {
        char versionText[64];
        snprintf(versionText, sizeof(versionText), "Installed %s  |  Latest %s",
                 config::FirmwareVersion, system.otaAvailableVersion);
        lv_label_set_text(otaVersionLabel_, versionText);
    } else {
        lv_label_set_text_fmt(otaVersionLabel_, "Installed %s", config::FirmwareVersion);
    }
    const bool updateBusy = system.otaState == OtaState::Checking ||
                            system.otaState == OtaState::Preparing ||
                            system.otaState == OtaState::Downloading ||
                            system.otaState == OtaState::Installing;
    if (updateBusy) lv_obj_add_state(otaInstallButton_, LV_STATE_DISABLED);
    else lv_obj_clear_state(otaInstallButton_, LV_STATE_DISABLED);
    if (factoryConfirmUntilMs_ && millis() >= factoryConfirmUntilMs_) {
        factoryConfirmUntilMs_ = 0;
        lv_label_set_text(factoryResetButtonLabel_, "FACTORY RESET");
    }
}

void SettingsScreen::showPairingWizard() {
    if (pairingOverlay_) return;
    const PairingSnapshot pairing = pairingService().beginPairing();
    pairingSuccessCloseAtMs_ = 0;

    pairingOverlay_ = lv_obj_create(root_);
    lv_obj_set_size(pairingOverlay_, 480, 320);
    lv_obj_set_pos(pairingOverlay_, 0, 0);
    lv_obj_clear_flag(pairingOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(pairingOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(pairingOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pairingOverlay_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pairingOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pairingOverlay_, 0, LV_PART_MAIN);

    makeLabel(pairingOverlay_, "PAIR YOUR PHONE", ui::ColorCyan,
              &lv_font_montserrat_12, 24, 18);
    makeLabel(pairingOverlay_, "Open the coroNET app and select this device.", ui::ColorText,
              &lv_font_montserrat_16, 24, 46, 432);
    makeLabel(pairingOverlay_, "Confirm that the same code appears on both screens.", ui::ColorMuted,
              &lv_font_montserrat_12, 24, 72, 432);

    pairingCodeLabel_ = makeLabel(pairingOverlay_, "000 000", ui::ColorText,
                                  &lv_font_montserrat_32, 24, 106, 432);
    lv_obj_set_style_text_align(pairingCodeLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    pairingStatusLabel_ = makeLabel(pairingOverlay_, "Waiting for phone", ui::ColorMuted,
                                    &lv_font_montserrat_12, 24, 158, 432);
    lv_obj_set_style_text_align(pairingStatusLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    pairingTimerLabel_ = makeLabel(pairingOverlay_, "02:00", ui::ColorMuted,
                                   &lv_font_montserrat_10, 24, 184, 432);
    lv_obj_set_style_text_align(pairingTimerLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    pairingCancelButton_ = lv_btn_create(pairingOverlay_);
    lv_obj_set_size(pairingCancelButton_, 190, 48);
    lv_obj_set_pos(pairingCancelButton_, 24, 238);
    styleSmallButton(pairingCancelButton_);
    actionBindings_[22] = {this, Action::PairingCancel};
    lv_obj_add_event_cb(pairingCancelButton_, actionEvent, LV_EVENT_CLICKED, &actionBindings_[22]);
    lv_obj_t* cancelLabel = lv_label_create(pairingCancelButton_);
    styleText(cancelLabel, ui::ColorMuted, &lv_font_montserrat_12);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_center(cancelLabel);

    pairingConfirmButton_ = lv_btn_create(pairingOverlay_);
    lv_obj_set_size(pairingConfirmButton_, 218, 48);
    lv_obj_set_pos(pairingConfirmButton_, 238, 238);
    styleSmallButton(pairingConfirmButton_);
    lv_obj_set_style_border_color(pairingConfirmButton_, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    actionBindings_[21] = {this, Action::PairingDeviceConfirm};
    lv_obj_add_event_cb(pairingConfirmButton_, actionEvent, LV_EVENT_CLICKED, &actionBindings_[21]);
    pairingConfirmLabel_ = lv_label_create(pairingConfirmButton_);
    styleText(pairingConfirmLabel_, ui::ColorCyan, &lv_font_montserrat_12);
    lv_label_set_text(pairingConfirmLabel_, "CODES MATCH");
    lv_obj_center(pairingConfirmLabel_);

    updatePairingWizard();
}

void SettingsScreen::updatePairingWizard() {
    if (!pairingOverlay_) return;
    const PairingSnapshot pairing = pairingService().snapshot();
    const uint32_t now = millis();
    const uint32_t remainingMs = static_cast<int32_t>(pairing.expiresAtMs - millis()) > 0
                                     ? pairing.expiresAtMs - millis() : 0;

    if (pairing.phase != PairingPhase::Completed) {
        pairingSuccessCloseAtMs_ = 0;
        lv_label_set_text_fmt(pairingCodeLabel_, "%03lu %03lu",
                              static_cast<unsigned long>(pairing.code / 1000U),
                              static_cast<unsigned long>(pairing.code % 1000U));
        lv_obj_set_style_text_color(pairingCodeLabel_, lv_color_hex(ui::ColorText), LV_PART_MAIN);
        lv_label_set_text_fmt(pairingTimerLabel_, "%02lu:%02lu",
                              static_cast<unsigned long>(remainingMs / 60000U),
                              static_cast<unsigned long>((remainingMs / 1000U) % 60U));
    }

    const bool terminal = pairing.phase == PairingPhase::Completed ||
                          pairing.phase == PairingPhase::Cancelled ||
                          pairing.phase == PairingPhase::Expired;
    if (terminal) {
        if (pairing.phase == PairingPhase::Completed) {
            if (pairingSuccessCloseAtMs_ == 0) pairingSuccessCloseAtMs_ = now + 10000U;
            if (static_cast<int32_t>(pairingSuccessCloseAtMs_ - now) <= 0) {
                closePairingWizard();
                return;
            }
            const uint32_t secondsLeft = (pairingSuccessCloseAtMs_ - now + 999U) / 1000U;
            lv_label_set_text(pairingCodeLabel_, "SUCCESS!");
            lv_obj_set_style_text_color(pairingCodeLabel_, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
            lv_label_set_text(pairingStatusLabel_, "Your phone is securely paired and ready.");
            lv_label_set_text_fmt(pairingTimerLabel_,
                                  "This window will close automatically in %lu seconds.",
                                  static_cast<unsigned long>(secondsLeft));
        } else {
            lv_label_set_text(pairingStatusLabel_,
                              pairing.phase == PairingPhase::Expired ? "Pairing session expired"
                                                                     : "Pairing cancelled");
            lv_label_set_text(pairingTimerLabel_,
                              pairing.phase == PairingPhase::Expired ? "SESSION EXPIRED"
                                                                     : "SESSION CLOSED");
        }
        lv_label_set_text(pairingConfirmLabel_, "CLOSE");
        actionBindings_[21].action = Action::PairingDone;
        lv_obj_clear_state(pairingConfirmButton_, LV_STATE_DISABLED);
        lv_obj_add_flag(pairingCancelButton_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(pairingConfirmButton_, 432);
        lv_obj_set_x(pairingConfirmButton_, 24);
    } else {
        actionBindings_[21].action = Action::PairingDeviceConfirm;
        if (pairing.phoneConfirmed && pairing.deviceConfirmed) {
            lv_label_set_text(pairingStatusLabel_, "Securing the new connection...");
        } else if (pairing.phoneConfirmed) {
            lv_label_set_text(pairingStatusLabel_, "Phone confirmed - confirm here");
        } else if (pairing.deviceConfirmed) {
            lv_label_set_text(pairingStatusLabel_, "Confirmed here - waiting for phone");
        } else {
            lv_label_set_text(pairingStatusLabel_, "Waiting for confirmation on both devices");
        }
        lv_label_set_text(pairingConfirmLabel_, pairing.deviceConfirmed ? "CONFIRMED HERE" : "CODES MATCH");
        if (pairing.deviceConfirmed) lv_obj_add_state(pairingConfirmButton_, LV_STATE_DISABLED);
        else lv_obj_clear_state(pairingConfirmButton_, LV_STATE_DISABLED);
    }
}

void SettingsScreen::closePairingWizard() {
    if (!pairingOverlay_) return;
    lv_obj_t* overlay = pairingOverlay_;
    pairingOverlay_ = nullptr;
    pairingCodeLabel_ = nullptr;
    pairingStatusLabel_ = nullptr;
    pairingTimerLabel_ = nullptr;
    pairingConfirmButton_ = nullptr;
    pairingConfirmLabel_ = nullptr;
    pairingCancelButton_ = nullptr;
    pairingSuccessCloseAtMs_ = 0;
    pairingService().dismiss();
    lv_obj_del_async(overlay);
    cacheValid_ = false;
}

void SettingsScreen::refreshTransportButtons() {
    const uint8_t selected = static_cast<uint8_t>(settingsService().settings().companionTransport);
    for (uint8_t index = 0; index < 3; ++index) {
        const bool active = index == selected;
        lv_obj_set_style_border_color(transportButtons_[index],
                                      lv_color_hex(active ? ui::ColorCyan : ui::ColorBorder),
                                      LV_PART_MAIN);
        lv_obj_set_style_bg_color(transportButtons_[index],
                                  lv_color_hex(active ? ui::ColorCyanDark
                                                      : ui::ColorSurfaceRaised),
                                  LV_PART_MAIN);
    }
}

void SettingsScreen::handleAction(Action action, lv_event_t* event) {
    AppSettings& settings = settingsService().mutableSettings();
    switch (action) {
        case Action::TransportAuto:
            settings.companionTransport = CompanionTransport::Auto;
            settings.bleEnabled = true;
            settingsService().save();
            update();
            break;
        case Action::TransportBle:
            settings.companionTransport = CompanionTransport::Ble;
            settings.bleEnabled = true;
            settingsService().save();
            update();
            break;
        case Action::TransportWifi:
            settings.companionTransport = CompanionTransport::Wifi;
            settings.bleEnabled = true;
            settingsService().save();
            update();
            break;
        case Action::Brightness: {
            const uint8_t value = static_cast<uint8_t>(lv_slider_get_value(brightnessSlider_));
            settings.displayBrightness = value;
            lv_label_set_text_fmt(brightnessLabel_, "DISPLAY %u%%",
                                  static_cast<unsigned>(value));
            const lv_event_code_t code = lv_event_get_code(event);
            if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
                settingsService().save();
            }
            break;
        }
        case Action::Reconfigure:
            if (setupCallback_) setupCallback_(callbackContext_);
            break;
        case Action::SkinNext:
            settings.uiSkin = static_cast<UiSkin>((static_cast<uint8_t>(settings.uiSkin) + 1U) % 4U);
            settingsService().save();
            break;
        case Action::ColorModeNext:
            settings.uiColorMode = static_cast<UiColorMode>((static_cast<uint8_t>(settings.uiColorMode) + 1U) % 3U);
            settingsService().save();
            break;
        case Action::AccentHue: {
            settings.accentHueDegrees = static_cast<uint16_t>(lv_slider_get_value(accentSlider_));
            lv_label_set_text_fmt(accentLabel_, "%u deg", static_cast<unsigned>(settings.accentHueDegrees));
            if (lv_event_get_code(event) == LV_EVENT_RELEASED) settingsService().save();
            break;
        }
        case Action::SaverModeNext:
            settings.screenSaverMode = static_cast<ScreenSaverMode>((static_cast<uint8_t>(settings.screenSaverMode) + 1U) % 3U);
            settingsService().save();
            break;
        case Action::ClockStyleNext:
            settings.clockStyle = static_cast<ClockStyle>((static_cast<uint8_t>(settings.clockStyle) + 1U) % static_cast<uint8_t>(ClockStyle::Count));
            settingsService().save();
            break;
        case Action::SaverDelay: {
            settings.screenSaverDelayMinutes = static_cast<uint8_t>(lv_slider_get_value(saverDelaySlider_));
            lv_label_set_text_fmt(saverDelayLabel_, "%u min", static_cast<unsigned>(settings.screenSaverDelayMinutes));
            if (lv_event_get_code(event) == LV_EVENT_RELEASED) settingsService().save();
            break;
        }
        case Action::ClockBrightness: {
            settings.clockBrightness = static_cast<uint8_t>(lv_slider_get_value(clockBrightnessSlider_));
            lv_label_set_text_fmt(clockBrightnessLabel_, "%u%%", static_cast<unsigned>(settings.clockBrightness));
            if (lv_event_get_code(event) == LV_EVENT_RELEASED) settingsService().save();
            break;
        }
        case Action::QuietTargetNext:
            settings.quietTarget = static_cast<QuietTarget>((static_cast<uint8_t>(settings.quietTarget) + 1U) % 4U);
            settingsService().save();
            break;
        case Action::QuietDuration: {
            settings.quietDurationMinutes = static_cast<uint16_t>(lv_slider_get_value(quietDurationSlider_));
            lv_label_set_text_fmt(quietDurationLabel_, "%u min", static_cast<unsigned>(settings.quietDurationMinutes));
            if (lv_event_get_code(event) == LV_EVENT_RELEASED) settingsService().save();
            break;
        }
        case Action::QuietErrorsBypass:
            settings.quietErrorsBypass = !settings.quietErrorsBypass;
            settingsService().save();
            break;
        case Action::OtaCheck:
            otaService().requestCheck();
            break;
        case Action::OtaInstall:
            otaService().requestInstall(false);
            break;
        case Action::OtaReinstall:
            otaService().requestInstall(true);
            break;
        case Action::OtaSdRecovery:
            otaService().requestSdRecovery();
            break;
        case Action::FactoryReset:
            if (factoryConfirmUntilMs_ && millis() < factoryConfirmUntilMs_) {
                otaService().factoryReset();
            } else {
                factoryConfirmUntilMs_ = millis() + 5000U;
                lv_label_set_text(factoryResetButtonLabel_, "CONFIRM RESET");
            }
            break;
        case Action::PairingStart:
            showPairingWizard();
            break;
        case Action::PairingDeviceConfirm: {
            const PairingSnapshot pairing = pairingService().snapshot();
            pairingService().confirmOnDevice(pairing.sessionId);
            updatePairingWizard();
            break;
        }
        case Action::PairingCancel: {
            const PairingSnapshot pairing = pairingService().snapshot();
            pairingService().cancel(pairing.sessionId);
            updatePairingWizard();
            break;
        }
        case Action::PairingDone:
            closePairingWizard();
            break;
    }
}

void SettingsScreen::actionEvent(lv_event_t* event) {
    ActionBinding* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
    if (!binding || !binding->owner) return;
    binding->owner->handleAction(binding->action, event);
}

}
