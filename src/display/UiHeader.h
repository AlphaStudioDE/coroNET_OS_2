#pragma once

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace coronet::ui {

constexpr int HeaderContentTop = 42;
constexpr int HeaderContentHeight = 208;

struct HeaderWidgets {
    lv_obj_t* wifiLabel = nullptr;
    lv_obj_t* bleLabel = nullptr;
    lv_obj_t* printerDot = nullptr;
    lv_obj_t* printerLabel = nullptr;
};

HeaderWidgets buildHeader(lv_obj_t* parent, const char* pageName);
void updateHeader(const HeaderWidgets& widgets,
                  bool wifiConnected,
                  bool bleConnected,
                  bool printerConnected);

}
