#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_page_create(lv_obj_t *parent, const char *title, lv_event_cb_t on_back,
                         void *user_data);

#ifdef __cplusplus
}
#endif
