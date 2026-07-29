#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_WEATHER_ICON_BASE 56

lv_obj_t *ui_weather_icon_create(lv_obj_t *parent, int32_t size);

void ui_weather_icon_set_code(lv_obj_t *icon, int code);

#ifdef __cplusplus
}
#endif
