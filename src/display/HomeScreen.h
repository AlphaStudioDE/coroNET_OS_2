#pragma once

#include <stdint.h>

#include "../core/SystemState.h"

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace coronet {

class HomeScreen {
public:
    void begin(bool animate = false);
    void update();

private:
    struct ViewCache {
        bool wifiConnected = false;
        bool bleReady = false;
        bool bleConnected = false;
        bool printerConfigured = false;
        bool printerConnected = false;
        PrinterState printerState = PrinterState::Unknown;
        uint8_t progress = 0;
        uint8_t activeTool = 0;
        int16_t toolTempTenths = INT16_MIN;
        int16_t bedTempTenths = INT16_MIN;
        int16_t chamberTempTenths = INT16_MIN;
        char filename[65] = "";
        char status[96] = "";
    };

    void buildHeader();
    void buildPrinterPanel();
    void buildMetricCards();
    void buildNavigation();
    bool stateChanged(const ViewCache& next) const;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* wifiLabel_ = nullptr;
    lv_obj_t* bleLabel_ = nullptr;
    lv_obj_t* printerDot_ = nullptr;
    lv_obj_t* printerConnectionLabel_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* detailLabel_ = nullptr;
    lv_obj_t* progressLabel_ = nullptr;
    lv_obj_t* progressBar_ = nullptr;
    lv_obj_t* statusAccent_ = nullptr;
    lv_obj_t* toolValueLabel_ = nullptr;
    lv_obj_t* toolTempLabel_ = nullptr;
    lv_obj_t* bedTempLabel_ = nullptr;
    lv_obj_t* chamberTempLabel_ = nullptr;

    ViewCache cache_;
    bool cacheValid_ = false;
};

}
