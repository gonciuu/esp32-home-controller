#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_TOP_BAR_HEIGHT 56

lv_obj_t *ui_top_bar_create(lv_obj_t *parent, lv_event_cb_t on_weather_click, void *user_data);

#ifdef __cplusplus
}
#endif
