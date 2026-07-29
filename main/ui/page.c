#include "page.h"
#include "theme.h"

lv_obj_t *ui_page_create(lv_obj_t *parent, const char *title)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_add_style(page, ui_style_transparent(), 0);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(page, UI_SPACE_XL, 0);
    lv_obj_set_style_pad_row(page, UI_SPACE_SM, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *heading = lv_label_create(page);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_font(heading, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(heading, ui_color(UI_COLOR_TEXT), 0);

    lv_obj_t *placeholder = lv_label_create(page);
    lv_label_set_text(placeholder, "Nothing here yet");
    lv_obj_set_style_text_font(placeholder, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(placeholder, ui_color(UI_COLOR_TEXT_MUTED), 0);

    return page;
}
