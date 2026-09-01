#pragma once

#include <stdint.h>

#include "UiNavigation.h"
#include "../core/ProductTypes.h"

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;
struct _lv_event_t;
typedef struct _lv_event_t lv_event_t;

namespace coronet {

class ControlScreen {
public:
    void begin(ui::Page page, ui::Navigation::Callback navigationCallback,
               void* callbackContext, bool animate = false);
    void update();

private:
    enum class Action : uint8_t {
        CategoryPrev, CategoryNext, AnimationPrev, AnimationNext, Preview,
        InsideStyle, Mirror, SectionNext, Brightness, DimmToggle, DimmPercent,
        RemixDefault, Remix,
        VentAuto, VentTarget, VentManual, VentTargetTemp, ManualFan, ManualFlap,
        ServoClosed, ServoOpen, ServoReverse, FanMinimum, FanMaximum,
        PandaEnabled, PandaMode, PandaTarget, PandaPreset, PandaHours,
        SoundPrev, SoundNext, SoundFilePrev, SoundFileNext, SoundVolume, SoundRepeat, SoundPlay, SoundStop,
    };

    struct Binding { ControlScreen* owner = nullptr; Action action = Action::Preview; };

    void buildHeader();
    void buildLedPage();
    void buildVentPage();
    void buildSoundPage();
    lv_obj_t* makeContent();
    lv_obj_t* makeCard(lv_obj_t* parent, int y, int height, const char* title);
    lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int width, int height,
                         const char* text, Action action);
    lv_obj_t* makeSlider(lv_obj_t* parent, int x, int y, int width,
                         int minimum, int maximum, int value, Action action);
    void handleAction(Action action, lv_event_t* event);
    void refreshLed();
    void refreshVent();
    void refreshSound();
    void refreshLedCanvas();
    static void eventHandler(lv_event_t* event);

    ui::Page page_ = ui::Page::Home;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* wifiLabel_ = nullptr;
    lv_obj_t* categoryLabel_ = nullptr;
    lv_obj_t* animationLabel_ = nullptr;
    lv_obj_t* previewCanvas_ = nullptr;
    void* previewBuffer_ = nullptr;
    lv_obj_t* insideButtonLabel_ = nullptr;
    lv_obj_t* mirrorButtonLabel_ = nullptr;
    lv_obj_t* sectionButtonLabel_ = nullptr;
    lv_obj_t* brightnessLabel_ = nullptr;
    lv_obj_t* brightnessSlider_ = nullptr;
    lv_obj_t* dimmButtonLabel_ = nullptr;
    lv_obj_t* dimmLabel_ = nullptr;
    lv_obj_t* dimmSlider_ = nullptr;
    lv_obj_t* remixLabel_ = nullptr;
    lv_obj_t* remixSlider_ = nullptr;
    lv_obj_t* ventStatusLabel_ = nullptr;
    lv_obj_t* ventModeLabels_[3] = {};
    lv_obj_t* ventTargetLabel_ = nullptr;
    lv_obj_t* ventTargetSlider_ = nullptr;
    lv_obj_t* fanLabel_ = nullptr;
    lv_obj_t* fanSlider_ = nullptr;
    lv_obj_t* flapLabel_ = nullptr;
    lv_obj_t* flapSlider_ = nullptr;
    lv_obj_t* pandaEnabledLabel_ = nullptr;
    lv_obj_t* pandaModeLabel_ = nullptr;
    lv_obj_t* pandaStatusLabel_ = nullptr;
    lv_obj_t* servoClosedLabel_ = nullptr;
    lv_obj_t* servoClosedSlider_ = nullptr;
    lv_obj_t* servoOpenLabel_ = nullptr;
    lv_obj_t* servoOpenSlider_ = nullptr;
    lv_obj_t* servoReverseLabel_ = nullptr;
    lv_obj_t* fanMinLabel_ = nullptr;
    lv_obj_t* fanMinSlider_ = nullptr;
    lv_obj_t* fanMaxLabel_ = nullptr;
    lv_obj_t* fanMaxSlider_ = nullptr;
    lv_obj_t* pandaTargetLabel_ = nullptr;
    lv_obj_t* pandaTargetSlider_ = nullptr;
    lv_obj_t* pandaPresetLabel_ = nullptr;
    lv_obj_t* pandaHoursLabel_ = nullptr;
    lv_obj_t* pandaHoursSlider_ = nullptr;
    lv_obj_t* soundScenarioLabel_ = nullptr;
    lv_obj_t* soundPathLabel_ = nullptr;
    lv_obj_t* soundVolumeLabel_ = nullptr;
    lv_obj_t* soundVolumeSlider_ = nullptr;
    lv_obj_t* soundRepeatLabel_ = nullptr;
    lv_obj_t* soundRuntimeLabel_ = nullptr;
    lv_obj_t* soundStorageLabel_ = nullptr;
    Binding bindings_[48] = {};
    uint8_t bindingCount_ = 0;
    uint8_t selectedCategory_ = 0;
    uint8_t selectedSection_ = 0;
    uint8_t selectedSound_ = 0;
    uint32_t settingsRevisionSeen_ = 0;
    uint32_t lastCanvasUpdateMs_ = 0;
    ui::Navigation navigation_;
};

}
