#include "DisplayService.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

#include "../config/AppConfig.h"
#include "../boot/BootExperience.h"
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
    cfg.lvgl_port_cfg.task_stack = 6144;
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
        bootScreen_.begin();
        bootActive_ = true;
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
    if (bootActive_) {
        if (now - lastUiUpdateMs_ < 30U) return;
        lastUiUpdateMs_ = now;
        if (bsp_display_lock(5)) {
            bootScreen_.update();
            if (!bootExperience().active()) {
                bootActive_ = false;
                if (settingsService().settings().setupDone) {
                    showPage(ui::Page::Home, true);
                } else {
                    setupWizard_.begin(true);
                    wizardActive_ = true;
                }
            }
            bsp_display_unlock();
        }
        return;
    }
    const OtaState otaState = state().otaState;
    const bool otaModal = otaState == OtaState::Preparing ||
                          otaState == OtaState::Downloading ||
                          otaState == OtaState::Installing ||
                          otaState == OtaState::Success;
    if (otaModal && screenSaverActive_) {
        state().lastTouchMs = now;
        leaveScreenSaver(true);
    }
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
        updateOtaOverlay();
        if (otaOverlay_) {
            bsp_display_unlock();
            return;
        } else if (wizardActive_) {
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

void DisplayService::updateOtaOverlay() {
    const SystemState& system = state();
    const bool visible = system.otaState == OtaState::Preparing ||
                         system.otaState == OtaState::Downloading ||
                         system.otaState == OtaState::Installing ||
                         system.otaState == OtaState::Success;
    if (!visible) {
        if (otaOverlay_) lv_obj_del(otaOverlay_);
        otaOverlay_ = nullptr;
        otaOverlayStatus_ = nullptr;
        otaOverlayProgress_ = nullptr;
        otaOverlayPercent_ = nullptr;
        otaOverlayStateSeen_ = system.otaState;
        otaOverlayProgressSeen_ = system.otaProgress;
        return;
    }

    if (!otaOverlay_) {
        otaOverlay_ = lv_obj_create(lv_layer_top());
        lv_obj_set_size(otaOverlay_, 480, 320);
        lv_obj_set_pos(otaOverlay_, 0, 0);
        lv_obj_clear_flag(otaOverlay_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(otaOverlay_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(otaOverlay_, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(otaOverlay_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(otaOverlay_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(otaOverlay_, lv_color_hex(ui::ColorBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(otaOverlay_, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t* brand = lv_label_create(otaOverlay_);
        lv_label_set_text(brand, "coroNET");
        lv_obj_set_style_text_font(brand, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(brand, lv_color_hex(ui::ColorCyan), LV_PART_MAIN);
        lv_obj_set_pos(brand, 24, 22);

        lv_obj_t* title = lv_label_create(otaOverlay_);
        lv_label_set_text(title, "FIRMWARE UPDATE");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(ui::ColorText), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 72);

        otaOverlayStatus_ = lv_label_create(otaOverlay_);
        lv_obj_set_width(otaOverlayStatus_, 420);
        lv_obj_set_style_text_align(otaOverlayStatus_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(otaOverlayStatus_, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(otaOverlayStatus_, lv_color_hex(ui::ColorMuted), LV_PART_MAIN);
        lv_obj_align(otaOverlayStatus_, LV_ALIGN_TOP_MID, 0, 116);

        otaOverlayProgress_ = lv_bar_create(otaOverlay_);
        lv_obj_set_size(otaOverlayProgress_, 400, 12);
        lv_obj_align(otaOverlayProgress_, LV_ALIGN_TOP_MID, 0, 154);
        lv_bar_set_range(otaOverlayProgress_, 0, 100);
        lv_obj_set_style_radius(otaOverlayProgress_, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(otaOverlayProgress_, 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(otaOverlayProgress_, lv_color_hex(ui::ColorSurfaceRaised), LV_PART_MAIN);
        lv_obj_set_style_bg_color(otaOverlayProgress_, lv_color_hex(ui::ColorCyan), LV_PART_INDICATOR);

        otaOverlayPercent_ = lv_label_create(otaOverlay_);
        lv_obj_set_style_text_font(otaOverlayPercent_, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(otaOverlayPercent_, lv_color_hex(ui::ColorText), LV_PART_MAIN);
        lv_obj_align(otaOverlayPercent_, LV_ALIGN_TOP_MID, 0, 180);

        lv_obj_t* warning = lv_label_create(otaOverlay_);
        lv_label_set_text(warning, "Keep coroNET powered on. The device will restart automatically.");
        lv_obj_set_width(warning, 420);
        lv_obj_set_style_text_align(warning, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(warning, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(warning, lv_color_hex(ui::ColorMuted), LV_PART_MAIN);
        lv_obj_align(warning, LV_ALIGN_BOTTOM_MID, 0, -48);
    }

    if (otaOverlayStateSeen_ != system.otaState ||
        otaOverlayProgressSeen_ != system.otaProgress) {
        lv_label_set_text(otaOverlayStatus_, system.otaStatusText);
        lv_bar_set_value(otaOverlayProgress_, system.otaProgress, LV_ANIM_OFF);
        lv_label_set_text_fmt(otaOverlayPercent_, "%u%%", static_cast<unsigned>(system.otaProgress));
        otaOverlayStateSeen_ = system.otaState;
        otaOverlayProgressSeen_ = system.otaProgress;
    }
    lv_obj_move_foreground(otaOverlay_);
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
    if (!started_ || !wasApplied || bootActive_ || wizardActive_ || state().displaySleeping) return;
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
    const SystemState& system = state();
    if (wizardActive_) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
        return;
    }
    const bool printerError = system.printerTelemetryValid && system.printerState == PrinterState::Error;
    const bool newPrinterEvent = system.printerStateEventSequence != observedPrinterEventSequence_;
    if (newPrinterEvent) {
        observedPrinterEventSequence_ = system.printerStateEventSequence;
    }
    if (newPrinterEvent && system.printerEventTo == PrinterState::Error) {
        state().lastTouchMs = now;
        if (screenSaverActive_) leaveScreenSaver(true);
    }
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
    if (page >= ui::Page::Count || bootActive_) return;
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
