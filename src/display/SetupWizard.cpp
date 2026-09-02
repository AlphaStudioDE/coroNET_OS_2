#include "SetupWizard.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../core/DeviceIdentity.h"
#include "../core/SystemState.h"
#include "../printer/PrinterService.h"
#include "../settings/SettingsService.h"
#include "../wifi/WifiService.h"
#include "UiTheme.h"

namespace coronet {

namespace {

constexpr uint8_t kStepCount = 6;
constexpr lv_coord_t kScreenWidth = ui::ScreenWidth;
constexpr lv_coord_t kScreenHeight = ui::ScreenHeight;

const ui::ThemeColor& ColorBackground = ui::ColorBackground;
const ui::ThemeColor& ColorSurface = ui::ColorSurface;
const ui::ThemeColor& ColorSurfaceRaised = ui::ColorSurfaceRaised;
const ui::ThemeColor& ColorBorder = ui::ColorBorder;
const ui::ThemeColor& ColorText = ui::ColorText;
const ui::ThemeColor& ColorMuted = ui::ColorMuted;
const ui::ThemeColor& ColorCyan = ui::ColorCyan;
const ui::ThemeColor& ColorCyanDark = ui::ColorCyanDark;
const ui::ThemeColor& ColorAmber = ui::ColorAmber;
const ui::ThemeColor& ColorRed = ui::ColorRed;

SetupWizard* gActiveSetupWizard = nullptr;

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

void clearContainerStyle(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(object, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    uint32_t color,
                    const lv_font_t* font,
                    lv_coord_t x,
                    lv_coord_t y,
                    lv_coord_t width = LV_SIZE_CONTENT) {
    lv_obj_t* label = lv_label_create(parent);
    styleText(label, color, font);
    if (width != LV_SIZE_CONTENT) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

void styleButton(lv_obj_t* button, bool primary) {
    lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button,
                                  lv_color_hex(primary ? ColorCyan : ColorBorder),
                                  LV_PART_MAIN);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(primary ? ColorCyan : ColorSurface),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(primary ? 0x1AB7A9 : ColorSurfaceRaised),
                              static_cast<lv_style_selector_t>(
                                  static_cast<uint32_t>(LV_PART_MAIN) |
                                  static_cast<uint32_t>(LV_STATE_PRESSED)));
}

void styleInput(lv_obj_t* field) {
    lv_obj_set_style_radius(field, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(field, lv_color_hex(ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, lv_color_hex(ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_border_color(field,
                                  lv_color_hex(ColorCyan),
                                  static_cast<lv_style_selector_t>(
                                      static_cast<uint32_t>(LV_PART_MAIN) |
                                      static_cast<uint32_t>(LV_STATE_FOCUSED)));
    lv_obj_set_style_text_color(field, lv_color_hex(ColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(field, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(field, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(field, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(field, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_top(field, 9, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(field, 7, LV_PART_MAIN);
}

void markTouch() {
    SystemState& system = state();
    system.touchCount++;
    system.lastTouchMs = millis();
}

void rootTouchEvent(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) markTouch();
}

const char* transportName(CompanionTransport transport) {
    switch (transport) {
        case CompanionTransport::Ble: return "Bluetooth LE";
        case CompanionTransport::Wifi: return "Wi-Fi";
        case CompanionTransport::Auto:
        default: return "Automatic";
    }
}

const char* signalName(int32_t rssi) {
    if (rssi >= -55) return "Excellent";
    if (rssi >= -67) return "Good";
    if (rssi >= -75) return "Fair";
    return "Weak";
}

void normalizePrinterHost(const char* input, char* output, size_t outputSize, uint16_t& port) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    String host(input ? input : "");
    host.trim();
    if (host.startsWith("http://")) host.remove(0, 7);
    if (host.startsWith("https://")) host.remove(0, 8);
    const int slash = host.indexOf('/');
    if (slash >= 0) host.remove(slash);
    const int colon = host.lastIndexOf(':');
    if (colon > 0) {
        const int parsedPort = host.substring(colon + 1).toInt();
        if (parsedPort > 0 && parsedPort <= 65535) {
            port = static_cast<uint16_t>(parsedPort);
            host.remove(colon);
        }
    }
    host.trim();
    host.toCharArray(output, outputSize);
}

}

void SetupWizard::begin(bool animate) {
    reset();
    gActiveSetupWizard = this;
    step_ = Step::Welcome;
    finished_ = false;
    buildRoot();
    renderStep();
    lv_scr_load_anim(root_, animate ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_NONE,
                     animate ? 550 : 0, 0, true);
}

void SetupWizard::loop() {
    if (!root_ || finished_) return;
    if (settingsService().settings().setupDone) {
        finished_ = true;
        return;
    }
    if (step_ == Step::Network && !networkCredentialsView_ &&
        wifiScanRevisionSeen_ != wifiService().scanRevision()) {
        wifiScanRevisionSeen_ = wifiService().scanRevision();
        renderPending_ = true;
    }
    if (step_ == Step::Network && networkCredentialsView_ &&
        wifiConnectionRevisionSeen_ != wifiService().connectionRevision()) {
        wifiConnectionRevisionSeen_ = wifiService().connectionRevision();
        if (wifiService().connectionStatus() == WifiConnectStatus::Connected) {
            networkConnectionVerified_ = true;
        }
        renderPending_ = true;
    }
    if (step_ == Step::Printer && !printerDetailsView_) {
        PrinterDiscoverySnapshot discovery;
        printerService().discoverySnapshot(discovery);
        if (printerDiscoveryRevisionSeen_ != discovery.revision) {
            printerDiscoveryRevisionSeen_ = discovery.revision;
            renderPending_ = true;
        }
    }
    if (renderPending_) {
        renderPending_ = false;
        renderStep();
    }
}

void SetupWizard::reset() {
    if (gActiveSetupWizard == this) gActiveSetupWizard = nullptr;
    root_ = nullptr;
    content_ = nullptr;
    stepLabel_ = nullptr;
    backButton_ = nullptr;
    nextButton_ = nullptr;
    nextButtonLabel_ = nullptr;
    keyboard_ = nullptr;
    nameField_ = nullptr;
    ssidField_ = nullptr;
    passwordField_ = nullptr;
    printerHostField_ = nullptr;
    printerPortField_ = nullptr;
    networkCredentialsView_ = false;
    selectedNetworkSecured_ = true;
    networkConnectionVerified_ = false;
    printerDetailsView_ = false;
    wifiScanRevisionSeen_ = 0;
    wifiConnectionRevisionSeen_ = 0;
    printerDiscoveryRevisionSeen_ = 0;
    selectedSsid_[0] = '\0';
    networkPassword_[0] = '\0';
    selectedPrinterName_[0] = '\0';
    selectedPrinterHost_[0] = '\0';
    selectedPrinterPort_ = 7125;
    for (lv_obj_t*& segment : progress_) segment = nullptr;
    renderPending_ = false;
}

void SetupWizard::buildRoot() {
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(root_, rootTouchEvent, LV_EVENT_PRESSED, nullptr);

    makeLabel(root_, "coroNET", ColorText, &lv_font_montserrat_26, 24, 15);
    makeLabel(root_, "FIRST SETUP", ColorCyan, &lv_font_montserrat_10, 145, 27);

    stepLabel_ = makeLabel(root_, "1 OF 6", ColorMuted, &lv_font_montserrat_12, 405, 23, 52);
    lv_obj_set_style_text_align(stepLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    for (uint8_t index = 0; index < kStepCount; ++index) {
        progress_[index] = lv_obj_create(root_);
        lv_obj_set_size(progress_[index], 66, 3);
        lv_obj_set_pos(progress_[index], 24 + index * 73, 52);
        lv_obj_set_style_radius(progress_[index], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(progress_[index], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(progress_[index], 0, LV_PART_MAIN);
    }

    content_ = lv_obj_create(root_);
    lv_obj_set_size(content_, 432, 190);
    lv_obj_set_pos(content_, 24, 66);
    clearContainerStyle(content_);

    backButton_ = lv_btn_create(root_);
    lv_obj_set_size(backButton_, 104, 40);
    lv_obj_set_pos(backButton_, 24, 270);
    styleButton(backButton_, false);
    lv_obj_add_event_cb(backButton_, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::Back)));
    lv_obj_t* backLabel = lv_label_create(backButton_);
    styleText(backLabel, ColorText, &lv_font_montserrat_14);
    lv_label_set_text(backLabel, "Back");
    lv_obj_center(backLabel);

    nextButton_ = lv_btn_create(root_);
    lv_obj_set_size(nextButton_, 122, 40);
    lv_obj_set_pos(nextButton_, 334, 270);
    styleButton(nextButton_, true);
    lv_obj_add_event_cb(nextButton_, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::Next)));
    nextButtonLabel_ = lv_label_create(nextButton_);
    styleText(nextButtonLabel_, ColorBackground, &lv_font_montserrat_14);
    lv_label_set_text(nextButtonLabel_, "Start");
    lv_obj_center(nextButtonLabel_);

    makeLabel(root_, "Your settings stay on this device.", ColorMuted,
              &lv_font_montserrat_10, 148, 285, 170);
}

void SetupWizard::renderStep() {
    if (!content_) return;
    hideKeyboard();
    lv_obj_clean(content_);
    nameField_ = nullptr;
    ssidField_ = nullptr;
    passwordField_ = nullptr;
    printerHostField_ = nullptr;
    printerPortField_ = nullptr;

    switch (step_) {
        case Step::Welcome: renderWelcome(); break;
        case Step::Identity: renderIdentity(); break;
        case Step::Connection: renderConnection(); break;
        case Step::Network: renderNetwork(); break;
        case Step::Printer: renderPrinter(); break;
        case Step::Ready: renderReady(); break;
        case Step::Count: break;
    }
    updateProgress();
    updateNavigation();
}

void SetupWizard::renderWelcome() {
    makeLabel(content_, "Welcome to coroNET", ColorText, &lv_font_montserrat_30, 0, 4);
    makeLabel(content_,
              "Let us shape this device around your printer and the way you work.",
              ColorMuted, &lv_font_montserrat_16, 0, 48, 420);

    lv_obj_t* accent = lv_obj_create(content_);
    lv_obj_set_size(accent, 4, 72);
    lv_obj_set_pos(accent, 0, 104);
    lv_obj_set_style_radius(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(accent, lv_color_hex(ColorAmber), LV_PART_MAIN);

    makeLabel(content_, "DISPLAY", ColorCyan, &lv_font_montserrat_10, 18, 106);
    makeLabel(content_, "Clear status at a glance", ColorText, &lv_font_montserrat_14, 18, 126);
    makeLabel(content_, "COMPANION", ColorCyan, &lv_font_montserrat_10, 218, 106);
    makeLabel(content_, "Control from your phone", ColorText, &lv_font_montserrat_14, 218, 126);
    makeLabel(content_, "PRIVATE", ColorCyan, &lv_font_montserrat_10, 18, 154);
    makeLabel(content_, "Local-first by design", ColorText, &lv_font_montserrat_14, 18, 173);
}

void SetupWizard::renderIdentity() {
    makeLabel(content_, "Make it yours", ColorText, &lv_font_montserrat_26, 0, 2);
    makeLabel(content_, "This name identifies the device in Bluetooth and the companion app.",
              ColorMuted, &lv_font_montserrat_14, 0, 38, 420);
    makeLabel(content_, "DEVICE NAME", ColorCyan, &lv_font_montserrat_10, 0, 92);

    char effectiveName[25] = "";
    const AppSettings& settings = settingsService().settings();
    deviceIdentity().effectiveName(settings.deviceName, effectiveName, sizeof(effectiveName));

    nameField_ = lv_textarea_create(content_);
    lv_obj_set_size(nameField_, 310, 44);
    lv_obj_set_pos(nameField_, 0, 112);
    styleInput(nameField_);
    lv_textarea_set_one_line(nameField_, true);
    lv_textarea_set_max_length(nameField_, 24);
    lv_textarea_set_text(nameField_, effectiveName);
    lv_obj_add_event_cb(nameField_, fieldEvent, LV_EVENT_FOCUSED, this);

    makeLabel(content_, deviceIdentity().id(), ColorMuted, &lv_font_montserrat_10, 326, 128, 100);
}

void SetupWizard::renderConnection() {
    makeLabel(content_, "Connect the companion app", ColorText, &lv_font_montserrat_26, 0, 0);
    makeLabel(content_,
              "Choose how the coroNET mobile app communicates with this device.",
              ColorMuted, &lv_font_montserrat_12, 0, 34, 420);
    makeLabel(content_, "Printer connection is configured in a separate step.",
              ColorAmber, &lv_font_montserrat_10, 0, 59, 420);

    const CompanionTransport selected = settingsService().settings().companionTransport;
    struct Choice {
        const char* title;
        const char* detail;
        CompanionTransport transport;
        Action action;
    };
    static constexpr Choice choices[] = {
        {"AUTO", "Wi-Fi + BLE fallback", CompanionTransport::Auto, Action::TransportAuto},
        {"BLE", "App connects directly", CompanionTransport::Ble, Action::TransportBle},
        {"WI-FI", "App uses local network", CompanionTransport::Wifi, Action::TransportWifi},
    };

    for (uint8_t index = 0; index < 3; ++index) {
        const bool active = selected == choices[index].transport;
        lv_obj_t* button = lv_btn_create(content_);
        lv_obj_set_size(button, 136, 84);
        lv_obj_set_pos(button, index * 148, 84);
        styleButton(button, active);
        lv_obj_set_style_border_color(button,
                                      lv_color_hex(active ? ColorCyan : ColorBorder),
                                      LV_PART_MAIN);
        lv_obj_add_event_cb(button, actionEvent, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(choices[index].action)));
        lv_obj_t* title = makeLabel(button, choices[index].title,
                                    active ? ColorBackground : ColorText,
                                    &lv_font_montserrat_18, 12, 12, 110);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t* detail = makeLabel(button, choices[index].detail,
                                     active ? ColorCyanDark : ColorMuted,
                                     &lv_font_montserrat_10, 8, 44, 118);
        lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

void SetupWizard::renderNetwork() {
    if (networkCredentialsView_) renderNetworkCredentials();
    else renderNetworkDiscovery();
}

void SetupWizard::renderNetworkDiscovery() {
    makeLabel(content_, "Choose a Wi-Fi network", ColorText, &lv_font_montserrat_24, 0, 0);
    makeLabel(content_, "Used by coroNET for local app control and printer communication.",
              ColorMuted, &lv_font_montserrat_10, 0, 31, 420);

    WifiService& wifi = wifiService();
    if (wifi.scanStatus() == WifiScanStatus::Idle) wifi.requestScan();
    wifiScanRevisionSeen_ = wifi.scanRevision();

    const char* statusText = "Select a nearby network";
    if (wifi.scanStatus() == WifiScanStatus::Scanning) statusText = "Scanning for networks...";
    else if (wifi.scanStatus() == WifiScanStatus::Failed) statusText = "Scan failed. Try again or enter it manually.";
    else if (wifi.scanStatus() == WifiScanStatus::Complete && wifi.scanCount() == 0) {
        statusText = "No networks found. Try again or enter it manually.";
    }
    makeLabel(content_, statusText, ColorCyan, &lv_font_montserrat_10, 0, 54, 295);

    lv_obj_t* refresh = lv_btn_create(content_);
    lv_obj_set_size(refresh, 66, 24);
    lv_obj_set_pos(refresh, 366, 47);
    styleButton(refresh, false);
    lv_obj_add_event_cb(refresh, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::NetworkRefresh)));
    lv_obj_t* refreshLabel = lv_label_create(refresh);
    styleText(refreshLabel, ColorText, &lv_font_montserrat_10);
    lv_label_set_text(refreshLabel, "Refresh");
    lv_obj_center(refreshLabel);

    lv_obj_t* list = lv_obj_create(content_);
    lv_obj_set_size(list, 432, 111);
    lv_obj_set_pos(list, 0, 76);
    lv_obj_set_style_radius(list, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, lv_color_hex(ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    if (wifi.scanStatus() == WifiScanStatus::Complete) {
        for (uint8_t index = 0; index < wifi.scanCount(); ++index) {
            const WifiNetworkInfo* network = wifi.network(index);
            if (!network) continue;
            lv_obj_t* row = lv_btn_create(list);
            lv_obj_set_width(row, lv_pct(100));
            lv_obj_set_height(row, 42);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            styleButton(row, false);
            lv_obj_set_flex_grow(row, 0);
            lv_obj_add_event_cb(
                row, actionEvent, LV_EVENT_CLICKED,
                reinterpret_cast<void*>(static_cast<uintptr_t>(Action::NetworkBase) + index));

            makeLabel(row, network->ssid, ColorText, &lv_font_montserrat_14, 10, 5, 245);
            char detail[48] = "";
            snprintf(detail, sizeof(detail), "%s  |  %s",
                     signalName(network->rssi), network->secured ? "Secured" : "Open");
            lv_obj_t* detailLabel = makeLabel(row, detail, ColorMuted,
                                              &lv_font_montserrat_10, 260, 13, 145);
            lv_obj_set_style_text_align(detailLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        }
    }

    lv_obj_t* manual = lv_btn_create(list);
    lv_obj_set_width(manual, lv_pct(100));
    lv_obj_set_height(manual, 36);
    lv_obj_clear_flag(manual, LV_OBJ_FLAG_SCROLLABLE);
    styleButton(manual, false);
    lv_obj_set_flex_grow(manual, 0);
    lv_obj_add_event_cb(manual, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::NetworkManual)));
    lv_obj_t* manualLabel = lv_label_create(manual);
    styleText(manualLabel, ColorCyan, &lv_font_montserrat_12);
    lv_label_set_text(manualLabel, "Enter network manually");
    lv_obj_center(manualLabel);
}

void SetupWizard::renderNetworkCredentials() {
    makeLabel(content_, selectedSsid_[0] ? "Connect to network" : "Enter Wi-Fi network",
              ColorText, &lv_font_montserrat_24, 0, 0);

    lv_obj_t* change = lv_btn_create(content_);
    lv_obj_set_size(change, 78, 28);
    lv_obj_set_pos(change, 354, 1);
    styleButton(change, false);
    lv_obj_add_event_cb(change, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::NetworkChange)));
    lv_obj_t* changeLabel = lv_label_create(change);
    styleText(changeLabel, ColorText, &lv_font_montserrat_10);
    lv_label_set_text(changeLabel, "Change");
    lv_obj_center(changeLabel);

    const WifiConnectStatus connection = wifiService().connectionStatus();
    const char* connectionText = "Enter the credentials, then test the connection.";
    uint32_t connectionColor = ColorMuted;
    char connectedText[80] = "";
    if (networkConnectionVerified_ || connection == WifiConnectStatus::Connected) {
        char ip[16] = "";
        wifiService().connectionIp(ip, sizeof(ip));
        snprintf(connectedText, sizeof(connectedText), "Connected successfully%s%s",
                 ip[0] ? "  |  " : "", ip);
        connectionText = connectedText;
        connectionColor = ColorCyan;
    } else if (connection == WifiConnectStatus::Connecting) {
        connectionText = "Connecting and validating the network...";
        connectionColor = ColorAmber;
    } else if (connection == WifiConnectStatus::NoNetwork) {
        connectionText = "Network not found. Move closer or refresh the list.";
        connectionColor = ColorRed;
    } else if (connection == WifiConnectStatus::AuthenticationFailed) {
        connectionText = "Connection rejected. Check the Wi-Fi password.";
        connectionColor = ColorRed;
    } else if (connection == WifiConnectStatus::Timeout) {
        connectionText = "Connection timed out. Check the password and signal.";
        connectionColor = ColorRed;
    } else if (connection == WifiConnectStatus::Failed) {
        connectionText = "Could not connect to this network.";
        connectionColor = ColorRed;
    }
    makeLabel(content_, connectionText, connectionColor, &lv_font_montserrat_10, 0, 39, 340);

    const AppSettings& settings = settingsService().settings();
    makeLabel(content_, "NETWORK NAME", ColorCyan, &lv_font_montserrat_10, 0, 70);
    ssidField_ = lv_textarea_create(content_);
    lv_obj_set_size(ssidField_, 206, 42);
    lv_obj_set_pos(ssidField_, 0, 88);
    styleInput(ssidField_);
    lv_textarea_set_one_line(ssidField_, true);
    lv_textarea_set_max_length(ssidField_, 32);
    lv_textarea_set_placeholder_text(ssidField_, "Wi-Fi SSID");
    lv_textarea_set_text(ssidField_, selectedSsid_[0] ? selectedSsid_ : settings.wifiSsid);
    if (selectedSsid_[0] || connection == WifiConnectStatus::Connecting || networkConnectionVerified_) {
        lv_obj_add_state(ssidField_, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(ssidField_, fieldEvent, LV_EVENT_FOCUSED, this);

    makeLabel(content_, selectedNetworkSecured_ ? "PASSWORD" : "OPEN NETWORK",
              ColorCyan, &lv_font_montserrat_10, 224, 70);
    passwordField_ = lv_textarea_create(content_);
    lv_obj_set_size(passwordField_, 208, 42);
    lv_obj_set_pos(passwordField_, 224, 88);
    styleInput(passwordField_);
    lv_textarea_set_one_line(passwordField_, true);
    lv_textarea_set_max_length(passwordField_, 64);
    lv_textarea_set_password_mode(passwordField_, true);
    lv_textarea_set_password_show_time(passwordField_, 800);
    lv_textarea_set_placeholder_text(passwordField_,
                                     selectedNetworkSecured_ ? "Wi-Fi password" : "No password needed");
    const bool selectedSavedNetwork = selectedSsid_[0] &&
                                      strncmp(selectedSsid_, settings.wifiSsid,
                                              sizeof(selectedSsid_)) == 0;
    const char* passwordText = networkPassword_[0]
                                   ? networkPassword_
                                   : (selectedSavedNetwork ? settings.wifiPassword : "");
    lv_textarea_set_text(passwordField_, selectedNetworkSecured_ ? passwordText : "");
    if (!selectedNetworkSecured_ || connection == WifiConnectStatus::Connecting ||
        networkConnectionVerified_) {
        lv_obj_add_state(passwordField_, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(passwordField_, fieldEvent, LV_EVENT_FOCUSED, this);

    makeLabel(content_, selectedNetworkSecured_
                            ? "Credentials stay on this device and are never sent to the cloud."
                            : "This network does not require a password.",
              ColorMuted, &lv_font_montserrat_10, 0, 149, 420);
}

void SetupWizard::renderPrinter() {
    if (printerDetailsView_) renderPrinterDetails();
    else renderPrinterDiscovery();
}

void SetupWizard::renderPrinterDiscovery() {
    makeLabel(content_, "Find your printer", ColorText, &lv_font_montserrat_24, 0, 0);
    makeLabel(content_, "coroNET searches this network for Snapmaker and Moonraker devices.",
              ColorMuted, &lv_font_montserrat_10, 0, 31, 420);

    PrinterDiscoverySnapshot discovery;
    printerService().discoverySnapshot(discovery);
    if (discovery.status == PrinterDiscoveryStatus::Idle && state().wifiConnected) {
        printerService().requestDiscovery();
        printerService().discoverySnapshot(discovery);
    }
    printerDiscoveryRevisionSeen_ = discovery.revision;

    const char* statusText = discovery.message[0] ? discovery.message : "Ready to search";
    uint32_t statusColor = ColorCyan;
    if (!state().wifiConnected) {
        statusText = "Wi-Fi is not connected. Go back or enter the printer manually.";
        statusColor = ColorRed;
    } else if (discovery.status == PrinterDiscoveryStatus::Scanning) {
        statusColor = ColorAmber;
    } else if (discovery.status == PrinterDiscoveryStatus::Failed) {
        statusColor = ColorRed;
    }
    makeLabel(content_, statusText, statusColor, &lv_font_montserrat_10, 0, 54, 330);

    lv_obj_t* refresh = lv_btn_create(content_);
    lv_obj_set_size(refresh, 66, 24);
    lv_obj_set_pos(refresh, 366, 47);
    styleButton(refresh, false);
    if (!state().wifiConnected || discovery.status == PrinterDiscoveryStatus::Scanning) {
        lv_obj_add_state(refresh, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(refresh, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::PrinterRefresh)));
    lv_obj_t* refreshLabel = lv_label_create(refresh);
    styleText(refreshLabel, ColorText, &lv_font_montserrat_10);
    lv_label_set_text(refreshLabel, "Refresh");
    lv_obj_center(refreshLabel);

    lv_obj_t* list = lv_obj_create(content_);
    lv_obj_set_size(list, 432, 111);
    lv_obj_set_pos(list, 0, 76);
    lv_obj_set_style_radius(list, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, lv_color_hex(ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    for (uint8_t index = 0; index < discovery.count; ++index) {
        DiscoveredPrinter printer;
        if (!printerService().discoveredPrinter(index, printer)) continue;
        lv_obj_t* row = lv_btn_create(list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 42);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        styleButton(row, false);
        lv_obj_set_flex_grow(row, 0);
        lv_obj_add_event_cb(
            row, actionEvent, LV_EVENT_CLICKED,
            reinterpret_cast<void*>(static_cast<uintptr_t>(Action::PrinterBase) + index));
        makeLabel(row, printer.name, ColorText, &lv_font_montserrat_14, 10, 5, 250);
        lv_obj_t* hostLabel = makeLabel(row, printer.host, ColorMuted,
                                        &lv_font_montserrat_10, 270, 13, 135);
        lv_obj_set_style_text_align(hostLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }

    lv_obj_t* manual = lv_btn_create(list);
    lv_obj_set_width(manual, lv_pct(100));
    lv_obj_set_height(manual, 36);
    lv_obj_clear_flag(manual, LV_OBJ_FLAG_SCROLLABLE);
    styleButton(manual, false);
    lv_obj_set_flex_grow(manual, 0);
    lv_obj_add_event_cb(manual, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::PrinterManual)));
    lv_obj_t* manualLabel = lv_label_create(manual);
    styleText(manualLabel, ColorCyan, &lv_font_montserrat_12);
    lv_label_set_text(manualLabel, "Enter printer manually");
    lv_obj_center(manualLabel);
}

void SetupWizard::renderPrinterDetails() {
    makeLabel(content_, selectedPrinterHost_[0] ? "Connect your printer" : "Enter printer address",
              ColorText, &lv_font_montserrat_24, 0, 0);
    makeLabel(content_, selectedPrinterName_[0] ? selectedPrinterName_ : "Moonraker connection",
              selectedPrinterHost_[0] ? ColorCyan : ColorMuted,
              &lv_font_montserrat_10, 0, 39, 300);

    lv_obj_t* change = lv_btn_create(content_);
    lv_obj_set_size(change, 78, 28);
    lv_obj_set_pos(change, 354, 1);
    styleButton(change, false);
    lv_obj_add_event_cb(change, actionEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(Action::PrinterChange)));
    lv_obj_t* changeLabel = lv_label_create(change);
    styleText(changeLabel, ColorText, &lv_font_montserrat_10);
    lv_label_set_text(changeLabel, "Change");
    lv_obj_center(changeLabel);

    const AppSettings& settings = settingsService().settings();
    makeLabel(content_, "HOST OR IP ADDRESS", ColorCyan, &lv_font_montserrat_10, 0, 70);
    printerHostField_ = lv_textarea_create(content_);
    lv_obj_set_size(printerHostField_, 310, 42);
    lv_obj_set_pos(printerHostField_, 0, 88);
    styleInput(printerHostField_);
    lv_textarea_set_one_line(printerHostField_, true);
    lv_textarea_set_max_length(printerHostField_, 64);
    lv_textarea_set_placeholder_text(printerHostField_, "192.168.1.50");
    lv_textarea_set_text(printerHostField_,
                         selectedPrinterHost_[0] ? selectedPrinterHost_ : settings.printerHost);
    if (selectedPrinterHost_[0]) lv_obj_add_state(printerHostField_, LV_STATE_DISABLED);
    lv_obj_add_event_cb(printerHostField_, fieldEvent, LV_EVENT_FOCUSED, this);

    makeLabel(content_, "PORT", ColorCyan, &lv_font_montserrat_10, 326, 70);
    printerPortField_ = lv_textarea_create(content_);
    lv_obj_set_size(printerPortField_, 106, 42);
    lv_obj_set_pos(printerPortField_, 326, 88);
    styleInput(printerPortField_);
    lv_textarea_set_one_line(printerPortField_, true);
    lv_textarea_set_max_length(printerPortField_, 5);
    lv_textarea_set_accepted_chars(printerPortField_, "0123456789");
    char portText[6] = "";
    snprintf(portText, sizeof(portText), "%u",
             static_cast<unsigned>(selectedPrinterHost_[0]
                                       ? selectedPrinterPort_
                                       : settings.printerPort));
    lv_textarea_set_text(printerPortField_, portText);
    if (selectedPrinterHost_[0]) lv_obj_add_state(printerPortField_, LV_STATE_DISABLED);
    lv_obj_add_event_cb(printerPortField_, fieldEvent, LV_EVENT_FOCUSED, this);

    makeLabel(content_, selectedPrinterHost_[0]
                            ? "Printer was detected and its Moonraker endpoint was verified."
                            : "Default Moonraker port: 7125",
              ColorMuted,
              &lv_font_montserrat_10, 0, 149);
}

void SetupWizard::renderReady() {
    makeLabel(content_, "Ready when you are", ColorText, &lv_font_montserrat_28, 0, 0);
    makeLabel(content_, "Review the essentials. Every setting remains editable later.",
              ColorMuted, &lv_font_montserrat_12, 0, 38, 420);

    const AppSettings& settings = settingsService().settings();
    char effectiveName[25] = "";
    deviceIdentity().effectiveName(settings.deviceName, effectiveName, sizeof(effectiveName));

    makeLabel(content_, "DEVICE", ColorCyan, &lv_font_montserrat_10, 0, 78);
    makeLabel(content_, effectiveName, ColorText, &lv_font_montserrat_16, 0, 96, 200);
    makeLabel(content_, "CONTROL", ColorCyan, &lv_font_montserrat_10, 230, 78);
    makeLabel(content_, transportName(settings.companionTransport), ColorText,
              &lv_font_montserrat_16, 230, 96, 200);

    makeLabel(content_, "NETWORK", ColorCyan, &lv_font_montserrat_10, 0, 134);
    makeLabel(content_, settings.wifiSsid[0] ? settings.wifiSsid : "Set up later",
              ColorText, &lv_font_montserrat_14, 0, 151, 200);
    makeLabel(content_, "PRINTER", ColorCyan, &lv_font_montserrat_10, 230, 134);
    makeLabel(content_, settings.printerHost[0] ? settings.printerHost : "Set up later",
              ColorText, &lv_font_montserrat_14, 230, 151, 200);
}

void SetupWizard::commitCurrentStep() {
    AppSettings& settings = settingsService().mutableSettings();
    switch (step_) {
        case Step::Identity: {
            char cleanName[sizeof(settings.deviceName)] = "";
            deviceIdentity().sanitizeName(nameField_ ? lv_textarea_get_text(nameField_) : "",
                                          cleanName, sizeof(cleanName));
            strlcpy(settings.deviceName, cleanName, sizeof(settings.deviceName));
            break;
        }
        case Step::Network:
            if (networkCredentialsView_ && networkConnectionVerified_) {
                strlcpy(settings.wifiSsid,
                        selectedSsid_[0]
                            ? selectedSsid_
                            : (ssidField_ ? lv_textarea_get_text(ssidField_) : ""),
                        sizeof(settings.wifiSsid));
                strlcpy(settings.wifiPassword,
                        selectedNetworkSecured_ ? networkPassword_ : "",
                        sizeof(settings.wifiPassword));
            }
            break;
        case Step::Printer: {
            if (!printerDetailsView_) break;
            uint16_t port = settings.printerPort ? settings.printerPort : 7125;
            if (printerPortField_) {
                const int parsed = atoi(lv_textarea_get_text(printerPortField_));
                if (parsed > 0 && parsed <= 65535) port = static_cast<uint16_t>(parsed);
            }
            char cleanHost[sizeof(settings.printerHost)] = "";
            normalizePrinterHost(printerHostField_ ? lv_textarea_get_text(printerHostField_) : "",
                                 cleanHost, sizeof(cleanHost), port);
            strlcpy(settings.printerHost, cleanHost, sizeof(settings.printerHost));
            settings.printerPort = port;
            break;
        }
        case Step::Welcome:
        case Step::Connection:
        case Step::Ready:
        case Step::Count:
            break;
    }
    settingsService().save();
}

void SetupWizard::moveNext() {
    hideKeyboard();

    if (step_ == Step::Network && networkCredentialsView_) {
        if (!networkConnectionVerified_) {
            const char* ssid = selectedSsid_[0]
                                   ? selectedSsid_
                                   : (ssidField_ ? lv_textarea_get_text(ssidField_) : "");
            const char* password = selectedNetworkSecured_ && passwordField_
                                       ? lv_textarea_get_text(passwordField_)
                                       : "";
            strlcpy(selectedSsid_, ssid ? ssid : "", sizeof(selectedSsid_));
            strlcpy(networkPassword_, password ? password : "", sizeof(networkPassword_));
            wifiService().requestConnectionTest(selectedSsid_, networkPassword_);
            wifiConnectionRevisionSeen_ = wifiService().connectionRevision();
            renderPending_ = true;
            return;
        }
        commitCurrentStep();
        settingsService().flush();
        wifiService().acceptConnectionTest();
        step_ = Step::Printer;
        printerService().requestDiscovery();
        renderPending_ = true;
        return;
    }

    commitCurrentStep();
    if (step_ == Step::Ready) {
        finish();
        return;
    }
    step_ = static_cast<Step>(static_cast<uint8_t>(step_) + 1);
    if (step_ == Step::Printer && state().wifiConnected) {
        printerService().requestDiscovery();
    }
    renderPending_ = true;
}

void SetupWizard::moveBack() {
    if (step_ == Step::Welcome) return;
    hideKeyboard();
    if (step_ == Step::Network && wifiService().connectionStatus() != WifiConnectStatus::Idle) {
        wifiService().cancelConnectionTest();
        networkConnectionVerified_ = false;
    }
    commitCurrentStep();
    step_ = static_cast<Step>(static_cast<uint8_t>(step_) - 1);
    renderPending_ = true;
}

void SetupWizard::finish() {
    AppSettings& settings = settingsService().mutableSettings();
    settings.setupDone = true;
    state().setupDone = true;
    settingsService().save();
    settingsService().flush();
    finished_ = true;
    Serial.println("[setup] first-run wizard completed");
}

void SetupWizard::showKeyboard(lv_obj_t* textarea) {
    if (!textarea || !root_) return;
    if (!keyboard_) {
        keyboard_ = lv_keyboard_create(root_);
        lv_obj_set_size(keyboard_, kScreenWidth, 160);
        lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(keyboard_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(keyboard_, lv_color_hex(ColorSurface), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(keyboard_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(keyboard_, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(keyboard_, lv_color_hex(ColorBorder), LV_PART_MAIN);
        lv_obj_set_style_bg_color(keyboard_, lv_color_hex(ColorSurfaceRaised), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard_, lv_color_hex(ColorCyan),
                                  static_cast<lv_style_selector_t>(
                                      static_cast<uint32_t>(LV_PART_ITEMS) |
                                      static_cast<uint32_t>(LV_STATE_PRESSED)));
        lv_obj_set_style_text_color(keyboard_, lv_color_hex(ColorText), LV_PART_ITEMS);
        lv_obj_set_style_text_font(keyboard_, &lv_font_montserrat_14, LV_PART_ITEMS);
        lv_obj_add_event_cb(keyboard_, keyboardEvent, LV_EVENT_READY, this);
        lv_obj_add_event_cb(keyboard_, keyboardEvent, LV_EVENT_CANCEL, this);
    }
    lv_keyboard_set_mode(keyboard_, textarea == printerPortField_
                                        ? LV_KEYBOARD_MODE_NUMBER
                                        : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keyboard_, textarea);
    if (content_) lv_obj_set_y(content_, 0);
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard_);
}

void SetupWizard::hideKeyboard() {
    if (content_) lv_obj_set_y(content_, 66);
    if (!keyboard_) return;
    lv_keyboard_set_textarea(keyboard_, nullptr);
    lv_obj_del_async(keyboard_);
    keyboard_ = nullptr;
}

void SetupWizard::updateNavigation() {
    if (!backButton_ || !nextButtonLabel_) return;
    if (step_ == Step::Welcome) lv_obj_add_flag(backButton_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(backButton_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_state(nextButton_, LV_STATE_DISABLED);
    const char* nextText = "Continue";
    if (step_ == Step::Welcome) nextText = "Start";
    else if (step_ == Step::Network && !networkCredentialsView_) nextText = "Skip";
    else if (step_ == Step::Network && networkCredentialsView_) {
        if (wifiService().connectionStatus() == WifiConnectStatus::Connecting) {
            nextText = "Connecting...";
            lv_obj_add_state(nextButton_, LV_STATE_DISABLED);
        } else if (!networkConnectionVerified_) {
            nextText = "Connect";
        }
    } else if (step_ == Step::Printer && !printerDetailsView_) {
        nextText = "Skip";
    }
    else if (step_ == Step::Ready) nextText = "Finish";
    lv_label_set_text(nextButtonLabel_, nextText);
    lv_obj_center(nextButtonLabel_);
}

void SetupWizard::updateProgress() {
    const uint8_t current = static_cast<uint8_t>(step_);
    if (stepLabel_) {
        lv_label_set_text_fmt(stepLabel_, "%u OF %u",
                              static_cast<unsigned>(current + 1),
                              static_cast<unsigned>(kStepCount));
    }
    for (uint8_t index = 0; index < kStepCount; ++index) {
        if (!progress_[index]) continue;
        const uint32_t color = index <= current ? ColorCyan : ColorBorder;
        lv_obj_set_style_bg_color(progress_[index], lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(progress_[index], index <= current ? LV_OPA_COVER : LV_OPA_60,
                                LV_PART_MAIN);
    }
}

void SetupWizard::actionEvent(lv_event_t* event) {
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const uintptr_t rawAction = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    const Action action = static_cast<Action>(rawAction);
    SetupWizard* wizard = gActiveSetupWizard;
    if (!wizard) return;
    markTouch();

    const uintptr_t printerBase = static_cast<uintptr_t>(Action::PrinterBase);
    if (rawAction >= printerBase) {
        const uint8_t index = static_cast<uint8_t>(rawAction - printerBase);
        DiscoveredPrinter printer;
        if (printerService().discoveredPrinter(index, printer)) {
            strlcpy(wizard->selectedPrinterName_, printer.name,
                    sizeof(wizard->selectedPrinterName_));
            strlcpy(wizard->selectedPrinterHost_, printer.host,
                    sizeof(wizard->selectedPrinterHost_));
            wizard->selectedPrinterPort_ = printer.port;
            wizard->printerDetailsView_ = true;
            wizard->renderPending_ = true;
        }
        return;
    }

    const uintptr_t networkBase = static_cast<uintptr_t>(Action::NetworkBase);
    if (rawAction >= networkBase && rawAction < printerBase) {
        const uint8_t index = static_cast<uint8_t>(rawAction - networkBase);
        const WifiNetworkInfo* network = wifiService().network(index);
        if (network) {
            strlcpy(wizard->selectedSsid_, network->ssid, sizeof(wizard->selectedSsid_));
            wizard->selectedNetworkSecured_ = network->secured;
            wizard->networkConnectionVerified_ = false;
            wizard->networkPassword_[0] = '\0';
            wizard->networkCredentialsView_ = true;
            wizard->renderPending_ = true;
        }
        return;
    }

    AppSettings& settings = settingsService().mutableSettings();
    switch (action) {
        case Action::Back:
            wizard->moveBack();
            break;
        case Action::Next:
            wizard->moveNext();
            break;
        case Action::TransportAuto:
            settings.companionTransport = CompanionTransport::Auto;
            settings.bleEnabled = true;
            settingsService().save();
            wizard->renderPending_ = true;
            break;
        case Action::TransportBle:
            settings.companionTransport = CompanionTransport::Ble;
            settings.bleEnabled = true;
            settingsService().save();
            wizard->renderPending_ = true;
            break;
        case Action::TransportWifi:
            settings.companionTransport = CompanionTransport::Wifi;
            settings.bleEnabled = true;
            settingsService().save();
            wizard->renderPending_ = true;
            break;
        case Action::NetworkRefresh:
            wifiService().requestScan();
            wizard->wifiScanRevisionSeen_ = wifiService().scanRevision();
            wizard->renderPending_ = true;
            break;
        case Action::NetworkManual:
            wizard->selectedSsid_[0] = '\0';
            wizard->networkPassword_[0] = '\0';
            wizard->selectedNetworkSecured_ = true;
            wizard->networkConnectionVerified_ = false;
            wizard->networkCredentialsView_ = true;
            wizard->renderPending_ = true;
            break;
        case Action::NetworkChange:
            wifiService().cancelConnectionTest();
            wizard->networkConnectionVerified_ = false;
            wizard->networkCredentialsView_ = false;
            wizard->renderPending_ = true;
            break;
        case Action::PrinterRefresh:
            printerService().requestDiscovery();
            wizard->renderPending_ = true;
            break;
        case Action::PrinterManual:
            wizard->selectedPrinterName_[0] = '\0';
            wizard->selectedPrinterHost_[0] = '\0';
            wizard->selectedPrinterPort_ = 7125;
            wizard->printerDetailsView_ = true;
            wizard->renderPending_ = true;
            break;
        case Action::PrinterChange:
            wizard->printerDetailsView_ = false;
            wizard->renderPending_ = true;
            break;
        case Action::NetworkBase:
        case Action::PrinterBase:
            break;
    }
}

void SetupWizard::fieldEvent(lv_event_t* event) {
    if (!event || lv_event_get_code(event) != LV_EVENT_FOCUSED) return;
    SetupWizard* wizard = static_cast<SetupWizard*>(lv_event_get_user_data(event));
    if (!wizard) return;
    markTouch();
    wizard->showKeyboard(lv_event_get_target(event));
}

void SetupWizard::keyboardEvent(lv_event_t* event) {
    if (!event) return;
    SetupWizard* wizard = static_cast<SetupWizard*>(lv_event_get_user_data(event));
    if (!wizard) return;
    markTouch();
    wizard->hideKeyboard();
}

}
