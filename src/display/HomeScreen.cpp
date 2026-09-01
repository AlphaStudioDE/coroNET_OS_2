#include "HomeScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "UiTheme.h"

namespace coronet {

namespace {

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

void stylePanel(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(object, ui::CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(object, lv_color_hex(ui::ColorSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(object, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
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
    if (width != LV_SIZE_CONTENT) lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

void markTouch() {
    SystemState& system = state();
    system.touchCount++;
    system.lastTouchMs = millis();
}

void touchEvent(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) markTouch();
}

int16_t temperatureTenths(float value) {
    if (isnan(value)) return INT16_MIN;
    if (value > 999.9f) value = 999.9f;
    if (value < -99.9f) value = -99.9f;
    return static_cast<int16_t>(lroundf(value * 10.0f));
}

void setTemperature(lv_obj_t* label, int16_t tenths) {
    if (!label) return;
    if (tenths == INT16_MIN) {
        lv_label_set_text(label, "--.- C");
        return;
    }
    const int16_t absolute = tenths < 0 ? static_cast<int16_t>(-tenths) : tenths;
    lv_label_set_text_fmt(label, "%s%d.%d C",
                          tenths < 0 ? "-" : "",
                          static_cast<int>(absolute / 10),
                          static_cast<int>(absolute % 10));
}

const char* stateTitle(bool printerConfigured, bool printerConnected, PrinterState printerState) {
    if (!printerConfigured) return "NO PRINTER";
    if (!printerConnected) return "PRINTER OFFLINE";
    switch (printerState) {
        case PrinterState::Idle: return "READY";
        case PrinterState::Printing: return "PRINTING";
        case PrinterState::Paused: return "PAUSED";
        case PrinterState::Error: return "ATTENTION";
        case PrinterState::Complete: return "PRINT COMPLETE";
        case PrinterState::Unknown:
        default: return "CONNECTED";
    }
}

uint32_t stateColor(bool printerConfigured, bool printerConnected, PrinterState printerState) {
    if (!printerConfigured || !printerConnected) return ui::ColorMuted;
    switch (printerState) {
        case PrinterState::Paused: return ui::ColorAmber;
        case PrinterState::Error: return ui::ColorRed;
        case PrinterState::Complete: return ui::ColorGreen;
        default: return ui::ColorCyan;
    }
}

const char* offlineDetail(bool printerConfigured, const char* status) {
    if (!printerConfigured) return "Add a printer from Settings";
    if (strcmp(status, "wifi_offline") == 0) return "Waiting for Wi-Fi";
    if (strcmp(status, "connecting") == 0 || strcmp(status, "waiting_for_wifi") == 0) {
        return "Connecting to printer";
    }
    return "Printer connection unavailable";
}

lv_obj_t* makeMetricCard(lv_obj_t* parent, lv_coord_t x, const char* caption, uint32_t accent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 144, 82);
    lv_obj_set_pos(card, x, 160);
    stylePanel(card);

    lv_obj_t* line = lv_obj_create(card);
    lv_obj_set_size(line, 32, 3);
    lv_obj_set_pos(line, 12, 11);
    lv_obj_set_style_radius(line, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(line, lv_color_hex(accent), LV_PART_MAIN);
    lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);

    makeLabel(card, caption, ui::ColorMuted, &lv_font_montserrat_10, 12, 21);
    return card;
}

void formatDuration(uint32_t seconds, char out[12]) {
    const uint32_t hours = seconds / 3600U;
    const uint32_t minutes = (seconds / 60U) % 60U;
    snprintf(out, 12, "%02lu:%02lu", static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes));
}

}

void HomeScreen::begin(ui::Navigation::Callback navigationCallback,
                       void* callbackContext,
                       bool animate) {
    cacheValid_ = false;
    root_ = lv_obj_create(nullptr);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(root_, touchEvent, LV_EVENT_PRESSED, nullptr);

    buildHeader();
    buildPrinterPanel();
    buildMetricCards();
    navigation_.build(root_, ui::Page::Home, navigationCallback, callbackContext);

    lv_scr_load_anim(root_,
                     animate ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_NONE,
                     animate ? 240 : 0,
                     0,
                     true);
    update();
}

void HomeScreen::buildHeader() {
    makeLabel(root_, "coroNET", ui::ColorText, &lv_font_montserrat_22, 18, 11);
    makeLabel(root_, "HOME", ui::ColorCyan, &lv_font_montserrat_10, 127, 20);

    wifiLabel_ = makeLabel(root_, LV_SYMBOL_WIFI, ui::ColorMuted, &lv_font_montserrat_16, 340, 14, 24);
    lv_obj_set_style_text_align(wifiLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    bleLabel_ = makeLabel(root_, "BT", ui::ColorMuted, &lv_font_montserrat_12, 375, 17, 28);
    lv_obj_set_style_text_align(bleLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    printerDot_ = lv_obj_create(root_);
    lv_obj_set_size(printerDot_, 8, 8);
    lv_obj_set_pos(printerDot_, 416, 20);
    lv_obj_set_style_radius(printerDot_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(printerDot_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(printerDot_, lv_color_hex(ui::ColorMuted), LV_PART_MAIN);
    lv_obj_set_style_pad_all(printerDot_, 0, LV_PART_MAIN);

    printerConnectionLabel_ = makeLabel(root_, "PRN", ui::ColorMuted,
                                        &lv_font_montserrat_10, 430, 18, 32);

    lv_obj_t* divider = lv_obj_create(root_);
    lv_obj_set_size(divider, 444, 1);
    lv_obj_set_pos(divider, 18, 49);
    lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(divider, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
}

void HomeScreen::buildPrinterPanel() {
    lv_obj_t* panel = lv_obj_create(root_);
    lv_obj_set_size(panel, 448, 94);
    lv_obj_set_pos(panel, 16, 58);
    stylePanel(panel);

    statusAccent_ = lv_obj_create(panel);
    lv_obj_set_size(statusAccent_, 4, 68);
    lv_obj_set_pos(statusAccent_, 0, 13);
    lv_obj_set_style_radius(statusAccent_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(statusAccent_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusAccent_, lv_color_hex(ui::ColorMuted), LV_PART_MAIN);
    lv_obj_set_style_pad_all(statusAccent_, 0, LV_PART_MAIN);

    stateLabel_ = makeLabel(panel, "NO PRINTER", ui::ColorText,
                            &lv_font_montserrat_22, 16, 11, 310);
    lv_label_set_long_mode(stateLabel_, LV_LABEL_LONG_DOT);

    detailLabel_ = makeLabel(panel, "Add a printer from Settings", ui::ColorMuted,
                             &lv_font_montserrat_12, 16, 42, 326);
    lv_label_set_long_mode(detailLabel_, LV_LABEL_LONG_DOT);

    progressLabel_ = makeLabel(panel, "0%", ui::ColorText,
                               &lv_font_montserrat_22, 358, 10, 72);
    lv_obj_set_style_text_align(progressLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    progressBar_ = lv_bar_create(panel);
    lv_obj_set_size(progressBar_, 414, 7);
    lv_obj_set_pos(progressBar_, 16, 73);
    lv_bar_set_range(progressBar_, 0, 100);
    lv_bar_set_value(progressBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(progressBar_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progressBar_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(ui::ColorCyan), LV_PART_INDICATOR);
    lv_obj_set_style_radius(progressBar_, 0, LV_PART_INDICATOR);
}

void HomeScreen::buildMetricCards() {
    lv_obj_t* toolCard = makeMetricCard(root_, 16, "ACTIVE TOOL", ui::ColorCyan);
    toolValueLabel_ = makeLabel(toolCard, "T1", ui::ColorText, &lv_font_montserrat_24, 12, 41);
    toolTempLabel_ = makeLabel(toolCard, "--.- C", ui::ColorMuted,
                               &lv_font_montserrat_12, 58, 49, 74);
    lv_obj_set_style_text_align(toolTempLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    materialLabel_ = makeLabel(toolCard, "--", ui::ColorMuted,
                               &lv_font_montserrat_10, 12, 68, 96);
    lv_label_set_long_mode(materialLabel_, LV_LABEL_LONG_DOT);
    materialSwatch_ = lv_obj_create(toolCard);
    lv_obj_set_size(materialSwatch_, 10, 10);
    lv_obj_set_pos(materialSwatch_, 120, 69);
    lv_obj_set_style_radius(materialSwatch_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(materialSwatch_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(materialSwatch_, lv_color_hex(ui::ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(materialSwatch_, 0, LV_PART_MAIN);

    lv_obj_t* bedCard = makeMetricCard(root_, 168, "BED", ui::ColorAmber);
    bedTempLabel_ = makeLabel(bedCard, "--.- C", ui::ColorText,
                              &lv_font_montserrat_22, 12, 41, 120);

    lv_obj_t* chamberCard = makeMetricCard(root_, 320, "CHAMBER", ui::ColorGreen);
    chamberTempLabel_ = makeLabel(chamberCard, "--.- C", ui::ColorText,
                                  &lv_font_montserrat_22, 12, 41, 120);
    ventOutputLabel_ = makeLabel(chamberCard, "F0  V0", ui::ColorMuted,
                                 &lv_font_montserrat_10, 12, 68, 120);
}

void HomeScreen::update() {
    if (!root_) return;
    const SystemState& system = state();

    ViewCache next;
    next.wifiConnected = system.wifiConnected;
    next.bleReady = system.bleReady;
    next.bleConnected = system.bleConnected;
    next.printerConfigured = system.printerConfigured;
    next.printerConnected = system.printerConnected;
    next.printerState = system.printerState;
    next.progress = system.printProgress > 100 ? 100 : system.printProgress;
    next.activeTool = system.activeTool;
    next.toolTempTenths = temperatureTenths(system.activeToolTempC);
    next.bedTempTenths = temperatureTenths(system.bedTempC);
    next.chamberTempTenths = temperatureTenths(system.chamberTempC);
    next.printDurationSec = system.printDurationSec;
    next.printEtaSec = system.printEtaSec;
    next.filamentColorRgb = system.filamentColorRgb;
    next.fanPercent = system.fanPercent;
    next.flapPercent = system.flapPercent;
    strlcpy(next.filename, system.printFilename, sizeof(next.filename));
    strlcpy(next.material, system.materialName, sizeof(next.material));
    strlcpy(next.status, system.printerStatusText, sizeof(next.status));

    if (!stateChanged(next)) return;
    cache_ = next;
    cacheValid_ = true;

    lv_obj_set_style_text_color(wifiLabel_,
                                lv_color_hex(next.wifiConnected ? ui::ColorCyan : ui::ColorMuted),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(bleLabel_,
                                lv_color_hex(next.bleConnected ? ui::ColorCyan : ui::ColorMuted),
                                LV_PART_MAIN);

    const uint32_t connectionColor = next.printerConnected ? ui::ColorGreen : ui::ColorMuted;
    lv_obj_set_style_bg_color(printerDot_, lv_color_hex(connectionColor), LV_PART_MAIN);
    lv_obj_set_style_text_color(printerConnectionLabel_, lv_color_hex(connectionColor), LV_PART_MAIN);

    const uint32_t color = stateColor(next.printerConfigured, next.printerConnected,
                                      next.printerState);
    lv_label_set_text(stateLabel_, stateTitle(next.printerConfigured, next.printerConnected,
                                              next.printerState));
    lv_obj_set_style_text_color(stateLabel_, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusAccent_, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(color), LV_PART_INDICATOR);

    if (!next.printerConnected) {
        lv_label_set_text(detailLabel_, offlineDetail(next.printerConfigured, next.status));
    } else if (next.filename[0]) {
        if (next.printerState == PrinterState::Printing || next.printerState == PrinterState::Paused) {
            char elapsed[12], eta[12], detail[112];
            formatDuration(next.printDurationSec, elapsed);
            formatDuration(next.printEtaSec, eta);
            snprintf(detail, sizeof(detail), "%s  |  %s  |  ETA %s", next.filename, elapsed, eta);
            lv_label_set_text(detailLabel_, detail);
        } else {
            lv_label_set_text(detailLabel_, next.filename);
        }
    } else {
        lv_label_set_text(detailLabel_, "Printer is ready");
    }

    lv_bar_set_value(progressBar_, next.progress, LV_ANIM_OFF);
    lv_label_set_text_fmt(progressLabel_, "%u%%", static_cast<unsigned>(next.progress));
    lv_label_set_text_fmt(toolValueLabel_, "T%u", static_cast<unsigned>(next.activeTool + 1U));
    setTemperature(toolTempLabel_, next.toolTempTenths);
    setTemperature(bedTempLabel_, next.bedTempTenths);
    setTemperature(chamberTempLabel_, next.chamberTempTenths);
    lv_label_set_text(materialLabel_, next.material[0] ? next.material : "MATERIAL --");
    lv_obj_set_style_bg_color(materialSwatch_, lv_color_hex(next.filamentColorRgb), LV_PART_MAIN);
    lv_label_set_text_fmt(ventOutputLabel_, "F%u  V%u", static_cast<unsigned>(next.fanPercent),
                          static_cast<unsigned>(next.flapPercent));
}

bool HomeScreen::stateChanged(const ViewCache& next) const {
    if (!cacheValid_) return true;
    return cache_.wifiConnected != next.wifiConnected ||
           cache_.bleReady != next.bleReady ||
           cache_.bleConnected != next.bleConnected ||
           cache_.printerConfigured != next.printerConfigured ||
           cache_.printerConnected != next.printerConnected ||
           cache_.printerState != next.printerState ||
           cache_.progress != next.progress ||
           cache_.activeTool != next.activeTool ||
           cache_.toolTempTenths != next.toolTempTenths ||
           cache_.bedTempTenths != next.bedTempTenths ||
           cache_.chamberTempTenths != next.chamberTempTenths ||
           cache_.printDurationSec != next.printDurationSec ||
           cache_.printEtaSec != next.printEtaSec ||
           cache_.filamentColorRgb != next.filamentColorRgb ||
           cache_.fanPercent != next.fanPercent || cache_.flapPercent != next.flapPercent ||
           strcmp(cache_.filename, next.filename) != 0 ||
           strcmp(cache_.material, next.material) != 0 ||
           strcmp(cache_.status, next.status) != 0;
}

}
