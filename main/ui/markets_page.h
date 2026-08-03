#pragma once

#include "lvgl.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_markets_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data);

void ui_markets_bind_tile(lv_obj_t *card);

void ui_markets_format_price(float value, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
