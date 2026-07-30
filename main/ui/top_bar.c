#include "top_bar.h"
#include "label.h"
#include "theme.h"
#include "time_sync.h"
#include "weather.h"
#include "weather_icon.h"
#include "wifi_sta.h"

#include <stdio.h>
#include <time.h>

#define TICK_PERIOD_MS 1000
#define TEXT_CAP       40
#define ICON_SIZE      28
#define BUTTON_HEIGHT  44

static lv_obj_t *s_time;
static lv_obj_t *s_place;
static lv_obj_t *s_icon;
static lv_obj_t *s_temp;
static lv_obj_t *s_wifi;

static void update_clock(void)
{
    static char time_cache[TEXT_CAP] = { 0 };
    static char place_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];
    char date[24];

    if (!time_sync_is_valid()) {
        ui_label_set(s_time, time_cache, sizeof(time_cache), "--:--:--");
        ui_label_set(s_place, place_cache, sizeof(place_cache), CONFIG_HOME_WEATHER_CITY);
        return;
    }

    const time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_now);
    ui_label_set(s_time, time_cache, sizeof(time_cache), buf);

    strftime(date, sizeof(date), "%a, %d %B", &tm_now);
    snprintf(buf, sizeof(buf), CONFIG_HOME_WEATHER_CITY ", %s", date);
    ui_label_set(s_place, place_cache, sizeof(place_cache), buf);
}

static void update_weather(void)
{
    static char temp_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];

    weather_t w;
    if (!weather_get(&w)) {
        ui_label_set(s_temp, temp_cache, sizeof(temp_cache), "--\xC2\xB0");
        return;
    }

    ui_weather_icon_set_code(s_icon, w.code);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", w.temp_c);
    ui_label_set(s_temp, temp_cache, sizeof(temp_cache), buf);
}

static void update_wifi(void)
{
    static int last = -1;
    const int connected = wifi_sta_is_connected() ? 1 : 0;
    if (connected == last) {
        return;
    }
    last = connected;
    lv_obj_set_style_text_color(s_wifi,
                                ui_color(connected ? UI_COLOR_ACCENT : UI_COLOR_TEXT_MUTED), 0);
}

static void tick_cb(lv_timer_t *timer)
{
    update_clock();
    update_weather();
    update_wifi();
}

static void create_weather_button(lv_obj_t *bar, lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *button = lv_obj_create(bar);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, LV_SIZE_CONTENT, BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_color(button, ui_color(UI_COLOR_SURFACE_HI), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(button, UI_SPACE_SM, 0);
    lv_obj_set_style_pad_column(button, UI_SPACE_SM, 0);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    s_icon = ui_weather_icon_create(button, ICON_SIZE);

    s_temp = ui_label(button, UI_FONT_BODY, UI_COLOR_TEXT, "--\xC2\xB0");
    lv_obj_remove_flag(s_temp, LV_OBJ_FLAG_CLICKABLE);

    if (on_click) {
        lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, user_data);
    }
}

lv_obj_t *ui_top_bar_create(lv_obj_t *parent, lv_event_cb_t on_weather_click, void *user_data)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, ui_style_surface(), 0);
    lv_obj_set_size(bar, LV_PCT(100), UI_TOP_BAR_HEIGHT);
    lv_obj_set_style_pad_hor(bar, UI_SPACE_LG, 0);
    lv_obj_set_style_pad_column(bar, UI_SPACE_MD, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_time = ui_label(bar, UI_FONT_TITLE, UI_COLOR_TEXT, "--:--:--");

    s_place = ui_label(bar, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, CONFIG_HOME_WEATHER_CITY);
    lv_obj_set_flex_grow(s_place, 1);
    lv_obj_set_style_text_align(s_place, LV_TEXT_ALIGN_RIGHT, 0);

    create_weather_button(bar, on_weather_click, user_data);

    s_wifi = ui_label(bar, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, LV_SYMBOL_WIFI);

    tick_cb(NULL);
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return bar;
}
