#pragma once

#include <stdint.h>

#include "HomeScreen.h"
#include "SetupWizard.h"

namespace coronet {

class DisplayService {
public:
    void begin();
    void loop();

private:
    void buildHomeScreen(bool animate = false);
    void applyBrightness(uint8_t percent);

    HomeScreen homeScreen_;
    SetupWizard setupWizard_;
    bool started_ = false;
    bool wizardActive_ = false;
    uint8_t appliedBrightness_ = 255;
    uint32_t lastUiUpdateMs_ = 0;
};

}
