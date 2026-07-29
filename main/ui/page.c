#include "page.h"
#include "theme.h"

#define BACK_HEIGHT 44

static lv_obj_t *create_back(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data)
{
    lv_obj_t *back = lv_obj_create(parent);
    lv_obj_remove_style_all(back);
    lv_obj_add_style(back, ui_style_transparent(), 0);
    lv_obj_set_size(back, LV_SIZE_CONTENT, BACK_HEIGHT);
    lv_obj_set_style_pad_right(back, UI_SPACE_LG, 0);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_text_font(back, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(back, ui_color(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_color(back, ui_color(UI_COLOR_ACCENT), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(back);
    lv_label_set_text(label, LV_SYMBOL_LEFT "  Back");
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);

    if (on_back) {
        lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, user_data);
    }

    return back;
}

lv_obj_t *ui_page_create(lv_obj_t *parent, const char *title, lv_event_cb_t on_back,
                         void *user_data)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_add_style(page, ui_style_surface(), 0);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(page, UI_SPACE_XL, 0);
    lv_obj_set_style_pad_row(page, UI_SPACE_LG, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    create_back(page, on_back, user_data);

    lv_obj_t *heading = lv_label_create(page);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_font(heading, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(heading, ui_color(UI_COLOR_TEXT), 0);

    return page;
}
