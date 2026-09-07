#include "UiHeader.h"

#include <lvgl.h>

#include "UiTheme.h"

namespace coronet::ui {

namespace {

void styleText(lv_obj_t* object, uint32_t color, const lv_font_t* font) {
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    uint32_t color,
                    const lv_font_t* font,
                    int x,
                    int y,
                    int width = LV_SIZE_CONTENT) {
    lv_obj_t* label = lv_label_create(parent);
    styleText(label, color, font);
    if (width != LV_SIZE_CONTENT) lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

}

HeaderWidgets buildHeader(lv_obj_t* parent, const char* pageName) {
    HeaderWidgets widgets;

    makeLabel(parent, "coroNET", ColorText, &lv_font_montserrat_22, 14, 4);
    makeLabel(parent, pageName, ColorCyan, &lv_font_montserrat_10, 123, 13);

    widgets.wifiLabel = makeLabel(parent, LV_SYMBOL_WIFI, ColorMuted,
                                  &lv_font_montserrat_16, 340, 7, 26);
    lv_obj_set_style_text_align(widgets.wifiLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    widgets.bleLabel = makeLabel(parent, "BT", ColorMuted,
                                 &lv_font_montserrat_12, 376, 10, 28);
    lv_obj_set_style_text_align(widgets.bleLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    widgets.printerDot = lv_obj_create(parent);
    lv_obj_set_size(widgets.printerDot, 8, 8);
    lv_obj_set_pos(widgets.printerDot, 416, 13);
    lv_obj_set_style_radius(widgets.printerDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(widgets.printerDot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(widgets.printerDot, lv_color_hex(ColorMuted), LV_PART_MAIN);
    lv_obj_set_style_pad_all(widgets.printerDot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(widgets.printerDot, LV_OBJ_FLAG_SCROLLABLE);

    widgets.printerLabel = makeLabel(parent, "PRN", ColorMuted,
                                     &lv_font_montserrat_10, 430, 11, 34);

    lv_obj_t* divider = lv_obj_create(parent);
    lv_obj_set_size(divider, 452, 1);
    lv_obj_set_pos(divider, 14, 38);
    lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(divider, lv_color_hex(ColorBorder), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    return widgets;
}

void updateHeader(const HeaderWidgets& widgets,
                  bool wifiConnected,
                  bool bleConnected,
                  bool printerConnected) {
    lv_obj_set_style_text_color(widgets.wifiLabel,
                                lv_color_hex(wifiConnected ? ColorCyan : ColorMuted),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(widgets.bleLabel,
                                lv_color_hex(bleConnected ? ColorCyan : ColorMuted),
                                LV_PART_MAIN);
    const uint32_t printerColor = printerConnected ? ColorGreen : ColorMuted;
    lv_obj_set_style_bg_color(widgets.printerDot, lv_color_hex(printerColor), LV_PART_MAIN);
    lv_obj_set_style_text_color(widgets.printerLabel, lv_color_hex(printerColor), LV_PART_MAIN);
}

}
