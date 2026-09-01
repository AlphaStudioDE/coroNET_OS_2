#pragma once

#include <lvgl.h>

#include "../core/ProductTypes.h"

namespace coronet {

class ClockScreen {
public:
    void begin(ClockStyle style);
    void update();

private:
    void buildDigital(bool retro);
    void buildAnalog();
    void buildLinear();
    void buildBauhaus();
    void buildMatrix();
    void buildArc();
    static void touchEvent(lv_event_t* event);

    ClockStyle style_ = ClockStyle::Digital;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* timeLabel_ = nullptr;
    lv_obj_t* secondsLabel_ = nullptr;
    lv_obj_t* dateLabel_ = nullptr;
    lv_obj_t* bars_[3] = {};
    lv_obj_t* indicators_[3] = {};
    lv_obj_t* circles_[3] = {};
    lv_point_t handPoints_[3][2] = {};
    uint32_t lastSecond_ = UINT32_MAX;
};

}
