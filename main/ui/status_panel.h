#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_STATUS_PANEL_WIDTH 360

lv_obj_t *ui_status_panel_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
