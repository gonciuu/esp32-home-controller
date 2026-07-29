#include "theme.h"

static bool s_inited;

static lv_style_t s_screen;
static lv_style_t s_surface;
static lv_style_t s_card;
static lv_style_t s_card_pressed;
static lv_style_t s_card_selected;
static lv_style_t s_transparent;

static lv_style_transition_dsc_t s_trans_press;

/* Colour-only, and it must stay that way. Anything that transforms an object
 * (scale, rotation, skew) makes LVGL render it into an intermediate layer
 * buffer — see calculate_layer_type() in lv_obj_style.c. A 384x68 card at
 * RGB565 needs ~52KB, which does not fit the 64KB LVGL heap, and the draw
 * dispatcher then spins retrying the allocation until the watchdog resets. */
static const lv_style_prop_t s_trans_props[] = {
    LV_STYLE_BG_COLOR,
    LV_STYLE_BORDER_COLOR,
    0,
};

void ui_theme_init(void)
{
    if (s_inited) {
        return;
    }
    s_inited = true;

    lv_style_transition_dsc_init(&s_trans_press, s_trans_props, lv_anim_path_ease_out,
                                 UI_ANIM_TIME_MS, 0, NULL);

    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, ui_color(UI_COLOR_BG));
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_radius(&s_screen, 0);
    lv_style_set_pad_all(&s_screen, UI_SPACE_XL);
    lv_style_set_pad_row(&s_screen, 0);
    lv_style_set_pad_column(&s_screen, UI_SPACE_XL);
    lv_style_set_text_color(&s_screen, ui_color(UI_COLOR_TEXT));
    lv_style_set_text_font(&s_screen, UI_FONT_BODY);

    lv_style_init(&s_surface);
    lv_style_set_bg_color(&s_surface, ui_color(UI_COLOR_SURFACE));
    lv_style_set_bg_opa(&s_surface, LV_OPA_COVER);
    lv_style_set_radius(&s_surface, UI_RADIUS_LG);
    lv_style_set_border_width(&s_surface, 1);
    lv_style_set_border_color(&s_surface, ui_color(UI_COLOR_BORDER));
    lv_style_set_border_opa(&s_surface, LV_OPA_COVER);
    lv_style_set_shadow_width(&s_surface, 0);
    lv_style_set_pad_all(&s_surface, 0);

    lv_style_init(&s_card);
    lv_style_set_bg_color(&s_card, ui_color(UI_COLOR_SURFACE));
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_radius(&s_card, UI_RADIUS_MD);
    lv_style_set_border_width(&s_card, 1);
    lv_style_set_border_color(&s_card, ui_color(UI_COLOR_BORDER));
    lv_style_set_border_side(&s_card, LV_BORDER_SIDE_FULL);
    lv_style_set_shadow_width(&s_card, 0);
    lv_style_set_pad_hor(&s_card, UI_SPACE_LG);
    lv_style_set_pad_ver(&s_card, UI_SPACE_SM);
    lv_style_set_pad_column(&s_card, UI_SPACE_MD);
    lv_style_set_transition(&s_card, &s_trans_press);

    lv_style_init(&s_card_pressed);
    lv_style_set_bg_color(&s_card_pressed, ui_color(UI_COLOR_SURFACE_HI));
    lv_style_set_border_color(&s_card_pressed, ui_color(UI_COLOR_TEXT_MUTED));

    lv_style_init(&s_card_selected);
    lv_style_set_bg_color(&s_card_selected, ui_color(UI_COLOR_ACCENT_SOFT));
    lv_style_set_border_width(&s_card_selected, 4);
    lv_style_set_border_color(&s_card_selected, ui_color(UI_COLOR_ACCENT));
    lv_style_set_border_side(&s_card_selected, LV_BORDER_SIDE_LEFT);

    lv_style_init(&s_transparent);
    lv_style_set_bg_opa(&s_transparent, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_transparent, 0);
    lv_style_set_radius(&s_transparent, 0);
    lv_style_set_pad_all(&s_transparent, 0);
    lv_style_set_pad_row(&s_transparent, 0);
    lv_style_set_pad_column(&s_transparent, 0);
}

lv_style_t *ui_style_screen(void)        { return &s_screen;        }
lv_style_t *ui_style_surface(void)       { return &s_surface;       }
lv_style_t *ui_style_card(void)          { return &s_card;          }
lv_style_t *ui_style_card_pressed(void)  { return &s_card_pressed;  }
lv_style_t *ui_style_card_selected(void) { return &s_card_selected; }
lv_style_t *ui_style_transparent(void)   { return &s_transparent;   }
