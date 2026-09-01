#include "DisplayService.h"

#include <Arduino.h>
#include <lvgl.h>

#include "../config/AppConfig.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"
#include "../bsp/display.h"
#include "../bsp/esp_bsp.h"

namespace coronet {

namespace {

constexpr uint32_t kUiUpdateIntervalMs = 250;
constexpr uint32_t kLvglLockTimeoutMs = 1000;

lv_obj_t* root = nullptr;
lv_obj_t* uptimeLabel = nullptr;
lv_obj_t* servicesLabel = nullptr;
lv_obj_t* memoryLabel = nullptr;
lv_obj_t* touchLabel = nullptr;
lv_obj_t* touchDot = nullptr;
lv_obj_t* printerLabel = nullptr;

const char* onOff(bool value) {
    return value ? "ON" : "OFF";
}

void markTouch() {
    SystemState& s = state();
    s.touchCount++;
    s.lastTouchMs = millis();
}

void touchEventCb(lv_event_t* event) {
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED) {
        markTouch();
    }
}

void styleText(lv_obj_t* obj, lv_color_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
}

void setPanelStyle(lv_obj_t* obj, lv_color_t bg, lv_color_t border) {
    lv_obj_set_style_bg_color(obj, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, border, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 14, LV_PART_MAIN);
}

void updateDashboardScreen() {
    if (!root) return;

    const SystemState& s = state();
    if (uptimeLabel) {
        lv_label_set_text_fmt(uptimeLabel, "uptime %lus", static_cast<unsigned long>(s.uptimeMs / 1000UL));
    }
    if (servicesLabel) {
        lv_label_set_text_fmt(servicesLabel,
                              "display %s   touch %s   audio %s   wifi %s   web %s   ble %s",
                              onOff(s.displayReady),
                              onOff(s.touchReady),
                              onOff(s.audioReady),
                              onOff(s.wifiConnected),
                              onOff(s.webReady),
                              onOff(s.bleReady));
    }
    if (memoryLabel) {
        lv_label_set_text_fmt(memoryLabel,
                              "internal %lu KB   dma %lu KB   psram %lu KB",
                              static_cast<unsigned long>(s.internalFree / 1024UL),
                              static_cast<unsigned long>(s.dmaLargest / 1024UL),
                              static_cast<unsigned long>(s.psramFree / 1024UL));
    }
    if (touchLabel) {
        const uint32_t age = s.lastTouchMs ? (millis() - s.lastTouchMs) : 0;
        lv_label_set_text_fmt(touchLabel,
                              "touches %lu   last %lums",
                              static_cast<unsigned long>(s.touchCount),
                              static_cast<unsigned long>(age));
    }
    if (touchDot) {
        const bool recentTouch = s.lastTouchMs && (millis() - s.lastTouchMs < 450);
        lv_obj_set_style_bg_color(touchDot,
                                  recentTouch ? lv_color_hex(0x35F6C8) : lv_color_hex(0x315066),
                                  LV_PART_MAIN);
    }
    if (printerLabel) {
        lv_label_set_text_fmt(printerLabel,
                              "printer %s   %s   %u%%   %s",
                              s.printerConfigured ? "CFG" : "NO CFG",
                              s.printerConnected ? "ONLINE" : "OFFLINE",
                              static_cast<unsigned>(s.printProgress),
                              s.printerStatusText[0] ? s.printerStatusText : "-");
    }
}

}

void DisplayService::begin() {
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90,
    };
    cfg.lvgl_port_cfg.task_priority = 16;
    cfg.lvgl_port_cfg.task_affinity = 1;
    cfg.lvgl_port_cfg.task_stack = 8192;
    cfg.lvgl_port_cfg.task_max_sleep_ms = 40;
    cfg.lvgl_port_cfg.timer_period_ms = 5;

    lv_disp_t* display = bsp_display_start_with_config(&cfg);
    if (!display || !lv_disp_get_default()) {
        Serial.println("DisplayService failed: LVGL display was not created");
        state().displayReady = false;
        return;
    }

    state().displayReady = true;
    state().touchReady = (bsp_display_get_input_dev() != nullptr);

    if (bsp_display_lock(kLvglLockTimeoutMs)) {
        state().setupDone = settingsService().settings().setupDone;
        if (settingsService().settings().setupDone) {
            buildDashboardScreen();
        } else {
            setupWizard_.begin();
            wizardActive_ = true;
        }
        bsp_display_unlock();
    } else {
        Serial.println("DisplayService warning: LVGL lock timeout while building boot screen");
    }

    bsp_display_backlight_on();
    applyBrightness(settingsService().settings().displayBrightness);
    appliedBrightness_ = settingsService().settings().displayBrightness;
    started_ = true;

    Serial.printf("DisplayService ready, brightness=%u%%, touch=%s\n",
                  static_cast<unsigned>(settingsService().settings().displayBrightness),
                  state().touchReady ? "ready" : "missing");
}

void DisplayService::loop() {
    if (!started_) return;
    const uint8_t requestedBrightness = settingsService().settings().displayBrightness;
    if (requestedBrightness != appliedBrightness_) {
        applyBrightness(requestedBrightness);
        appliedBrightness_ = requestedBrightness;
    }

    const uint32_t now = millis();
    const uint32_t updateIntervalMs = wizardActive_ ? 30 : kUiUpdateIntervalMs;
    if (now - lastUiUpdateMs_ < updateIntervalMs) return;
    lastUiUpdateMs_ = now;

    if (bsp_display_lock(5)) {
        if (wizardActive_) {
            setupWizard_.loop();
            if (setupWizard_.finished()) {
                buildDashboardScreen(true);
                setupWizard_.reset();
                wizardActive_ = false;
            }
        } else {
            updateDashboardScreen();
        }
        bsp_display_unlock();
    }
}

void DisplayService::buildDashboardScreen(bool animate) {
    root = lv_obj_create(nullptr);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x07111E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(root, touchEventCb, LV_EVENT_PRESSED, nullptr);

    lv_obj_t* title = lv_label_create(root);
    styleText(title, lv_color_hex(0xF8FAFC), &lv_font_montserrat_34);
    lv_label_set_text(title, "coroNET");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 28, 26);

    lv_obj_t* subtitle = lv_label_create(root);
    styleText(subtitle, lv_color_hex(0x9BE7FF), &lv_font_montserrat_16);
    lv_label_set_text(subtitle, "OS 2 bring-up build");
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_LEFT, 2, 4);

    lv_obj_t* badge = lv_label_create(root);
    styleText(badge, lv_color_hex(0xF5C542), &lv_font_montserrat_14);
    lv_label_set_text_fmt(badge, "%s   MIT", config::FirmwareVersion);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -28, 34);

    lv_obj_t* card = lv_obj_create(root);
    lv_obj_set_size(card, 424, 198);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 24);
    setPanelStyle(card, lv_color_hex(0x0D1B2D), lv_color_hex(0x1E9FD6));

    uptimeLabel = lv_label_create(card);
    styleText(uptimeLabel, lv_color_hex(0xF8FAFC), &lv_font_montserrat_22);
    lv_label_set_text(uptimeLabel, "uptime 0s");
    lv_obj_align(uptimeLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    servicesLabel = lv_label_create(card);
    styleText(servicesLabel, lv_color_hex(0xB8D7E8), &lv_font_montserrat_14);
    lv_obj_set_width(servicesLabel, 390);
    lv_label_set_text(servicesLabel, "display ON   touch ON   audio ON   wifi OFF   web OFF   ble ON");
    lv_obj_align(servicesLabel, LV_ALIGN_TOP_LEFT, 0, 45);

    memoryLabel = lv_label_create(card);
    styleText(memoryLabel, lv_color_hex(0x8EAFC6), &lv_font_montserrat_14);
    lv_obj_set_width(memoryLabel, 390);
    lv_label_set_text(memoryLabel, "heap - KB   dma - KB   psram - KB");
    lv_obj_align(memoryLabel, LV_ALIGN_TOP_LEFT, 0, 78);

    printerLabel = lv_label_create(card);
    styleText(printerLabel, lv_color_hex(0x9BE7FF), &lv_font_montserrat_14);
    lv_obj_set_width(printerLabel, 390);
    lv_label_set_text(printerLabel, "printer -");
    lv_obj_align(printerLabel, LV_ALIGN_TOP_LEFT, 0, 111);

    lv_obj_t* touchRow = lv_obj_create(card);
    lv_obj_clear_flag(touchRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(touchRow, 390, 36);
    lv_obj_align(touchRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(touchRow, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(touchRow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(touchRow, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(touchRow, touchEventCb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(touchRow, touchEventCb, LV_EVENT_CLICKED, nullptr);

    touchDot = lv_obj_create(touchRow);
    lv_obj_set_size(touchDot, 16, 16);
    lv_obj_set_style_radius(touchDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(touchDot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(touchDot, lv_color_hex(0x315066), LV_PART_MAIN);
    lv_obj_align(touchDot, LV_ALIGN_LEFT_MID, 0, 0);

    touchLabel = lv_label_create(touchRow);
    styleText(touchLabel, lv_color_hex(0xD5F7FF), &lv_font_montserrat_14);
    lv_label_set_text(touchLabel, "touches 0   last -");
    lv_obj_align_to(touchLabel, touchDot, LV_ALIGN_OUT_RIGHT_MID, 12, 0);

    if (animate) lv_scr_load_anim(root, LV_SCR_LOAD_ANIM_FADE_ON, 280, 0, true);
    else lv_scr_load_anim(root, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    updateDashboardScreen();
}

void DisplayService::applyBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    bsp_display_brightness_set(percent);
}

}
