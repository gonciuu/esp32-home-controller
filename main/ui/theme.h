#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_COLOR_BG           0x0F1115
#define UI_COLOR_SURFACE      0x181B21
#define UI_COLOR_SURFACE_HI   0x232830
#define UI_COLOR_BORDER       0x2A2F38
#define UI_COLOR_TEXT         0xF2F4F8
#define UI_COLOR_TEXT_MUTED   0x8A93A3
#define UI_COLOR_ACCENT       0x4C8DFF
#define UI_COLOR_ACCENT_SOFT  0x1B2942

#define UI_COLOR_UP           0x3DD68C
#define UI_COLOR_DOWN         0xFF5C5C

#define UI_COLOR_BTC          0xF7931A

#define UI_COLOR_SUN          0xFFB020
#define UI_COLOR_CLOUD        0x9AA6BA
#define UI_COLOR_RAIN         0x35C6E8
#define UI_COLOR_SNOW         0xE8F0FA

#define UI_SPACE_XS   4
#define UI_SPACE_SM   8
#define UI_SPACE_MD  12
#define UI_SPACE_LG  16
#define UI_SPACE_XL  20

#define UI_RADIUS_SM 10
#define UI_RADIUS_MD 16
#define UI_RADIUS_LG 20

LV_FONT_DECLARE(ui_font_14);
LV_FONT_DECLARE(ui_font_18);
LV_FONT_DECLARE(ui_font_22);
LV_FONT_DECLARE(ui_font_28);

#define UI_FONT_CLOCK   (&lv_font_montserrat_48)
#define UI_FONT_TITLE   (&ui_font_28)
#define UI_FONT_HEADING (&ui_font_22)
#define UI_FONT_BODY    (&ui_font_18)
#define UI_FONT_CAPTION (&ui_font_14)

#define UI_ANIM_TIME_MS 80

static inline lv_color_t ui_color(uint32_t hex)
{
    return lv_color_hex(hex);
}

void ui_theme_init(void);

lv_style_t *ui_style_screen(void);
lv_style_t *ui_style_surface(void);
lv_style_t *ui_style_card(void);
lv_style_t *ui_style_card_pressed(void);
lv_style_t *ui_style_transparent(void);

#ifdef __cplusplus
}
#endif
