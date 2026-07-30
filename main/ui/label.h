#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);

void ui_label_set(lv_obj_t *label, char *cache, size_t cap, const char *text);

#ifdef __cplusplus
}
#endif
