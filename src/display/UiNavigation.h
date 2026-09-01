#pragma once

#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;
struct _lv_event_t;
typedef struct _lv_event_t lv_event_t;

namespace coronet::ui {

enum class Page : uint8_t {
    Home = 0,
    Led,
    Vent,
    Sound,
    Settings,
    Count,
};

class Navigation {
public:
    using Callback = void (*)(Page page, void* context);

    void build(lv_obj_t* parent, Page activePage, Callback callback, void* context);

private:
    struct Binding {
        Navigation* owner = nullptr;
        Page page = Page::Home;
    };

    static void buttonEvent(lv_event_t* event);

    Callback callback_ = nullptr;
    void* callbackContext_ = nullptr;
    Binding bindings_[static_cast<uint8_t>(Page::Count)] = {};
};

}
