#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_TILE_CARD_WIDTH  245
#define UI_TILE_CARD_HEIGHT 178

lv_obj_t *ui_tile_card_create(lv_obj_t *parent, const char *icon, const char *label,
                              const char *info, uint32_t accent, lv_event_cb_t on_click,
                              void *user_data);

lv_obj_t *ui_tile_card_icon_tile(lv_obj_t *card);
lv_obj_t *ui_tile_card_info_label(lv_obj_t *card);

#ifdef __cplusplus
}
#endif
