#include "screensaver.h"
#include "bsp.h"
#include "label.h"
#include "markets.h"
#include "markets_page.h"
#include "theme.h"
#include "time_sync.h"
#include "weather.h"
#include "weather_icon.h"

#include <stdio.h>
#include <time.h>

#define TICK_PERIOD_MS 1000
#define TEXT_CAP       64
#define ICON_SIZE      40

#define SHIFT_PERIOD_S 60
#define SHIFT_STEP     6
#define SHIFT_STOPS    4

typedef enum {
    STATE_ACTIVE,
    STATE_AMBIENT,
    STATE_OFF,
} state_t;

static lv_obj_t *s_overlay;
static lv_obj_t *s_block;
static lv_obj_t *s_time;
static lv_obj_t *s_date;
static lv_obj_t *s_icon;
static lv_obj_t *s_temp;
static lv_obj_t *s_btc;
static state_t   s_state = STATE_ACTIVE;
static int       s_btc_dir = -1;

static void update_labels(void)
{
    static char time_cache[TEXT_CAP] = { 0 };
    static char date_cache[TEXT_CAP] = { 0 };
    static char temp_cache[TEXT_CAP] = { 0 };
    char buf[TEXT_CAP];

    if (time_sync_is_valid()) {
        const time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        strftime(buf, sizeof(buf), "%H:%M", &tm_now);
        ui_label_set(s_time, time_cache, sizeof(time_cache), buf);

        char date[32];
        strftime(date, sizeof(date), "%A, %d %B", &tm_now);
        snprintf(buf, sizeof(buf), "%s  \xE2\x80\xA2  " CONFIG_HOME_WEATHER_CITY, date);
        ui_label_set(s_date, date_cache, sizeof(date_cache), buf);
    }
    else {
        ui_label_set(s_time, time_cache, sizeof(time_cache), "--:--");
        ui_label_set(s_date, date_cache, sizeof(date_cache), CONFIG_HOME_WEATHER_CITY);
    }

    weather_t w;
    if (weather_get(&w)) {
        ui_weather_icon_set_code(s_icon, w.code);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", w.temp_c);
        ui_label_set(s_temp, temp_cache, sizeof(temp_cache), buf);
    }
    else {
        ui_label_set(s_temp, temp_cache, sizeof(temp_cache), "--\xC2\xB0");
    }

    static char btc_cache[TEXT_CAP] = { 0 };
    markets_t m;
    if (markets_get(&m) && m.asset[0].valid) {
        char price[16];
        ui_markets_format_price(m.asset[0].price, price, sizeof(price));
        snprintf(buf, sizeof(buf), "%s $%s  %+.1f%%", markets_symbol(0), price,
                 m.asset[0].change_pct);
        ui_label_set(s_btc, btc_cache, sizeof(btc_cache), buf);

        const int up = m.asset[0].change_pct >= 0.0f;
        if (up != s_btc_dir) {
            s_btc_dir = up;
            lv_obj_set_style_text_color(s_btc, ui_color(up ? UI_COLOR_UP : UI_COLOR_DOWN), 0);
        }
    }
    else {
        ui_label_set(s_btc, btc_cache, sizeof(btc_cache), "");
    }
}

static void update_shift(uint32_t idle_s)
{
    static const lv_point_t stops[SHIFT_STOPS] = {
        { -SHIFT_STEP, -SHIFT_STEP },
        {  SHIFT_STEP, -SHIFT_STEP },
        {  SHIFT_STEP,  SHIFT_STEP },
        { -SHIFT_STEP,  SHIFT_STEP },
    };
    static int32_t last = -1;

    const int32_t stop = (idle_s / SHIFT_PERIOD_S) % SHIFT_STOPS;
    if (stop == last) {
        return;
    }
    last = stop;

    lv_obj_set_style_translate_x(s_block, stops[stop].x, 0);
    lv_obj_set_style_translate_y(s_block, stops[stop].y, 0);
}

static void wake(void)
{
    if (s_state == STATE_ACTIVE) {
        return;
    }
    if (s_state == STATE_OFF) {
        bsp_display_backlight(true);
    }
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    s_state = STATE_ACTIVE;
}

static void press_cb(lv_event_t *e)
{
    wake();
}

static void tick_cb(lv_timer_t *timer)
{
    const uint32_t idle_s = lv_display_get_inactive_time(NULL) / 1000;

    if (CONFIG_HOME_SCREENSAVER_IDLE_S == 0 || idle_s < CONFIG_HOME_SCREENSAVER_IDLE_S) {
        wake();
        return;
    }

    if (s_state == STATE_ACTIVE) {
        lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        s_state = STATE_AMBIENT;
    }

    if (s_state == STATE_AMBIENT && CONFIG_HOME_SCREENSAVER_OFF_S > 0 &&
        idle_s >= CONFIG_HOME_SCREENSAVER_OFF_S) {
        bsp_display_backlight(false);
        s_state = STATE_OFF;
    }

    if (s_state == STATE_AMBIENT) {
        update_labels();
        update_shift(idle_s);
    }
}

void ui_screensaver_create(lv_obj_t *parent)
{
    s_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_add_style(s_overlay, ui_style_screen(), 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(s_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_overlay, press_cb, LV_EVENT_PRESSED, NULL);

    s_block = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_block);
    lv_obj_add_style(s_block, ui_style_transparent(), 0);
    lv_obj_set_size(s_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(s_block);
    lv_obj_set_flex_flow(s_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_block, UI_SPACE_MD, 0);
    lv_obj_set_flex_align(s_block, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_block, LV_OBJ_FLAG_CLICKABLE);

    s_time = ui_label(s_block, UI_FONT_CLOCK, UI_COLOR_TEXT_MUTED, "--:--");
    s_date = ui_label(s_block, UI_FONT_HEADING, UI_COLOR_TEXT_MUTED, CONFIG_HOME_WEATHER_CITY);

    lv_obj_t *row = lv_obj_create(s_block);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, ui_style_transparent(), 0);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_SPACE_SM, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    s_icon = ui_weather_icon_create(row, ICON_SIZE);
    s_temp = ui_label(row, UI_FONT_TITLE, UI_COLOR_TEXT_MUTED, "--\xC2\xB0");

    s_btc = ui_label(s_block, UI_FONT_BODY, UI_COLOR_TEXT_MUTED, "");

    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);
}
