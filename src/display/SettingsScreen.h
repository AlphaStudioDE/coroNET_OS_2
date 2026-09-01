#pragma once

#include <stdint.h>

#include "UiNavigation.h"
#include "../core/ProductTypes.h"

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
        SkinNext,
        ColorModeNext,
        AccentHue,
        SaverModeNext,
        ClockStyleNext,
        SaverDelay,
        ClockBrightness,
        QuietTargetNext,
        QuietDuration,
        QuietErrorsBypass,
        OtaCheck,
        OtaInstall,
        OtaReinstall,
        OtaSdRecovery,
        FactoryReset,
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
    void buildQuietCard(lv_obj_t* parent, int y);
    void buildSystemCard(lv_obj_t* parent, int y);
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
    lv_obj_t* skinButtonLabel_ = nullptr;
    lv_obj_t* colorModeButtonLabel_ = nullptr;
    lv_obj_t* accentLabel_ = nullptr;
    lv_obj_t* accentSlider_ = nullptr;
    lv_obj_t* saverModeButtonLabel_ = nullptr;
    lv_obj_t* clockStyleButtonLabel_ = nullptr;
    lv_obj_t* saverDelayLabel_ = nullptr;
    lv_obj_t* saverDelaySlider_ = nullptr;
    lv_obj_t* clockBrightnessLabel_ = nullptr;
    lv_obj_t* clockBrightnessSlider_ = nullptr;
    lv_obj_t* quietTargetButtonLabel_ = nullptr;
    lv_obj_t* quietDurationLabel_ = nullptr;
    lv_obj_t* quietDurationSlider_ = nullptr;
    lv_obj_t* quietErrorsButtonLabel_ = nullptr;
    lv_obj_t* otaStatusLabel_ = nullptr;
    lv_obj_t* otaVersionLabel_ = nullptr;
    lv_obj_t* otaInstallButton_ = nullptr;
    lv_obj_t* factoryResetButtonLabel_ = nullptr;
    ActionBinding actionBindings_[20] = {};
    ui::Navigation navigation_;
    SetupCallback setupCallback_ = nullptr;
    void* callbackContext_ = nullptr;
    uint32_t settingsRevisionSeen_ = 0;
    bool wifiConnectedSeen_ = false;
    bool bleConnectedSeen_ = false;
    bool printerConnectedSeen_ = false;
    OtaState otaStateSeen_ = OtaState::Idle;
    uint8_t otaProgressSeen_ = 0;
    uint32_t factoryConfirmUntilMs_ = 0;
    bool cacheValid_ = false;
};

}
