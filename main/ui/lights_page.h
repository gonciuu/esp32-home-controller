#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_lights_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data);

void ui_lights_bind_tile(lv_obj_t *card);

void ui_lights_page_refresh(void);

#ifdef __cplusplus
}
#endif
