#pragma once

#include <stdint.h>

struct _lv_event_t;
typedef struct _lv_event_t lv_event_t;
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace coronet::ui {

enum class SliderGestureResult : uint8_t {
    Ignore,
    Preview,
    Commit,
    Cancel,
};

struct SliderGestureState {
    int16_t startX = 0;
    int16_t startY = 0;
    int32_t startValue = 0;
    uint8_t axis = 0;
    bool active = false;
};

void enableVerticalScrollFromSlider(lv_obj_t* slider);
SliderGestureResult processSliderGesture(lv_event_t* event,
                                         lv_obj_t* slider,
                                         SliderGestureState& state);

}
