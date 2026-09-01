#include "DisplayService.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

#include "../config/AppConfig.h"
#include "../core/SystemState.h"
#include "../settings/SettingsService.h"
#include "../bsp/display.h"
#include "../bsp/esp_bsp.h"
#include "UiTheme.h"

namespace coronet {

namespace {

constexpr uint32_t kUiUpdateIntervalMs = 250;
constexpr uint32_t kLvglLockTimeoutMs = 1000;

}

void DisplayService::begin() {
    updateTheme();
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
    const uint32_t now = millis();
    updateTimeService(now);
    updateTheme();
    updateScreenSaver(now);
    const uint8_t requestedBrightness = settingsService().settings().displayBrightness;
    if (!screenSaverActive_ && requestedBrightness != appliedBrightness_) {
        applyBrightness(requestedBrightness);
        appliedBrightness_ = requestedBrightness;
    }

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
        } else if (screenSaverActive_) {
            if (screenSaverClock_) clockScreen_.update();
        } else if (pageRequestPending_) {
            const ui::Page page = requestedPage_;
            pageRequestPending_ = false;
            if (page != activePage_) showPage(page, true);
        } else {
            if (activePage_ == ui::Page::Home) homeScreen_.update();
            else if (activePage_ == ui::Page::Settings) settingsScreen_.update();
            else controlScreen_.update();
        }
        bsp_display_unlock();
    }
}

void DisplayService::updateTheme() {
    const AppSettings& settings = settingsService().settings();
    bool daytime = false;
    if (settings.uiColorMode == UiColorMode::Auto && state().timeReady) {
        time_t now = time(nullptr);
        struct tm local = {};
        daytime = localtime_r(&now, &local) && local.tm_hour >= 7 && local.tm_hour < 19;
    }
    const uint32_t signature = static_cast<uint32_t>(settings.uiSkin) |
                               (static_cast<uint32_t>(settings.uiColorMode) << 4U) |
                               (static_cast<uint32_t>(settings.accentHueDegrees) << 8U) |
                               (static_cast<uint32_t>(daytime) << 24U);
    if (signature == appliedThemeSignature_) return;
    ui::applyTheme(static_cast<uint8_t>(settings.uiSkin),
                   static_cast<uint8_t>(settings.uiColorMode),
                   settings.accentHueDegrees, daytime);
    const bool wasApplied = appliedThemeSignature_ != UINT32_MAX;
    appliedThemeSignature_ = signature;
    if (!started_ || !wasApplied || wizardActive_ || state().displaySleeping) return;
    if (bsp_display_lock(100)) {
        if (screenSaverActive_ && screenSaverClock_) clockScreen_.begin(settings.clockStyle);
        else if (!screenSaverActive_) showPage(activePage_, false);
        bsp_display_unlock();
    }
}

void DisplayService::updateTimeService(uint32_t now) {
    const AppSettings& settings = settingsService().settings();
    if (!state().wifiConnected) {
        state().timeReady = time(nullptr) >= 1700000000;
        return;
    }
    if (strcmp(configuredTimeZone_, settings.timeZone) != 0 ||
        (!state().timeReady && now - lastTimeSyncRequestMs_ >= 60000U)) {
        strlcpy(configuredTimeZone_, settings.timeZone, sizeof(configuredTimeZone_));
        configTzTime(configuredTimeZone_, "pool.ntp.org", "time.nist.gov");
        lastTimeSyncRequestMs_ = now;
    }
    state().timeReady = time(nullptr) >= 1700000000;
}

void DisplayService::updateScreenSaver(uint32_t now) {
    if (wizardActive_) return;
    const bool printerError = state().printerState == PrinterState::Error;
    if (printerError && !printerErrorSeen_) {
        state().lastTouchMs = now;
        if (screenSaverActive_) leaveScreenSaver(true);
    }
    printerErrorSeen_ = printerError;
    if (printerError) return;

    if (screenSaverActive_) {
        if (state().lastTouchMs != screenSaverActivityMark_) leaveScreenSaver(screenSaverClock_);
        return;
    }
    const AppSettings& settings = settingsService().settings();
    if (settings.screenSaverMode == ScreenSaverMode::Disabled) return;
    const uint32_t activity = state().lastTouchMs ? state().lastTouchMs : state().bootMs;
    const uint32_t delayMs = static_cast<uint32_t>(settings.screenSaverDelayMinutes) * 60000UL;
    if (now - activity >= delayMs) enterScreenSaver();
}

void DisplayService::enterScreenSaver() {
    if (screenSaverActive_) return;
    const uint32_t now = millis();
    if (now - lastScreenTransitionMs_ < 350U) return;
    const AppSettings& settings = settingsService().settings();
    const bool clockMode = settings.screenSaverMode == ScreenSaverMode::Clock;
    if (clockMode) {
        if (!bsp_display_lock(100)) return;
        clockScreen_.begin(settings.clockStyle);
        bsp_display_unlock();
    }
    screenSaverActivityMark_ = state().lastTouchMs;
    screenSaverClock_ = clockMode;
    screenSaverActive_ = true;
    lastScreenTransitionMs_ = now;
    state().screenSaverActive = true;
    state().displaySleeping = !screenSaverClock_;
    if (screenSaverClock_) {
        applyBrightness(settings.clockBrightness);
        appliedBrightness_ = settings.clockBrightness;
    } else {
        applyBrightness(0);
        appliedBrightness_ = 0;
    }
}

void DisplayService::leaveScreenSaver(bool rebuildPage) {
    if (!screenSaverActive_) return;
    const uint32_t now = millis();
    if (now - lastScreenTransitionMs_ < 350U) return;
    if (rebuildPage) {
        if (!bsp_display_lock(100)) return;
        showPage(activePage_, false);
        bsp_display_unlock();
    }
    screenSaverActive_ = false;
    screenSaverClock_ = false;
    lastScreenTransitionMs_ = now;
    state().screenSaverActive = false;
    state().displaySleeping = false;
    applyBrightness(settingsService().settings().displayBrightness);
    appliedBrightness_ = settingsService().settings().displayBrightness;
}

void DisplayService::requestPage(ui::Page page) {
    if (page >= ui::Page::Count) return;
    requestedPage_ = page;
    pageRequestPending_ = true;
}

void DisplayService::showPage(ui::Page page, bool animate) {
    if (page >= ui::Page::Count) return;
    activePage_ = page;
    if (page == ui::Page::Home) {
        homeScreen_.begin(navigationRequested, this, animate);
    } else if (page == ui::Page::Settings) {
        settingsScreen_.begin(navigationRequested, setupRequested, this, animate);
    } else {
        controlScreen_.begin(page, navigationRequested, this, animate);
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
