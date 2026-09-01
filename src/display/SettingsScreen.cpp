#include "SettingsScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../core/DeviceIdentity.h"
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
    buildDeviceCard(content, 132);
    buildSetupCard(content, 276);
    buildAppearanceCard(content, 414);

    lv_obj_t* endSpacer = lv_obj_create(content);
    lv_obj_set_size(endSpacer, 1, 1);
    lv_obj_set_pos(endSpacer, 0, 510);
    lv_obj_set_style_bg_opa(endSpacer, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(endSpacer, 0, LV_PART_MAIN);
}

void SettingsScreen::buildConnectionCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 124);
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
    lv_obj_set_size(card, 448, 88);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "APPEARANCE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    makeLabel(card, "CORONET", ui::ColorText, &lv_font_montserrat_16, 14, 37);
    makeLabel(card, "DARK", ui::ColorAmber, &lv_font_montserrat_12, 126, 41);
    makeLabel(card, "Additional skins and light modes are planned", ui::ColorMuted,
              &lv_font_montserrat_10, 204, 41, 226);
}

void SettingsScreen::update() {
    if (!root_) return;
    const SystemState& system = state();
    const uint32_t revision = settingsService().revision();
    if (cacheValid_ && revision == settingsRevisionSeen_ &&
        system.wifiConnected == wifiConnectedSeen_ &&
        system.bleConnected == bleConnectedSeen_ &&
        system.printerConnected == printerConnectedSeen_) {
        return;
    }

    cacheValid_ = true;
    settingsRevisionSeen_ = revision;
    wifiConnectedSeen_ = system.wifiConnected;
    bleConnectedSeen_ = system.bleConnected;
    printerConnectedSeen_ = system.printerConnected;
    const AppSettings& settings = settingsService().settings();

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
    }
}

void SettingsScreen::actionEvent(lv_event_t* event) {
    ActionBinding* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
    if (!binding || !binding->owner) return;
    binding->owner->handleAction(binding->action, event);
}

}
