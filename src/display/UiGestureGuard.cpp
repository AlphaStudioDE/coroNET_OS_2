#include "UiGestureGuard.h"

#include <stdlib.h>
#include <lvgl.h>

namespace coronet::ui {

namespace {

constexpr int kGestureThreshold = 10;
constexpr uint8_t kAxisPending = 0;
constexpr uint8_t kAxisHorizontal = 1;
constexpr uint8_t kAxisVertical = 2;

lv_indev_t* activeInput() {
    return lv_indev_get_act();
}

void classifyGesture(SliderGestureState& state) {
    if (!state.active || state.axis != kAxisPending) return;
    lv_indev_t* input = activeInput();
    if (!input) return;
    if (lv_indev_get_scroll_obj(input)) {
        state.axis = kAxisVertical;
        return;
    }

    lv_point_t point{};
    lv_indev_get_point(input, &point);
    const int dx = abs(static_cast<int>(point.x) - state.startX);
    const int dy = abs(static_cast<int>(point.y) - state.startY);

    if (dy >= kGestureThreshold && dy * 4 >= dx * 5) {
        state.axis = kAxisVertical;
    } else if (dx >= kGestureThreshold && dx * 4 >= dy * 5) {
        state.axis = kAxisHorizontal;
    }
}

void restoreStartValue(lv_obj_t* slider, const SliderGestureState& state) {
    if (lv_slider_get_value(slider) != state.startValue) {
        lv_slider_set_value(slider, state.startValue, LV_ANIM_OFF);
    }
}

}

void enableVerticalScrollFromSlider(lv_obj_t* slider) {
    lv_obj_add_flag(slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
}

SliderGestureResult processSliderGesture(lv_event_t* event,
                                         lv_obj_t* slider,
                                         SliderGestureState& state) {
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        lv_indev_t* input = activeInput();
        lv_point_t point{};
        if (input) lv_indev_get_point(input, &point);
        state.startX = point.x;
        state.startY = point.y;
        state.startValue = lv_slider_get_value(slider);
        state.axis = kAxisPending;
        state.active = true;
        return SliderGestureResult::Ignore;
    }

    if (!state.active) return SliderGestureResult::Ignore;
    classifyGesture(state);

    if (code == LV_EVENT_PRESSING) {
        if (state.axis == kAxisVertical) restoreStartValue(slider, state);
        return SliderGestureResult::Ignore;
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        if (state.axis == kAxisVertical) {
            restoreStartValue(slider, state);
            return SliderGestureResult::Cancel;
        }
        return state.axis == kAxisHorizontal
                   ? SliderGestureResult::Preview
                   : SliderGestureResult::Ignore;
    }
    if (code == LV_EVENT_RELEASED) {
        lv_indev_t* input = activeInput();
        const bool scrolled = input && lv_indev_get_scroll_obj(input);
        if (scrolled || state.axis == kAxisVertical) {
            restoreStartValue(slider, state);
            state.active = false;
            return SliderGestureResult::Cancel;
        }
        state.active = false;
        return SliderGestureResult::Commit;
    }
    if (code == LV_EVENT_PRESS_LOST) {
        restoreStartValue(slider, state);
        state.active = false;
        return SliderGestureResult::Cancel;
    }
    return SliderGestureResult::Ignore;
}

}
