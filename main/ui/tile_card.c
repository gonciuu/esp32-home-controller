#include "tile_card.h"
#include "theme.h"

#define ICON_TILE_SIZE 56
#define ICON_TILE_TINT 46

lv_obj_t *ui_tile_card_create(lv_obj_t *parent, const char *icon, const char *label,
                              const char *info, uint32_t accent, lv_event_cb_t on_click,
                              void *user_data)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_add_style(card, ui_style_card_pressed(), LV_STATE_PRESSED);

    lv_obj_set_size(card, UI_TILE_CARD_WIDTH, UI_TILE_CARD_HEIGHT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, UI_SPACE_XS, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *tile = lv_obj_create(card);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, ICON_TILE_SIZE, ICON_TILE_SIZE);
    lv_obj_set_style_radius(tile, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_color(tile, lv_color_mix(ui_color(accent), ui_color(UI_COLOR_SURFACE),
                                                 ICON_TILE_TINT), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon_label = lv_label_create(tile);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, UI_FONT_HEADING, 0);
    lv_obj_set_style_text_color(icon_label, ui_color(accent), 0);
    lv_obj_center(icon_label);
    lv_obj_remove_flag(icon_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, label);
    lv_obj_set_style_text_font(name, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(name, ui_color(UI_COLOR_TEXT), 0);
    lv_obj_remove_flag(name, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *subtitle = lv_label_create(card);
    lv_label_set_text(subtitle, info);
    lv_obj_set_style_text_font(subtitle, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(subtitle, ui_color(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_remove_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);

    if (on_click) {
        lv_obj_add_event_cb(card, on_click, LV_EVENT_CLICKED, user_data);
    }

    return card;
}
