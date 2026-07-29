#include "status_panel.h"
#include "hourly_strip.h"
#include "theme.h"
#include "time_sync.h"
#include "weather.h"
#include "weather_icon.h"
#include "wifi_sta.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define TICK_PERIOD_MS    1000
#define TEXT_CAP          40
#define TODAY_ICON_SIZE   48
#define TOMORROW_ICON     30
#define ROW_DIVIDER_H     48

static lv_obj_t *s_time;
static lv_obj_t *s_date;
static lv_obj_t *s_icon;
static lv_obj_t *s_temp;
static lv_obj_t *s_desc;
static lv_obj_t *s_tomorrow;
static lv_obj_t *s_tomorrow_icon;
static lv_obj_t *s_tomorrow_temp;
static lv_obj_t *s_detail;
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
    static char desc_cache[TEXT_CAP] = { 0 };
    static char detail_cache[TEXT_CAP] = { 0 };
    static char tomorrow_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];

    weather_t w;
    if (!weather_get(&w)) {
        set_text_if_changed(s_temp, temp_cache, "--\xC2\xB0");
        set_text_if_changed(s_desc, desc_cache, "No weather data");
        set_text_if_changed(s_detail, detail_cache, "");
        lv_obj_add_flag(s_tomorrow, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ui_weather_icon_set_code(s_icon, w.code);

    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", w.temp_c);
    set_text_if_changed(s_temp, temp_cache, buf);

    set_text_if_changed(s_desc, desc_cache, weather_code_text(w.code));

    snprintf(buf, sizeof(buf), "%d%% humidity   %.0f km/h", w.humidity, w.wind_kmh);
    set_text_if_changed(s_detail, detail_cache, buf);

    if (!w.tomorrow_valid) {
        lv_obj_add_flag(s_tomorrow, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ui_weather_icon_set_code(s_tomorrow_icon, w.tomorrow_code);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0/%.0f\xC2\xB0", w.tomorrow_max, w.tomorrow_min);
    set_text_if_changed(s_tomorrow_temp, tomorrow_cache, buf);
    lv_obj_remove_flag(s_tomorrow, LV_OBJ_FLAG_HIDDEN);
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

static lv_obj_t *make_box(lv_obj_t *parent, lv_flex_flow_t flow, int32_t gap)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_add_style(box, ui_style_transparent(), 0);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, flow);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(box, gap, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static lv_obj_t *make_stack(lv_obj_t *parent)
{
    lv_obj_t *stack = make_box(parent, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_width(stack, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(stack, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return stack;
}

static void make_divider(lv_obj_t *parent)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(line, ui_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_ver(line, UI_SPACE_SM, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void make_vertical_divider(lv_obj_t *parent)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 1, ROW_DIVIDER_H);
    lv_obj_set_style_bg_color(line, ui_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_hor(line, UI_SPACE_SM, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_weather_row(lv_obj_t *panel)
{
    lv_obj_t *row = make_box(panel, LV_FLEX_FLOW_ROW, 0);

    lv_obj_t *today = make_box(row, LV_FLEX_FLOW_ROW, UI_SPACE_SM);
    lv_obj_set_flex_grow(today, 1);
    s_icon = ui_weather_icon_create(today, TODAY_ICON_SIZE);
    lv_obj_t *today_text = make_stack(today);
    s_temp = make_label(today_text, UI_FONT_TITLE, UI_COLOR_TEXT, "--\xC2\xB0");
    s_desc = make_label(today_text, UI_FONT_BODY, UI_COLOR_TEXT, "");

    s_tomorrow = make_box(row, LV_FLEX_FLOW_ROW, UI_SPACE_SM);
    lv_obj_set_width(s_tomorrow, LV_SIZE_CONTENT);
    make_vertical_divider(s_tomorrow);
    s_tomorrow_icon = ui_weather_icon_create(s_tomorrow, TOMORROW_ICON);
    lv_obj_t *tomorrow_text = make_stack(s_tomorrow);
    make_label(tomorrow_text, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "Tomorrow");
    s_tomorrow_temp = make_label(tomorrow_text, UI_FONT_BODY, UI_COLOR_TEXT, "");
    lv_obj_add_flag(s_tomorrow, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_status_panel_create(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, ui_style_surface(), 0);
    lv_obj_set_size(panel, UI_STATUS_PANEL_WIDTH, LV_PCT(100));
    lv_obj_set_style_pad_all(panel, UI_SPACE_XL, 0);
    lv_obj_set_style_pad_row(panel, UI_SPACE_XS, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    s_time = make_label(panel, UI_FONT_CLOCK, UI_COLOR_TEXT, "--:--:--");

    lv_obj_t *date_row = make_box(panel, LV_FLEX_FLOW_ROW, UI_SPACE_SM);
    s_date = make_label(date_row, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");
    lv_obj_set_flex_grow(s_date, 1);
    make_label(date_row, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, CONFIG_HOME_WEATHER_CITY);

    make_divider(panel);
    create_weather_row(panel);
    make_divider(panel);

    ui_hourly_strip_create(panel);

    lv_obj_t *spacer = lv_obj_create(panel);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, LV_PCT(100));
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_remove_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *footer = make_box(panel, LV_FLEX_FLOW_ROW, UI_SPACE_SM);
    s_detail = make_label(footer, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");
    lv_obj_set_flex_grow(s_detail, 1);
    s_wifi = make_label(footer, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, LV_SYMBOL_WIFI);

    tick_cb(NULL);
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return panel;
}
