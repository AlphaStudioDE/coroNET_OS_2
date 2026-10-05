#include "SettingsScreen.h"

#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>

#include "../config/AppConfig.h"
#include "../companion/PairingService.h"
#include "../core/DeviceIdentity.h"
#include "../core/SystemState.h"
#include "../core/TimeZoneCatalog.h"
#include "../settings/SettingsService.h"
#include "../update/OtaService.h"
#include "UiTheme.h"
#include "UiHeader.h"

namespace coronet {

namespace {

constexpr uint8_t TimeZonePageSize = 6;
constexpr uint8_t TimeZonePageCount =
    static_cast<uint8_t>((TimeZoneOptionCount + TimeZonePageSize - 1) / TimeZonePageSize);
constexpr int kCardSliderRight = 422;
constexpr int kSliderClickPadding = 13;
constexpr const char* QuietHourOptions =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24";
constexpr const char* QuietMinuteOptions =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19"
    "\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39"
    "\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

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
    lv_obj_set_ext_click_area(slider, kSliderClickPadding);
}

void styleTimeRoller(lv_obj_t* roller) {
    lv_obj_set_style_bg_color(roller, lv_color_hex(ui::ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_radius(roller, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(ui::ColorMuted), LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(roller, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, lv_color_hex(ui::ColorCyanDark), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_hex(ui::ColorText), LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_22, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 1, LV_PART_SELECTED);
    lv_obj_set_style_border_color(roller, lv_color_hex(ui::ColorCyan), LV_PART_SELECTED);
}

void setQuietDurationText(lv_obj_t* label, uint16_t totalMinutes) {
    if (!label) return;
    if (totalMinutes == 0) {
        lv_label_set_text(label, "UNLIMITED");
        return;
    }
    lv_label_set_text_fmt(label, "%02u : %02u",
                          static_cast<unsigned>(totalMinutes / 60U),
                          static_cast<unsigned>(totalMinutes % 60U));
}

}

void SettingsScreen::begin(ui::Navigation::Callback navigationCallback,
                           SetupCallback setupCallback,
                           void* callbackContext,
                           bool animate,
                           bool preserveScroll) {
    setupCallback_ = setupCallback;
    callbackContext_ = callbackContext;
    cacheValid_ = false;
    if (!preserveScroll) scrollY_ = 0;

    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);

    buildHeader();
    buildContent();
    navigation_.build(root_, ui::Page::Settings, navigationCallback, callbackContext);

    lv_obj_update_layout(root_);
    lv_obj_scroll_to_y(content_, scrollY_, LV_ANIM_OFF);

    lv_scr_load_anim(root_,
                     animate ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_NONE,
                     animate ? 180 : 0,
                     0,
                     true);
    update();
}

void SettingsScreen::buildHeader() {
    header_ = ui::buildHeader(root_, "SETTINGS");
}

void SettingsScreen::buildContent() {
    lv_obj_t* content = lv_obj_create(root_);
    content_ = content;
    lv_obj_set_size(content, 464, ui::HeaderContentHeight);
    lv_obj_set_pos(content, 8, ui::HeaderContentTop);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(content, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(content, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(content, scrollEvent, LV_EVENT_SCROLL, this);

    buildConnectionCard(content, 0);
    buildDeviceCard(content, 176);
    buildSetupCard(content, 320);
    buildAppearanceCard(content, 458);
    buildQuietCard(content, 914);
    buildSystemCard(content, 1088);

    lv_obj_t* endSpacer = lv_obj_create(content);
    lv_obj_set_size(endSpacer, 1, 1);
    lv_obj_set_pos(endSpacer, 0, 1320);
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
    lv_obj_t* pairingButton = makeActionButton(card, 122, 112, 146,
                                               &pairingButtonLabel_);
    lv_obj_set_style_border_color(pairingButton, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    actionBindings_[20] = {this, Action::PairingStart};
    lv_obj_add_event_cb(pairingButton, actionEvent, LV_EVENT_CLICKED, &actionBindings_[20]);

    lv_obj_t* portalLabel = nullptr;
    lv_obj_t* portalButton = makeActionButton(card, 278, 112, 152, &portalLabel);
    lv_obj_set_style_border_color(portalButton, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_obj_set_style_text_color(portalLabel, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_label_set_text(portalLabel, "PORTAL");
    actionBindings_[28] = {this, Action::PortalOpen};
    lv_obj_add_event_cb(portalButton, actionEvent, LV_EVENT_CLICKED, &actionBindings_[28]);
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
    lv_obj_set_size(brightnessSlider_, kCardSliderRight - 148, 18);
    lv_obj_set_pos(brightnessSlider_, 148, 86);
    lv_slider_set_range(brightnessSlider_, 10, 100);
    lv_slider_set_value(brightnessSlider_, settingsService().settings().displayBrightness,
                        LV_ANIM_OFF);
    styleSlider(brightnessSlider_);
    bindSlider(brightnessSlider_, 3, Action::Brightness);
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
    lv_obj_set_size(card, 448, 448);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "APPEARANCE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    static const char* const rowNames[] = {
        "UI STYLE", "COLOR MODE", "ACCENT", "SCREEN SAVER", "CLOCK STYLE",
        "TIME FORMAT", "TIME ZONE", "INACTIVITY", "CLOCK BRIGHTNESS"
    };
    for (uint8_t i = 0; i < 9; ++i) {
        makeLabel(card, rowNames[i], ui::ColorMuted, &lv_font_montserrat_10,
                  14, 39 + i * 48, 180);
    }

    lv_obj_t* button = makeActionButton(card, 250, 30, 180, &skinButtonLabel_);
    actionBindings_[5] = {this, Action::SkinNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[5]);
    button = makeActionButton(card, 250, 78, 180, &colorModeButtonLabel_);
    actionBindings_[6] = {this, Action::ColorModeNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[6]);

    accentLabel_ = makeLabel(card, "190 deg", ui::ColorText, &lv_font_montserrat_10, 168, 134, 72);
    lv_obj_set_style_text_align(accentLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    accentSlider_ = lv_slider_create(card);
    lv_obj_set_size(accentSlider_, kCardSliderRight - 250, 16);
    lv_obj_set_pos(accentSlider_, 250, 126);
    lv_slider_set_range(accentSlider_, 0, 359);
    styleSlider(accentSlider_);
    bindSlider(accentSlider_, 7, Action::AccentHue);

    button = makeActionButton(card, 250, 174, 180, &saverModeButtonLabel_);
    actionBindings_[8] = {this, Action::SaverModeNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[8]);
    button = makeActionButton(card, 250, 222, 180, &clockStyleButtonLabel_);
    actionBindings_[9] = {this, Action::ClockStyleNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[9]);

    button = makeActionButton(card, 250, 270, 180, &clockFormatButtonLabel_);
    actionBindings_[24] = {this, Action::ClockFormatNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[24]);
    button = makeActionButton(card, 250, 318, 180, &timeZoneButtonLabel_);
    lv_obj_set_width(timeZoneButtonLabel_, 168);
    lv_label_set_long_mode(timeZoneButtonLabel_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(timeZoneButtonLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(timeZoneButtonLabel_);
    actionBindings_[23] = {this, Action::TimeZoneOpen};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[23]);

    saverDelayLabel_ = makeLabel(card, "5 min", ui::ColorText, &lv_font_montserrat_10, 168, 374, 72);
    lv_obj_set_style_text_align(saverDelayLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    saverDelaySlider_ = lv_slider_create(card);
    lv_obj_set_size(saverDelaySlider_, kCardSliderRight - 250, 16);
    lv_obj_set_pos(saverDelaySlider_, 250, 366);
    lv_slider_set_range(saverDelaySlider_, 1, 60);
    styleSlider(saverDelaySlider_);
    bindSlider(saverDelaySlider_, 10, Action::SaverDelay);

    clockBrightnessLabel_ = makeLabel(card, "35%", ui::ColorText, &lv_font_montserrat_10, 168, 422, 72);
    lv_obj_set_style_text_align(clockBrightnessLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    clockBrightnessSlider_ = lv_slider_create(card);
    lv_obj_set_size(clockBrightnessSlider_, kCardSliderRight - 250, 16);
    lv_obj_set_pos(clockBrightnessSlider_, 250, 414);
    lv_slider_set_range(clockBrightnessSlider_, 5, 100);
    styleSlider(clockBrightnessSlider_);
    bindSlider(clockBrightnessSlider_, 11, Action::ClockBrightness);
}

void SettingsScreen::buildQuietCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 166);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "QUIET MODE", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    makeLabel(card, "TARGET", ui::ColorMuted, &lv_font_montserrat_10, 14, 43);
    makeLabel(card, "DURATION", ui::ColorMuted, &lv_font_montserrat_10, 14, 91);
    makeLabel(card, "ERROR ALERTS", ui::ColorMuted, &lv_font_montserrat_10, 14, 139);

    lv_obj_t* button = makeActionButton(card, 250, 30, 180, &quietTargetButtonLabel_);
    actionBindings_[12] = {this, Action::QuietTargetNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[12]);
    button = makeActionButton(card, 250, 78, 180, &quietDurationLabel_);
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    actionBindings_[13] = {this, Action::QuietDurationOpen};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[13]);
    button = makeActionButton(card, 250, 126, 180, &quietErrorsButtonLabel_);
    actionBindings_[14] = {this, Action::QuietErrorsBypass};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[14]);
}

void SettingsScreen::buildSystemCard(lv_obj_t* parent, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 448, 224);
    lv_obj_set_pos(card, 0, y);
    stylePanel(card);
    makeLabel(card, "FIRMWARE & RECOVERY", ui::ColorCyan, &lv_font_montserrat_10, 14, 12);
    otaStatusLabel_ = makeLabel(card, "Ready", ui::ColorText, &lv_font_montserrat_12, 14, 34, 416);
    lv_label_set_long_mode(otaStatusLabel_, LV_LABEL_LONG_DOT);
    otaVersionLabel_ = makeLabel(card, "", ui::ColorMuted, &lv_font_montserrat_10, 14, 54, 416);

    lv_obj_t* button = makeActionButton(card, 14, 76, 128, &otaButtonLabels_[0]);
    otaButtons_[0] = button;
    lv_label_set_text(otaButtonLabels_[0], "CHECK");
    actionBindings_[15] = {this, Action::OtaCheck};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[15]);
    otaInstallButton_ = makeActionButton(card, 160, 76, 128, &otaButtonLabels_[1]);
    otaButtons_[1] = otaInstallButton_;
    lv_label_set_text(otaButtonLabels_[1], "INSTALL");
    actionBindings_[16] = {this, Action::OtaInstall};
    lv_obj_add_event_cb(otaInstallButton_, actionEvent, LV_EVENT_CLICKED, &actionBindings_[16]);
    button = makeActionButton(card, 306, 76, 124, &otaButtonLabels_[2]);
    otaButtons_[2] = button;
    lv_label_set_text(otaButtonLabels_[2], "REINSTALL");
    actionBindings_[17] = {this, Action::OtaReinstall};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[17]);

    button = makeActionButton(card, 14, 116, 202, &otaButtonLabels_[3]);
    otaButtons_[3] = button;
    lv_label_set_text(otaButtonLabels_[3], "SD RECOVERY");
    actionBindings_[18] = {this, Action::OtaSdRecovery};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[18]);
    button = makeActionButton(card, 228, 116, 202, &factoryResetButtonLabel_);
    lv_label_set_text(factoryResetButtonLabel_, "FACTORY RESET");
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorRed), LV_PART_MAIN);
    actionBindings_[19] = {this, Action::FactoryReset};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[19]);

    otaProgressLabel_ = makeLabel(card, "0%", ui::ColorMuted, &lv_font_montserrat_10, 370, 151, 60);
    lv_obj_set_style_text_align(otaProgressLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    otaProgressBar_ = lv_bar_create(card);
    lv_obj_set_size(otaProgressBar_, 342, 8);
    lv_obj_set_pos(otaProgressBar_, 14, 154);
    lv_bar_set_range(otaProgressBar_, 0, 100);
    lv_obj_set_style_radius(otaProgressBar_, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(otaProgressBar_, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(otaProgressBar_, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_color(otaProgressBar_, lv_color_hex(ui::ColorCyan), LV_PART_INDICATOR);
    makeLabel(card, "SD recovery reads /firmware.bin and archives it after installation.",
              ui::ColorMuted, &lv_font_montserrat_10, 14, 181, 416);
}

void SettingsScreen::update() {
    if (!root_) return;
    if (pairingOverlay_) updatePairingWizard();
    const SystemState system = stateSnapshot();
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

    ui::updateHeader(header_, system.wifiConnected, system.bleConnected,
                     system.printerConnected);
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
    lv_label_set_text(clockFormatButtonLabel_, settings.clock24Hour ? "24 HOUR" : "12 HOUR");
    lv_label_set_text(timeZoneButtonLabel_, timeZoneOptionLabel(settings.timeZone));
    lv_label_set_text_fmt(saverDelayLabel_, "%u min", static_cast<unsigned>(settings.screenSaverDelayMinutes));
    if (!lv_obj_has_state(saverDelaySlider_, LV_STATE_PRESSED))
        lv_slider_set_value(saverDelaySlider_, settings.screenSaverDelayMinutes, LV_ANIM_OFF);
    lv_label_set_text_fmt(clockBrightnessLabel_, "%u%%", static_cast<unsigned>(settings.clockBrightness));
    if (!lv_obj_has_state(clockBrightnessSlider_, LV_STATE_PRESSED))
        lv_slider_set_value(clockBrightnessSlider_, settings.clockBrightness, LV_ANIM_OFF);
    lv_label_set_text(quietTargetButtonLabel_, quietTargetName(settings.quietTarget));
    setQuietDurationText(quietDurationLabel_, settings.quietDurationMinutes);
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
    for (lv_obj_t* button : otaButtons_) {
        if (!button) continue;
        if (updateBusy) lv_obj_add_state(button, LV_STATE_DISABLED);
        else lv_obj_clear_state(button, LV_STATE_DISABLED);
    }
    lv_bar_set_value(otaProgressBar_, system.otaProgress, LV_ANIM_OFF);
    lv_label_set_text_fmt(otaProgressLabel_, "%u%%", static_cast<unsigned>(system.otaProgress));
    lv_label_set_text(otaButtonLabels_[0], system.otaState == OtaState::Checking ? "CHECKING" : "CHECK");
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

void SettingsScreen::showPortalPopup() {
    if (portalOverlay_) return;

    portalOverlay_ = lv_obj_create(root_);
    lv_obj_set_size(portalOverlay_, 480, 320);
    lv_obj_set_pos(portalOverlay_, 0, 0);
    lv_obj_clear_flag(portalOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(portalOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(portalOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(portalOverlay_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(portalOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(portalOverlay_, 0, LV_PART_MAIN);

    makeLabel(portalOverlay_, "WEB PORTAL", ui::ColorCyan,
              &lv_font_montserrat_12, 24, 18);
    makeLabel(portalOverlay_, "Open either address on this Wi-Fi network.", ui::ColorText,
              &lv_font_montserrat_14, 24, 45, 432);

    lv_obj_t* addressPanel = lv_obj_create(portalOverlay_);
    lv_obj_set_size(addressPanel, 432, 142);
    lv_obj_set_pos(addressPanel, 24, 78);
    stylePanel(addressPanel);

    char ipAddress[48] = "Wi-Fi not connected";
    if (WiFi.status() == WL_CONNECTED) {
        const String ip = WiFi.localIP().toString();
        snprintf(ipAddress, sizeof(ipAddress), "http://%s/", ip.c_str());
    }
    char localAddress[64] = "";
    snprintf(localAddress, sizeof(localAddress), "http://%s.local/",
             deviceIdentity().hostname());

    makeLabel(addressPanel, "IP ADDRESS", ui::ColorMuted,
              &lv_font_montserrat_10, 16, 15);
    makeLabel(addressPanel, ipAddress,
              WiFi.status() == WL_CONNECTED ? ui::ColorText : ui::ColorMuted,
              &lv_font_montserrat_18, 16, 35, 400);
    makeLabel(addressPanel, "LOCAL ADDRESS", ui::ColorMuted,
              &lv_font_montserrat_10, 16, 78);
    makeLabel(addressPanel, localAddress, ui::ColorText,
              &lv_font_montserrat_18, 16, 98, 400);

    lv_obj_t* closeLabel = nullptr;
    lv_obj_t* closeButton = makeActionButton(portalOverlay_, 24, 246, 432, &closeLabel);
    lv_obj_set_height(closeButton, 48);
    lv_obj_set_style_border_color(closeButton, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_obj_set_style_text_color(closeLabel, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_obj_set_style_text_font(closeLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_label_set_text(closeLabel, "CLOSE");
    actionBindings_[29] = {this, Action::PortalClose};
    lv_obj_add_event_cb(closeButton, actionEvent, LV_EVENT_CLICKED, &actionBindings_[29]);
}

void SettingsScreen::closePortalPopup() {
    if (!portalOverlay_) return;
    lv_obj_t* overlay = portalOverlay_;
    portalOverlay_ = nullptr;
    lv_obj_del_async(overlay);
    cacheValid_ = false;
}

void SettingsScreen::showTimeZonePicker() {
    if (timeZoneOverlay_) return;
    const int selected = timeZoneOptionIndex(settingsService().settings().timeZone);
    timeZonePage_ = selected >= 0 ? static_cast<uint8_t>(selected / TimeZonePageSize) : 0;

    timeZoneOverlay_ = lv_obj_create(root_);
    lv_obj_set_size(timeZoneOverlay_, 480, 320);
    lv_obj_set_pos(timeZoneOverlay_, 0, 0);
    lv_obj_clear_flag(timeZoneOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(timeZoneOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(timeZoneOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(timeZoneOverlay_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(timeZoneOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(timeZoneOverlay_, 0, LV_PART_MAIN);

    makeLabel(timeZoneOverlay_, "SELECT TIME ZONE", ui::ColorCyan,
              &lv_font_montserrat_12, 18, 14);
    timeZonePageLabel_ = makeLabel(timeZoneOverlay_, "", ui::ColorMuted,
                                   &lv_font_montserrat_10, 350, 17, 112);
    lv_obj_set_style_text_align(timeZonePageLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    for (uint8_t slot = 0; slot < 6; ++slot) {
        const int column = slot % 2;
        const int row = slot / 2;
        lv_obj_t* button = makeActionButton(timeZoneOverlay_, 18 + column * 222,
                                            45 + row * 64, 210, &timeZoneLabels_[slot]);
        lv_obj_set_height(button, 48);
        lv_obj_set_width(timeZoneLabels_[slot], 196);
        lv_obj_set_style_text_align(timeZoneLabels_[slot], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(timeZoneLabels_[slot]);
        timeZoneButtons_[slot] = button;
        timeZoneBindings_[slot] = {this, slot};
        lv_obj_add_event_cb(button, timeZoneEvent, LV_EVENT_CLICKED, &timeZoneBindings_[slot]);
    }

    lv_obj_t* label = nullptr;
    lv_obj_t* button = makeActionButton(timeZoneOverlay_, 18, 253, 130, &label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_label_set_text(label, LV_SYMBOL_LEFT " PREV");
    lv_obj_center(label);
    actionBindings_[25] = {this, Action::TimeZonePrevious};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[25]);

    button = makeActionButton(timeZoneOverlay_, 166, 253, 130, &label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_label_set_text(label, "NEXT " LV_SYMBOL_RIGHT);
    lv_obj_center(label);
    actionBindings_[26] = {this, Action::TimeZoneNext};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[26]);

    button = makeActionButton(timeZoneOverlay_, 314, 253, 148, &label);
    lv_obj_set_style_text_color(label, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_label_set_text(label, "CLOSE");
    lv_obj_center(label);
    actionBindings_[27] = {this, Action::TimeZoneClose};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[27]);
    refreshTimeZonePicker();
}

void SettingsScreen::refreshTimeZonePicker() {
    if (!timeZoneOverlay_) return;
    if (timeZonePage_ >= TimeZonePageCount) timeZonePage_ = TimeZonePageCount - 1;
    lv_label_set_text_fmt(timeZonePageLabel_, "%u / %u",
                          static_cast<unsigned>(timeZonePage_ + 1),
                          static_cast<unsigned>(TimeZonePageCount));
    const int selected = timeZoneOptionIndex(settingsService().settings().timeZone);
    for (uint8_t slot = 0; slot < TimeZonePageSize; ++slot) {
        const size_t index = static_cast<size_t>(timeZonePage_) * TimeZonePageSize + slot;
        if (index >= TimeZoneOptionCount) {
            lv_obj_add_flag(timeZoneButtons_[slot], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(timeZoneButtons_[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(timeZoneLabels_[slot], "%s\n%s",
                              TimeZoneOptions[index].offset, TimeZoneOptions[index].label);
        lv_obj_set_style_border_color(timeZoneButtons_[slot],
                                      lv_color_hex(static_cast<int>(index) == selected
                                                       ? ui::ColorCyan : ui::ColorBorder),
                                      LV_PART_MAIN);
    }
}

void SettingsScreen::closeTimeZonePicker() {
    if (!timeZoneOverlay_) return;
    lv_obj_t* overlay = timeZoneOverlay_;
    timeZoneOverlay_ = nullptr;
    timeZonePageLabel_ = nullptr;
    memset(timeZoneButtons_, 0, sizeof(timeZoneButtons_));
    memset(timeZoneLabels_, 0, sizeof(timeZoneLabels_));
    lv_obj_del_async(overlay);
    cacheValid_ = false;
}

void SettingsScreen::selectTimeZone(uint8_t slot) {
    const size_t index = static_cast<size_t>(timeZonePage_) * TimeZonePageSize + slot;
    if (index >= TimeZoneOptionCount) return;
    settingsService().update([index](AppSettings& settings) {
        strlcpy(settings.timeZone, TimeZoneOptions[index].spec, sizeof(settings.timeZone));
    });
    closeTimeZonePicker();
}

void SettingsScreen::showQuietDurationPicker() {
    if (quietDurationOverlay_) return;

    quietDurationOverlay_ = lv_obj_create(root_);
    lv_obj_set_size(quietDurationOverlay_, 480, 320);
    lv_obj_set_pos(quietDurationOverlay_, 0, 0);
    lv_obj_clear_flag(quietDurationOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(quietDurationOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(quietDurationOverlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(quietDurationOverlay_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(quietDurationOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(quietDurationOverlay_, 0, LV_PART_MAIN);

    makeLabel(quietDurationOverlay_, "QUIET MODE DURATION", ui::ColorCyan,
              &lv_font_montserrat_12, 18, 13);
    lv_obj_t* hoursLabel = makeLabel(quietDurationOverlay_, "HOURS", ui::ColorMuted,
                                     &lv_font_montserrat_10, 110, 39, 96);
    lv_obj_set_style_text_align(hoursLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t* minutesLabel = makeLabel(quietDurationOverlay_, "MINUTES", ui::ColorMuted,
                                       &lv_font_montserrat_10, 274, 39, 96);
    lv_obj_set_style_text_align(minutesLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    quietHoursRoller_ = lv_roller_create(quietDurationOverlay_);
    lv_obj_set_size(quietHoursRoller_, 104, 164);
    lv_obj_set_pos(quietHoursRoller_, 106, 56);
    lv_roller_set_options(quietHoursRoller_, QuietHourOptions, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(quietHoursRoller_, 5);
    styleTimeRoller(quietHoursRoller_);
    lv_obj_add_event_cb(quietHoursRoller_, quietDurationEvent, LV_EVENT_VALUE_CHANGED, this);

    quietMinutesRoller_ = lv_roller_create(quietDurationOverlay_);
    lv_obj_set_size(quietMinutesRoller_, 104, 164);
    lv_obj_set_pos(quietMinutesRoller_, 270, 56);
    lv_roller_set_options(quietMinutesRoller_, QuietMinuteOptions, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(quietMinutesRoller_, 5);
    styleTimeRoller(quietMinutesRoller_);
    lv_obj_add_event_cb(quietMinutesRoller_, quietDurationEvent, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* separator = makeLabel(quietDurationOverlay_, ":", ui::ColorText,
                                    &lv_font_montserrat_32, 210, 113, 60);
    lv_obj_set_style_text_align(separator, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    quietDurationSelectionLabel_ = makeLabel(quietDurationOverlay_, "01 : 00",
                                              ui::ColorText, &lv_font_montserrat_14,
                                              140, 228, 200);
    lv_obj_set_style_text_align(quietDurationSelectionLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    uint16_t selectedMinutes = settingsService().settings().quietDurationMinutes;
    if (selectedMinutes == 0) selectedMinutes = 60;
    if (selectedMinutes > 1440) selectedMinutes = 1440;
    lv_roller_set_selected(quietHoursRoller_, selectedMinutes / 60U, LV_ANIM_OFF);
    lv_roller_set_selected(quietMinutesRoller_, selectedMinutes % 60U, LV_ANIM_OFF);

    lv_obj_t* label = nullptr;
    lv_obj_t* button = makeActionButton(quietDurationOverlay_, 12, 256, 110, &label);
    lv_obj_set_height(button, 48);
    lv_label_set_text(label, "CANCEL");
    actionBindings_[30] = {this, Action::QuietDurationCancel};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[30]);

    button = makeActionButton(quietDurationOverlay_, 130, 256, 148, &label);
    lv_obj_set_height(button, 48);
    lv_obj_set_style_text_color(label, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_label_set_text(label, "UNLIMITED");
    actionBindings_[31] = {this, Action::QuietDurationUnlimited};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[31]);

    button = makeActionButton(quietDurationOverlay_, 286, 256, 182, &label);
    lv_obj_set_height(button, 48);
    lv_obj_set_style_border_color(button, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
    lv_label_set_text(label, "CONFIRM");
    actionBindings_[32] = {this, Action::QuietDurationConfirm};
    lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED, &actionBindings_[32]);

    refreshQuietDurationPicker();
}

void SettingsScreen::refreshQuietDurationPicker() {
    if (!quietDurationOverlay_ || !quietHoursRoller_ || !quietMinutesRoller_) return;
    const uint16_t hours = lv_roller_get_selected(quietHoursRoller_);
    if (hours >= 24U && lv_roller_get_selected(quietMinutesRoller_) != 0) {
        lv_roller_set_selected(quietMinutesRoller_, 0, LV_ANIM_OFF);
    }
    const uint16_t minutes = hours >= 24U ? 0U : lv_roller_get_selected(quietMinutesRoller_);
    const uint16_t totalMinutes = static_cast<uint16_t>(hours * 60U + minutes);
    setQuietDurationText(quietDurationSelectionLabel_, totalMinutes);
}

void SettingsScreen::closeQuietDurationPicker() {
    if (!quietDurationOverlay_) return;
    lv_obj_t* overlay = quietDurationOverlay_;
    quietDurationOverlay_ = nullptr;
    quietHoursRoller_ = nullptr;
    quietMinutesRoller_ = nullptr;
    quietDurationSelectionLabel_ = nullptr;
    lv_obj_del_async(overlay);
    cacheValid_ = false;
}

void SettingsScreen::confirmQuietDuration() {
    if (!quietHoursRoller_ || !quietMinutesRoller_) return;
    const uint16_t hours = lv_roller_get_selected(quietHoursRoller_);
    const uint16_t minutes = hours >= 24U ? 0U : lv_roller_get_selected(quietMinutesRoller_);
    uint16_t totalMinutes = static_cast<uint16_t>(hours * 60U + minutes);
    if (totalMinutes == 0) totalMinutes = 1;
    settingsService().update([totalMinutes](AppSettings& settings) {
        settings.quietDurationMinutes = totalMinutes;
    });
    closeQuietDurationPicker();
}

void SettingsScreen::bindSlider(lv_obj_t* slider, uint8_t bindingIndex, Action action) {
    ActionBinding& binding = actionBindings_[bindingIndex];
    binding = {};
    binding.owner = this;
    binding.action = action;
    binding.guardedSlider = true;
    ui::enableVerticalScrollFromSlider(slider);
    lv_obj_add_event_cb(slider, actionEvent, LV_EVENT_PRESSED, &binding);
    lv_obj_add_event_cb(slider, actionEvent, LV_EVENT_PRESSING, &binding);
    lv_obj_add_event_cb(slider, actionEvent, LV_EVENT_VALUE_CHANGED, &binding);
    lv_obj_add_event_cb(slider, actionEvent, LV_EVENT_RELEASED, &binding);
    lv_obj_add_event_cb(slider, actionEvent, LV_EVENT_PRESS_LOST, &binding);
}

void SettingsScreen::previewSlider(Action action, lv_obj_t* slider) {
    const int value = lv_slider_get_value(slider);
    switch (action) {
        case Action::Brightness:
            lv_label_set_text_fmt(brightnessLabel_, "DISPLAY %d%%", value);
            break;
        case Action::AccentHue:
            lv_label_set_text_fmt(accentLabel_, "%d deg", value);
            break;
        case Action::SaverDelay:
            lv_label_set_text_fmt(saverDelayLabel_, "%d min", value);
            break;
        case Action::ClockBrightness:
            lv_label_set_text_fmt(clockBrightnessLabel_, "%d%%", value);
            break;
        default:
            break;
    }
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
    switch (action) {
        case Action::TransportAuto:
            settingsService().update([](AppSettings& settings) {
                settings.companionTransport = CompanionTransport::Auto;
                settings.bleEnabled = true;
            });
            update();
            break;
        case Action::TransportBle:
            settingsService().update([](AppSettings& settings) {
                settings.companionTransport = CompanionTransport::Ble;
                settings.bleEnabled = true;
            });
            update();
            break;
        case Action::TransportWifi:
            settingsService().update([](AppSettings& settings) {
                settings.companionTransport = CompanionTransport::Wifi;
                settings.bleEnabled = true;
            });
            update();
            break;
        case Action::Brightness: {
            const uint8_t value = static_cast<uint8_t>(lv_slider_get_value(brightnessSlider_));
            const lv_event_code_t code = lv_event_get_code(event);
            const bool persist = code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST;
            settingsService().update([value](AppSettings& settings) {
                settings.displayBrightness = value;
            }, persist);
            lv_label_set_text_fmt(brightnessLabel_, "DISPLAY %u%%",
                                  static_cast<unsigned>(value));
            break;
        }
        case Action::Reconfigure:
            if (setupCallback_) setupCallback_(callbackContext_);
            break;
        case Action::SkinNext:
            settingsService().update([](AppSettings& settings) {
                settings.uiSkin = static_cast<UiSkin>(
                    (static_cast<uint8_t>(settings.uiSkin) + 1U) % 4U);
            });
            break;
        case Action::ColorModeNext:
            settingsService().update([](AppSettings& settings) {
                settings.uiColorMode = static_cast<UiColorMode>(
                    (static_cast<uint8_t>(settings.uiColorMode) + 1U) % 3U);
            });
            break;
        case Action::AccentHue: {
            const uint16_t value = static_cast<uint16_t>(lv_slider_get_value(accentSlider_));
            settingsService().update([value](AppSettings& settings) {
                settings.accentHueDegrees = value;
            }, lv_event_get_code(event) == LV_EVENT_RELEASED);
            lv_label_set_text_fmt(accentLabel_, "%u deg", static_cast<unsigned>(value));
            break;
        }
        case Action::SaverModeNext:
            settingsService().update([](AppSettings& settings) {
                settings.screenSaverMode = static_cast<ScreenSaverMode>(
                    (static_cast<uint8_t>(settings.screenSaverMode) + 1U) % 3U);
            });
            break;
        case Action::ClockStyleNext:
            settingsService().update([](AppSettings& settings) {
                settings.clockStyle = static_cast<ClockStyle>(
                    (static_cast<uint8_t>(settings.clockStyle) + 1U) %
                    static_cast<uint8_t>(ClockStyle::Count));
            });
            break;
        case Action::ClockFormatNext:
            settingsService().update([](AppSettings& settings) {
                settings.clock24Hour = !settings.clock24Hour;
            });
            break;
        case Action::TimeZoneOpen:
            showTimeZonePicker();
            break;
        case Action::TimeZonePrevious:
            timeZonePage_ = timeZonePage_ == 0 ? TimeZonePageCount - 1 : timeZonePage_ - 1;
            refreshTimeZonePicker();
            break;
        case Action::TimeZoneNext:
            timeZonePage_ = static_cast<uint8_t>((timeZonePage_ + 1) % TimeZonePageCount);
            refreshTimeZonePicker();
            break;
        case Action::TimeZoneClose:
            closeTimeZonePicker();
            break;
        case Action::SaverDelay: {
            const uint8_t value = static_cast<uint8_t>(lv_slider_get_value(saverDelaySlider_));
            settingsService().update([value](AppSettings& settings) {
                settings.screenSaverDelayMinutes = value;
            }, lv_event_get_code(event) == LV_EVENT_RELEASED);
            lv_label_set_text_fmt(saverDelayLabel_, "%u min", static_cast<unsigned>(value));
            break;
        }
        case Action::ClockBrightness: {
            const uint8_t value = static_cast<uint8_t>(lv_slider_get_value(clockBrightnessSlider_));
            settingsService().update([value](AppSettings& settings) {
                settings.clockBrightness = value;
            }, lv_event_get_code(event) == LV_EVENT_RELEASED);
            lv_label_set_text_fmt(clockBrightnessLabel_, "%u%%", static_cast<unsigned>(value));
            break;
        }
        case Action::QuietTargetNext:
            settingsService().update([](AppSettings& settings) {
                settings.quietTarget = static_cast<QuietTarget>(
                    (static_cast<uint8_t>(settings.quietTarget) + 1U) % 4U);
            });
            break;
        case Action::QuietDurationOpen:
            showQuietDurationPicker();
            break;
        case Action::QuietDurationCancel:
            closeQuietDurationPicker();
            break;
        case Action::QuietDurationUnlimited:
            settingsService().update([](AppSettings& settings) {
                settings.quietDurationMinutes = 0;
            });
            closeQuietDurationPicker();
            break;
        case Action::QuietDurationConfirm:
            confirmQuietDuration();
            break;
        case Action::QuietErrorsBypass:
            settingsService().update([](AppSettings& settings) {
                settings.quietErrorsBypass = !settings.quietErrorsBypass;
            });
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
        case Action::PortalOpen:
            showPortalPopup();
            break;
        case Action::PortalClose:
            closePortalPopup();
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
    if (binding->guardedSlider) {
        lv_obj_t* slider = lv_event_get_target(event);
        const ui::SliderGestureResult result = ui::processSliderGesture(
            event, slider, binding->sliderGesture);
        if (result == ui::SliderGestureResult::Preview) {
            binding->owner->previewSlider(binding->action, slider);
        } else if (result == ui::SliderGestureResult::Commit) {
            binding->owner->handleAction(binding->action, event);
        } else if (result == ui::SliderGestureResult::Cancel) {
            binding->owner->update();
        }
        return;
    }
    binding->owner->handleAction(binding->action, event);
}

void SettingsScreen::scrollEvent(lv_event_t* event) {
    SettingsScreen* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    lv_obj_t* content = lv_event_get_target(event);
    if (!screen || !content) return;
    const int32_t scrollY = lv_obj_get_scroll_y(content);
    screen->scrollY_ = scrollY > 0 ? scrollY : 0;
}

void SettingsScreen::timeZoneEvent(lv_event_t* event) {
    TimeZoneBinding* binding = static_cast<TimeZoneBinding*>(lv_event_get_user_data(event));
    if (!binding || !binding->owner) return;
    binding->owner->selectTimeZone(binding->slot);
}

void SettingsScreen::quietDurationEvent(lv_event_t* event) {
    SettingsScreen* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    if (screen) screen->refreshQuietDurationPicker();
}

}
