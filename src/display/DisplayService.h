#pragma once

#include <stdint.h>

#include "HomeScreen.h"
#include "ControlScreen.h"
#include "ClockScreen.h"
#include "SettingsScreen.h"
#include "SetupWizard.h"

namespace coronet {

class DisplayService {
public:
    void begin();
    void loop();
    void requestPage(ui::Page page);

private:
    void showPage(ui::Page page, bool animate = false);
    void reopenSetupWizard();
    void updateTimeService(uint32_t now);
    void updateTheme();
    void updateScreenSaver(uint32_t now);
    void enterScreenSaver();
    void leaveScreenSaver(bool rebuildPage);
    void applyBrightness(uint8_t percent);
    static void navigationRequested(ui::Page page, void* context);
    static void setupRequested(void* context);

    HomeScreen homeScreen_;
    ControlScreen controlScreen_;
    ClockScreen clockScreen_;
    SettingsScreen settingsScreen_;
    SetupWizard setupWizard_;
    ui::Page activePage_ = ui::Page::Home;
    ui::Page requestedPage_ = ui::Page::Home;
    bool pageRequestPending_ = false;
    bool started_ = false;
    bool wizardActive_ = false;
    bool screenSaverActive_ = false;
    bool screenSaverClock_ = false;
    uint32_t observedPrinterEventSequence_ = 0;
    uint8_t appliedBrightness_ = 255;
    uint32_t lastUiUpdateMs_ = 0;
    uint32_t screenSaverActivityMark_ = 0;
    uint32_t lastScreenTransitionMs_ = 0;
    uint32_t lastTimeSyncRequestMs_ = 0;
    char configuredTimeZone_[41] = "";
    uint32_t appliedThemeSignature_ = UINT32_MAX;
};

}
