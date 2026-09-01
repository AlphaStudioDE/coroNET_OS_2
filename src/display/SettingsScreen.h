#pragma once

#include <stdint.h>

#include "UiNavigation.h"

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;
struct _lv_event_t;
typedef struct _lv_event_t lv_event_t;

namespace coronet {

class SettingsScreen {
public:
    using SetupCallback = void (*)(void* context);

    void begin(ui::Navigation::Callback navigationCallback,
               SetupCallback setupCallback,
               void* callbackContext,
               bool animate = false);
    void update();

private:
    enum class Action : uint8_t {
        TransportAuto,
        TransportBle,
        TransportWifi,
        Brightness,
        Reconfigure,
    };

    struct ActionBinding {
        SettingsScreen* owner = nullptr;
        Action action = Action::TransportAuto;
    };

    void buildHeader();
    void buildContent();
    void buildConnectionCard(lv_obj_t* parent, int y);
    void buildDeviceCard(lv_obj_t* parent, int y);
    void buildSetupCard(lv_obj_t* parent, int y);
    void buildAppearanceCard(lv_obj_t* parent, int y);
    void refreshTransportButtons();
    void handleAction(Action action, lv_event_t* event);
    static void actionEvent(lv_event_t* event);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* wifiLabel_ = nullptr;
    lv_obj_t* bleLabel_ = nullptr;
    lv_obj_t* transportButtons_[3] = {};
    lv_obj_t* connectionDetailLabel_ = nullptr;
    lv_obj_t* deviceNameLabel_ = nullptr;
    lv_obj_t* brightnessLabel_ = nullptr;
    lv_obj_t* brightnessSlider_ = nullptr;
    lv_obj_t* networkValueLabel_ = nullptr;
    lv_obj_t* printerValueLabel_ = nullptr;
    ActionBinding actionBindings_[5] = {};
    ui::Navigation navigation_;
    SetupCallback setupCallback_ = nullptr;
    void* callbackContext_ = nullptr;
    uint32_t settingsRevisionSeen_ = 0;
    bool wifiConnectedSeen_ = false;
    bool bleConnectedSeen_ = false;
    bool printerConnectedSeen_ = false;
    bool cacheValid_ = false;
};

}
