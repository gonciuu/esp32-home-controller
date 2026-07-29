#include "weather_icon.h"
#include "theme.h"

#define CODE_NONE (-2)

typedef enum {
    COND_CLEAR,
    COND_PARTLY,
    COND_CLOUDY,
    COND_FOG,
    COND_RAIN,
    COND_SNOW,
    COND_STORM,
} condition_t;

static int32_t icon_size(lv_obj_t *icon)
{
    return lv_obj_get_style_width(icon, LV_PART_MAIN);
}

static int32_t scaled(int32_t value, int32_t size)
{
    return value * size / UI_WEATHER_ICON_BASE;
}

static void shape(lv_obj_t *icon, int32_t w, int32_t h, int32_t radius, uint32_t color,
                  int32_t x, int32_t y)
{
    const int32_t size = icon_size(icon);

    lv_obj_t *obj = lv_obj_create(icon);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, scaled(w, size), scaled(h, size));
    lv_obj_set_style_radius(obj, (radius == LV_RADIUS_CIRCLE) ? LV_RADIUS_CIRCLE
                                                              : scaled(radius, size), 0);
    lv_obj_set_style_bg_color(obj, ui_color(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(obj, LV_ALIGN_CENTER, scaled(x, size), scaled(y, size));
}

static void add_sun(lv_obj_t *icon, int32_t diameter, int32_t x, int32_t y, bool rays)
{
    shape(icon, diameter, diameter, LV_RADIUS_CIRCLE, UI_COLOR_SUN, x, y);

    if (!rays) {
        return;
    }
    const int32_t reach = diameter / 2 + 7;
    shape(icon, 3, 8, 2, UI_COLOR_SUN, x, y - reach);
    shape(icon, 3, 8, 2, UI_COLOR_SUN, x, y + reach);
    shape(icon, 8, 3, 2, UI_COLOR_SUN, x - reach, y);
    shape(icon, 8, 3, 2, UI_COLOR_SUN, x + reach, y);
}

static void add_cloud(lv_obj_t *icon, int32_t dx, int32_t dy)
{
    shape(icon, 20, 20, LV_RADIUS_CIRCLE, UI_COLOR_CLOUD, dx - 9, dy + 3);
    shape(icon, 26, 26, LV_RADIUS_CIRCLE, UI_COLOR_CLOUD, dx + 1, dy - 1);
    shape(icon, 18, 18, LV_RADIUS_CIRCLE, UI_COLOR_CLOUD, dx + 12, dy + 4);
    shape(icon, 38, 12, 6, UI_COLOR_CLOUD, dx + 1, dy + 10);
}

static condition_t condition_of(int code)
{
    switch (code) {
    case 0:  return COND_CLEAR;
    case 1:
    case 2:  return COND_PARTLY;
    case 3:  return COND_CLOUDY;
    case 45:
    case 48: return COND_FOG;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86: return COND_SNOW;
    case 95:
    case 96:
    case 99: return COND_STORM;
    default: break;
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return COND_RAIN;
    }
    return COND_CLOUDY;
}

static void build(lv_obj_t *icon, condition_t cond)
{
    switch (cond) {
    case COND_CLEAR:
        add_sun(icon, 24, 0, 0, true);
        break;

    case COND_PARTLY:
        add_sun(icon, 18, 11, -13, false);
        add_cloud(icon, -4, 5);
        break;

    case COND_CLOUDY:
        add_cloud(icon, 0, 0);
        break;

    case COND_FOG:
        shape(icon, 38, 4, 2, UI_COLOR_CLOUD, 0, -9);
        shape(icon, 30, 4, 2, UI_COLOR_CLOUD, -2, 0);
        shape(icon, 38, 4, 2, UI_COLOR_CLOUD, 1, 9);
        break;

    case COND_RAIN:
        add_cloud(icon, 0, -5);
        shape(icon, 3, 9, 2, UI_COLOR_RAIN, -10, 17);
        shape(icon, 3, 9, 2, UI_COLOR_RAIN, 0, 20);
        shape(icon, 3, 9, 2, UI_COLOR_RAIN, 10, 17);
        break;

    case COND_SNOW:
        add_cloud(icon, 0, -5);
        shape(icon, 6, 6, LV_RADIUS_CIRCLE, UI_COLOR_SNOW, -10, 18);
        shape(icon, 6, 6, LV_RADIUS_CIRCLE, UI_COLOR_SNOW, 0, 21);
        shape(icon, 6, 6, LV_RADIUS_CIRCLE, UI_COLOR_SNOW, 10, 18);
        break;

    case COND_STORM:
        add_cloud(icon, 0, -6);
        /* Two offset bars read as a bolt; a real zigzag would need lv_line
         * point arrays kept alive per instance. */
        shape(icon, 5, 11, 1, UI_COLOR_SUN, 3, 14);
        shape(icon, 5, 11, 1, UI_COLOR_SUN, -2, 21);
        break;
    }
}

lv_obj_t *ui_weather_icon_create(lv_obj_t *parent, int32_t size)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_remove_style_all(icon);
    lv_obj_add_style(icon, ui_style_transparent(), 0);
    lv_obj_set_size(icon, size, size);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(icon, (void *)(intptr_t)CODE_NONE);

    return icon;
}

void ui_weather_icon_set_code(lv_obj_t *icon, int code)
{
    if ((int)(intptr_t)lv_obj_get_user_data(icon) == code) {
        return;
    }
    lv_obj_set_user_data(icon, (void *)(intptr_t)code);

    lv_obj_clean(icon);
    build(icon, condition_of(code));
}
