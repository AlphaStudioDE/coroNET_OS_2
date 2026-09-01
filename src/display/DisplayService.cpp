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
            showPage(ui::Page::Home);
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
                showPage(ui::Page::Home, true);
                setupWizard_.reset();
                wizardActive_ = false;
            }
        } else if (pageRequestPending_) {
            const ui::Page page = requestedPage_;
            pageRequestPending_ = false;
            if (page != activePage_) showPage(page, true);
        } else {
            if (activePage_ == ui::Page::Home) homeScreen_.update();
            else if (activePage_ == ui::Page::Settings) settingsScreen_.update();
        }
        bsp_display_unlock();
    }
}

void DisplayService::requestPage(ui::Page page) {
    if (page != ui::Page::Home && page != ui::Page::Settings) return;
    requestedPage_ = page;
    pageRequestPending_ = true;
}

void DisplayService::showPage(ui::Page page, bool animate) {
    if (page != ui::Page::Home && page != ui::Page::Settings) return;
    activePage_ = page;
    if (page == ui::Page::Home) {
        homeScreen_.begin(navigationRequested, this, animate);
    } else {
        settingsScreen_.begin(navigationRequested, setupRequested, this, animate);
    }
}

void DisplayService::reopenSetupWizard() {
    if (wizardActive_) return;
    AppSettings& settings = settingsService().mutableSettings();
    settings.setupDone = false;
    state().setupDone = false;
    settingsService().save();
    settingsService().flush();
    setupWizard_.begin();
    wizardActive_ = true;
}

void DisplayService::navigationRequested(ui::Page page, void* context) {
    DisplayService* service = static_cast<DisplayService*>(context);
    if (!service || service->wizardActive_ || page == service->activePage_) return;
    service->showPage(page, true);
}

void DisplayService::setupRequested(void* context) {
    DisplayService* service = static_cast<DisplayService*>(context);
    if (service) service->reopenSetupWizard();
}

void DisplayService::applyBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    bsp_display_brightness_set(percent);
}

}
