#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_sysinfo_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data);

/* Drives the System card's subtitle with the live CPU load and SoC temperature. */
void ui_sysinfo_bind_tile(lv_obj_t *card);

#ifdef __cplusplus
}
#endif
