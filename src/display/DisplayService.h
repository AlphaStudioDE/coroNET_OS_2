#pragma once

#include <stdint.h>

#include "HomeScreen.h"
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
    void applyBrightness(uint8_t percent);
    static void navigationRequested(ui::Page page, void* context);
    static void setupRequested(void* context);

    HomeScreen homeScreen_;
    SettingsScreen settingsScreen_;
    SetupWizard setupWizard_;
    ui::Page activePage_ = ui::Page::Home;
    ui::Page requestedPage_ = ui::Page::Home;
    bool pageRequestPending_ = false;
    bool started_ = false;
    bool wizardActive_ = false;
    uint8_t appliedBrightness_ = 255;
    uint32_t lastUiUpdateMs_ = 0;
};

}
