#pragma once

#include <time.h>

#include <lvgl.h>

#include "../core/ProductTypes.h"

namespace coronet {

class ClockScreen {
public:
    void begin(ClockStyle style);
    void update();

private:
    void buildDigital();
    void buildRetro();
    void buildAnalog();
    void buildLinear();
    void buildBauhaus();
    void buildMatrix();
    void buildArc();
    void updateRetro(const struct tm* local);
    static void touchEvent(lv_event_t* event);

    ClockStyle style_ = ClockStyle::Digital;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* timeLabel_ = nullptr;
    lv_obj_t* secondsLabel_ = nullptr;
    lv_obj_t* dateLabel_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* retroPanel_ = nullptr;
    lv_obj_t* retroSegments_[4][7] = {};
    lv_obj_t* retroColon_[2] = {};
    lv_obj_t* retroSuffixLabel_ = nullptr;
    char retroLastDigits_[5] = {};
    char retroLastSuffix_[3] = {};
    bool retroColonVisible_ = false;
    lv_obj_t* bars_[3] = {};
    lv_obj_t* indicators_[3] = {};
    lv_obj_t* circles_[3] = {};
    lv_point_t handPoints_[3][2] = {};
    uint32_t lastSecond_ = UINT32_MAX;
};

}
