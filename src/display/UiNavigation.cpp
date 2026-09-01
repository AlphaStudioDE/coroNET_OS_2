#include "UiNavigation.h"

#include <lvgl.h>

#include "UiTheme.h"

namespace coronet::ui {

namespace {

constexpr bool pageAvailable(Page page) {
    return page < Page::Count;
}

void styleButton(lv_obj_t* button, bool active) {
    lv_obj_set_style_radius(button, CornerRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button,
                                  lv_color_hex(active ? ColorCyan : ColorBorder),
                                  LV_PART_MAIN);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(active ? ColorSurfaceRaised : ColorBackground),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, active ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
}

}

void Navigation::build(lv_obj_t* parent, Page activePage, Callback callback, void* context) {
    callback_ = callback;
    callbackContext_ = context;

    lv_obj_t* nav = lv_obj_create(parent);
    lv_obj_set_size(nav, ScreenWidth, 66);
    lv_obj_set_pos(nav, 0, 254);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(nav, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(nav, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(nav, lv_color_hex(ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(nav, 0, LV_PART_MAIN);

    static const char* const Labels[static_cast<uint8_t>(Page::Count)] = {
        LV_SYMBOL_HOME "\nHOME",
        LV_SYMBOL_CHARGE "\nLED",
        LV_SYMBOL_REFRESH "\nVENT",
        LV_SYMBOL_AUDIO "\nSOUND",
        LV_SYMBOL_SETTINGS "\nSET",
    };

    for (uint8_t index = 0; index < static_cast<uint8_t>(Page::Count); ++index) {
        const Page page = static_cast<Page>(index);
        const bool active = page == activePage;
        lv_obj_t* button = lv_btn_create(nav);
        lv_obj_set_size(button, 86, 50);
        lv_obj_set_pos(button, 9 + index * 94, 8);
        styleButton(button, active);

        bindings_[index].owner = this;
        bindings_[index].page = page;
        lv_obj_add_event_cb(button, buttonEvent, LV_EVENT_CLICKED, &bindings_[index]);

        if (!pageAvailable(page)) {
            lv_obj_add_state(button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(button, LV_OPA_50, LV_PART_MAIN);
        }

        lv_obj_t* label = lv_label_create(button);
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(active ? ColorCyan : ColorMuted),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(label, 1, LV_PART_MAIN);
        lv_label_set_text(label, Labels[index]);
        lv_obj_center(label);
    }
}

void Navigation::buttonEvent(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    Binding* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (!binding || !binding->owner || !binding->owner->callback_) return;
    binding->owner->callback_(binding->page, binding->owner->callbackContext_);
}

}
