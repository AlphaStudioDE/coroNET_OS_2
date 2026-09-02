#pragma once

#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace coronet {

class BootScreen {
public:
    void begin();
    void update();

private:
    void updateFull(uint32_t elapsedMs);
    void updateQuick(uint32_t elapsedMs);
    void updatePrelude(uint32_t elapsedMs);
    void setLogoColor(uint32_t rgb);
    void setOpacity(uint8_t logo, uint8_t wordmark, uint8_t detail);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* arc_ = nullptr;
    lv_obj_t* core_ = nullptr;
    lv_obj_t* horizon_ = nullptr;
    lv_obj_t* endpoint_ = nullptr;
    lv_obj_t* coroLabel_ = nullptr;
    lv_obj_t* netLabel_ = nullptr;
    lv_obj_t* descriptorLabel_ = nullptr;
    lv_obj_t* editionLabel_ = nullptr;
    lv_obj_t* featureLabel_ = nullptr;
    lv_obj_t* authorLabel_ = nullptr;
    lv_obj_t* glowLine_ = nullptr;
    int8_t featureIndex_ = -1;
};

}
