#include "menu_card.h"
#include "theme.h"

#define ICON_TILE_SIZE 48

#define CHILD_ICON_TILE 0
#define CHILD_CHEVRON   2

#define ICON_TILE_TINT  46

static uint32_t card_accent(lv_obj_t *card)
{
    return (uint32_t)(uintptr_t)lv_obj_get_user_data(card);
}

lv_obj_t *ui_menu_card_create(lv_obj_t *parent, const char *icon, const char *label,
                              uint32_t accent, lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_add_style(card, ui_style_card_pressed(), LV_STATE_PRESSED);
    lv_obj_set_user_data(card, (void *)(uintptr_t)accent);

    lv_obj_set_size(card, LV_PCT(100), UI_MENU_CARD_HEIGHT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *tile = lv_obj_create(card);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, ICON_TILE_SIZE, ICON_TILE_SIZE);
    lv_obj_set_style_radius(tile, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon_label = lv_label_create(tile);
    lv_label_set_text(icon_label, icon);
    lv_obj_center(icon_label);
    lv_obj_remove_flag(icon_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *text = lv_label_create(card);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_font(text, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(text, ui_color(UI_COLOR_TEXT), 0);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_remove_flag(text, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *chevron = lv_label_create(card);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chevron, ui_color(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_remove_flag(chevron, LV_OBJ_FLAG_CLICKABLE);

    if (on_click) {
        lv_obj_add_event_cb(card, on_click, LV_EVENT_CLICKED, user_data);
    }

    ui_menu_card_set_selected(card, false);
    return card;
}

void ui_menu_card_set_selected(lv_obj_t *card, bool selected)
{
    const lv_color_t accent = ui_color(card_accent(card));

    lv_obj_t *tile = lv_obj_get_child(card, CHILD_ICON_TILE);
    lv_obj_t *icon_label = lv_obj_get_child(tile, 0);
    lv_obj_t *chevron = lv_obj_get_child(card, CHILD_CHEVRON);

    if (selected) {
        lv_obj_add_style(card, ui_style_card_selected(), 0);
        lv_obj_set_style_bg_color(tile, accent, 0);
        lv_obj_set_style_text_color(icon_label, ui_color(UI_COLOR_BG), 0);
        lv_obj_set_style_text_color(chevron, ui_color(UI_COLOR_ACCENT), 0);
    }
    else {
        lv_obj_remove_style(card, ui_style_card_selected(), 0);
        lv_obj_set_style_bg_color(tile, lv_color_mix(accent, ui_color(UI_COLOR_SURFACE),
                                                     ICON_TILE_TINT), 0);
        lv_obj_set_style_text_color(icon_label, accent, 0);
        lv_obj_set_style_text_color(chevron, ui_color(UI_COLOR_TEXT_MUTED), 0);
    }
}
