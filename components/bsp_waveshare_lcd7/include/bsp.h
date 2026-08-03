#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up the whole display stack: I2C, backlight, RGB panel, GT911 touch and
 * the LVGL port. When it returns, the LVGL task is running and the screen is on. */
esp_err_t bsp_display_start(void);

/* Guard every lv_* call with these. timeout_ms == 0 waits forever. */
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

/* The backlight is a single expander bit, so this is on/off — there is no PWM. */
esp_err_t bsp_display_backlight(bool on);

#ifdef __cplusplus
}
#endif
