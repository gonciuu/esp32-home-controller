#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_MENU_CARD_HEIGHT 68

lv_obj_t *ui_menu_card_create(lv_obj_t *parent, const char *icon, const char *label,
                              uint32_t accent, lv_event_cb_t on_click, void *user_data);

void ui_menu_card_set_selected(lv_obj_t *card, bool selected);

#ifdef __cplusplus
}
#endif
