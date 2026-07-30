#include "top_bar.h"
#include "theme.h"
#include "time_sync.h"
#include "weather.h"
#include "weather_icon.h"
#include "wifi_sta.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define TICK_PERIOD_MS 1000
#define TEXT_CAP       40
#define ICON_SIZE      28
#define BUTTON_HEIGHT  44
#define CLOCK_WIDTH    116

static lv_obj_t *s_time;
static lv_obj_t *s_date;
static lv_obj_t *s_icon;
static lv_obj_t *s_temp;
static lv_obj_t *s_wifi;

static void set_text_if_changed(lv_obj_t *label, char *cache, const char *text)
{
    if (strcmp(cache, text) == 0) {
        return;
    }
    strlcpy(cache, text, TEXT_CAP);
    lv_label_set_text(label, text);
}

static void update_clock(void)
{
    static char time_cache[TEXT_CAP] = { 0 };
    static char date_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];

    if (!time_sync_is_valid()) {
        set_text_if_changed(s_time, time_cache, "--:--:--");
        set_text_if_changed(s_date, date_cache, "Waiting for time");
        return;
    }

    const time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_now);
    set_text_if_changed(s_time, time_cache, buf);

    strftime(buf, sizeof(buf), "%a, %d %B", &tm_now);
    set_text_if_changed(s_date, date_cache, buf);
}

static void update_weather(void)
{
    static char temp_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];

    weather_t w;
    if (!weather_get(&w)) {
        set_text_if_changed(s_temp, temp_cache, "--\xC2\xB0");
        return;
    }

    ui_weather_icon_set_code(s_icon, w.code);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", w.temp_c);
    set_text_if_changed(s_temp, temp_cache, buf);
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

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                            const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, ui_color(color), 0);
    return label;
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

    s_temp = make_label(button, UI_FONT_BODY, UI_COLOR_TEXT, "--\xC2\xB0");
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

    s_time = make_label(bar, UI_FONT_TITLE, UI_COLOR_TEXT, "--:--:--");
    lv_obj_set_width(s_time, CLOCK_WIDTH);

    s_date = make_label(bar, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");
    lv_obj_set_flex_grow(s_date, 1);

    make_label(bar, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, CONFIG_HOME_WEATHER_CITY);

    create_weather_button(bar, on_weather_click, user_data);

    s_wifi = make_label(bar, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, LV_SYMBOL_WIFI);

    tick_cb(NULL);
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return bar;
}
