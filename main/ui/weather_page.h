#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_weather_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data);

void ui_weather_page_select_today(void);

#ifdef __cplusplus
}
#endif
