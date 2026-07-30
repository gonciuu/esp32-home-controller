#include "weather_page.h"
#include "hourly_strip.h"
#include "label.h"
#include "page.h"
#include "theme.h"
#include "tile_card.h"
#include "weather.h"
#include "weather_icon.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* 720 x 340 inside the page padding and back row: 112 + 12 + 104 + 12 + 92 = 332. The
 * day card needs its 104 for name, 30 px icon, temps and wind plus padding and border. */
#define HERO_HEIGHT    112
#define DAYS_HEIGHT    104
#define TICK_PERIOD_MS 5000
#define TEXT_CAP       16
#define TILE_TEXT_CAP  32
#define HERO_ICON_SIZE 72
#define DAY_ICON_SIZE  30
#define DIVIDER_HEIGHT 88
#define STAT_KEY_WIDTH 84

enum {
    STAT_HIGH,
    STAT_LOW,
    STAT_EXTRA,
    STAT_WIND,
    STAT_SUNRISE,
    STAT_SUNSET,
    STAT_COUNT,
};

static const char *s_stat_keys[STAT_COUNT] = {
    "High", "Low", "Humidity", "Wind", "Sunrise", "Sunset",
};

static lv_obj_t *s_hero_icon;
static lv_obj_t *s_hero_temp;
static lv_obj_t *s_hero_cond;
static lv_obj_t *s_stat_key[STAT_COUNT];
static lv_obj_t *s_stat[STAT_COUNT];
static lv_obj_t *s_days;
static lv_obj_t *s_day_name[WEATHER_DAY_COUNT];
static lv_obj_t *s_day_icon[WEATHER_DAY_COUNT];
static lv_obj_t *s_day_temp[WEATHER_DAY_COUNT];
static lv_obj_t *s_day_wind[WEATHER_DAY_COUNT];
static lv_obj_t *s_day_card[WEATHER_DAY_COUNT];

static char s_stat_cache[STAT_COUNT][TEXT_CAP];
static char s_stat_key_cache[STAT_COUNT][TEXT_CAP];
static char s_day_name_cache[WEATHER_DAY_COUNT][TEXT_CAP];
static char s_day_temp_cache[WEATHER_DAY_COUNT][TEXT_CAP];
static char s_day_wind_cache[WEATHER_DAY_COUNT][TEXT_CAP];
static char s_hero_temp_cache[TEXT_CAP];
static char s_hero_cond_cache[TEXT_CAP];
static char s_tile_cache[TILE_TEXT_CAP];

static lv_obj_t *s_tile;

static int s_selected;

static void set_stat(int index, const char *text)
{
    ui_label_set(s_stat[index], s_stat_cache[index], TEXT_CAP, text);
}

static void day_name(int offset, char *out, size_t cap)
{
    if (offset == 0) {
        strlcpy(out, "Today", cap);
        return;
    }

    const time_t now = time(NULL);
    struct tm tm_day;
    localtime_r(&now, &tm_day);
    tm_day.tm_mday += offset;
    tm_day.tm_isdst = -1;
    mktime(&tm_day);
    strftime(out, cap, "%a", &tm_day);
}

static bool minutes_of(const char *hm, int *out)
{
    if (strlen(hm) != 5) {
        return false;
    }
    *out = (hm[0] - '0') * 600 + (hm[1] - '0') * 60 + (hm[3] - '0') * 10 + (hm[4] - '0');
    return true;
}

static void set_daylight(const weather_day_t *day)
{
    int rise, set;

    if (!minutes_of(day->sunrise, &rise) || !minutes_of(day->sunset, &set) || set <= rise) {
        set_stat(STAT_EXTRA, "--");
        return;
    }

    char buf[TEXT_CAP];
    snprintf(buf, sizeof(buf), "%dh %02dm", (set - rise) / 60, (set - rise) % 60);
    set_stat(STAT_EXTRA, buf);
}

static void set_card_selected(int index, bool selected)
{
    lv_obj_set_style_bg_color(s_day_card[index],
                              ui_color(selected ? UI_COLOR_ACCENT_SOFT : UI_COLOR_SURFACE_HI), 0);
    lv_obj_set_style_border_color(s_day_card[index],
                                  ui_color(selected ? UI_COLOR_ACCENT : UI_COLOR_SURFACE_HI), 0);
}

static void render_days(const weather_t *w)
{
    char buf[TEXT_CAP];

    for (int i = 0; i < WEATHER_DAY_COUNT; i++) {
        if (i >= w->daily_count) {
            lv_obj_add_flag(s_day_card[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(s_day_card[i], LV_OBJ_FLAG_HIDDEN);
        set_card_selected(i, i == s_selected);

        day_name(i, buf, sizeof(buf));
        ui_label_set(s_day_name[i], s_day_name_cache[i], TEXT_CAP, buf);

        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0/%.0f\xC2\xB0", w->daily[i].max_c,
                 w->daily[i].min_c);
        ui_label_set(s_day_temp[i], s_day_temp_cache[i], TEXT_CAP, buf);

        snprintf(buf, sizeof(buf), "%.0f km/h", w->daily[i].wind_kmh);
        ui_label_set(s_day_wind[i], s_day_wind_cache[i], TEXT_CAP, buf);

        ui_weather_icon_set_code(s_day_icon[i], w->daily[i].code);
    }
}

static void render_hero(const weather_t *w)
{
    const bool today = (s_selected == 0);
    const bool has_day = w->daily_valid && s_selected < w->daily_count;
    const weather_day_t *day = has_day ? &w->daily[s_selected] : NULL;
    char buf[TEXT_CAP];

    if (today) {
        snprintf(buf, sizeof(buf), "%.0f", w->temp_c);
        ui_label_set(s_hero_temp, s_hero_temp_cache, TEXT_CAP, buf);
        ui_label_set(s_hero_cond, s_hero_cond_cache, TEXT_CAP, weather_code_text(w->code));
        ui_weather_icon_set_code(s_hero_icon, w->code);

        snprintf(buf, sizeof(buf), "%d%%", w->humidity);
        set_stat(STAT_EXTRA, buf);

        snprintf(buf, sizeof(buf), "%.0f km/h", w->wind_kmh);
        set_stat(STAT_WIND, buf);
    }
    else if (day) {
        snprintf(buf, sizeof(buf), "%.0f", day->max_c);
        ui_label_set(s_hero_temp, s_hero_temp_cache, TEXT_CAP, buf);
        ui_label_set(s_hero_cond, s_hero_cond_cache, TEXT_CAP, weather_code_text(day->code));
        ui_weather_icon_set_code(s_hero_icon, day->code);

        set_daylight(day);

        snprintf(buf, sizeof(buf), "%.0f km/h", day->wind_kmh);
        set_stat(STAT_WIND, buf);
    }

    ui_label_set(s_stat_key[STAT_EXTRA], s_stat_key_cache[STAT_EXTRA], TEXT_CAP,
                 today ? "Humidity" : "Daylight");

    if (!day) {
        if (!today) {
            ui_label_set(s_hero_temp, s_hero_temp_cache, TEXT_CAP, "--");
            ui_label_set(s_hero_cond, s_hero_cond_cache, TEXT_CAP, "");
        }
        set_stat(STAT_HIGH, "--");
        set_stat(STAT_LOW, "--");
        set_stat(STAT_SUNRISE, "--:--");
        set_stat(STAT_SUNSET, "--:--");
        return;
    }

    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", day->max_c);
    set_stat(STAT_HIGH, buf);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", day->min_c);
    set_stat(STAT_LOW, buf);
    set_stat(STAT_SUNRISE, day->sunrise[0] ? day->sunrise : "--:--");
    set_stat(STAT_SUNSET, day->sunset[0] ? day->sunset : "--:--");
}

static void render_tile(const weather_t *w)
{
    if (!s_tile) {
        return;
    }
    if (!w) {
        ui_label_set(s_tile, s_tile_cache, TILE_TEXT_CAP, "--");
        return;
    }

    char cond[16];
    strlcpy(cond, weather_code_text(w->code), sizeof(cond));

    char buf[TILE_TEXT_CAP];
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C  %s", w->temp_c, cond);
    ui_label_set(s_tile, s_tile_cache, TILE_TEXT_CAP, buf);
}

static void render(void)
{
    weather_t w;

    if (!weather_get(&w)) {
        ui_label_set(s_hero_temp, s_hero_temp_cache, TEXT_CAP, "--");
        ui_label_set(s_hero_cond, s_hero_cond_cache, TEXT_CAP, "No weather data");
        for (int i = 0; i < STAT_COUNT; i++) {
            set_stat(i, "--");
        }
        render_tile(NULL);
        lv_obj_add_flag(s_days, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    render_tile(&w);
    render_hero(&w);

    if (!w.daily_valid) {
        lv_obj_add_flag(s_days, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(s_days, LV_OBJ_FLAG_HIDDEN);
    render_days(&w);
}

static void tick_cb(lv_timer_t *timer)
{
    render();
}

static void day_event_cb(lv_event_t *e)
{
    const int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index == s_selected) {
        return;
    }
    s_selected = index;
    ui_hourly_strip_set_day(index);
    render();
}

void ui_weather_page_bind_tile(lv_obj_t *card)
{
    s_tile = ui_tile_card_info_label(card);
}

void ui_weather_page_select_today(void)
{
    if (s_selected == 0) {
        return;
    }
    s_selected = 0;
    ui_hourly_strip_set_day(0);
    render();
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
    lv_obj_set_style_pad_row(box, gap, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static void make_vertical_divider(lv_obj_t *parent)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 1, DIVIDER_HEIGHT);
    lv_obj_set_style_bg_color(line, ui_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_stat(lv_obj_t *column, int index)
{
    lv_obj_t *row = make_box(column, LV_FLEX_FLOW_ROW, UI_SPACE_SM);
    lv_obj_set_width(row, LV_SIZE_CONTENT);

    s_stat_key[index] = ui_label(row, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED,
                                   s_stat_keys[index]);
    lv_obj_set_width(s_stat_key[index], STAT_KEY_WIDTH);
    strlcpy(s_stat_key_cache[index], s_stat_keys[index], TEXT_CAP);

    s_stat[index] = ui_label(row, UI_FONT_BODY, UI_COLOR_TEXT, "--");
}

static void create_hero(lv_obj_t *page)
{
    lv_obj_t *hero = make_box(page, LV_FLEX_FLOW_ROW, UI_SPACE_LG);
    lv_obj_set_height(hero, HERO_HEIGHT);

    lv_obj_t *now = make_box(hero, LV_FLEX_FLOW_ROW, UI_SPACE_MD);
    lv_obj_set_width(now, LV_SIZE_CONTENT);

    s_hero_icon = ui_weather_icon_create(now, HERO_ICON_SIZE);

    lv_obj_t *text = make_box(now, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_width(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *temp_row = make_box(text, LV_FLEX_FLOW_ROW, UI_SPACE_XS);
    lv_obj_set_width(temp_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    s_hero_temp = ui_label(temp_row, UI_FONT_CLOCK, UI_COLOR_TEXT, "--");
    ui_label(temp_row, UI_FONT_TITLE, UI_COLOR_TEXT_MUTED, "\xC2\xB0" "C");

    s_hero_cond = ui_label(text, UI_FONT_BODY, UI_COLOR_TEXT_MUTED, "");

    lv_obj_t *stats = make_box(hero, LV_FLEX_FLOW_ROW, UI_SPACE_XL);
    lv_obj_set_flex_grow(stats, 1);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_vertical_divider(stats);

    for (int column = 0; column < 2; column++) {
        lv_obj_t *box = make_box(stats, LV_FLEX_FLOW_COLUMN, UI_SPACE_SM);
        lv_obj_set_width(box, LV_SIZE_CONTENT);
        for (int row = 0; row < 3; row++) {
            create_stat(box, column * 3 + row);
        }
    }
}

static void create_day(lv_obj_t *row, int index)
{
    lv_obj_t *card = lv_obj_create(row);
    lv_obj_remove_style_all(card);
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_radius(card, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_side(card, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_pad_ver(card, UI_SPACE_XS, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, day_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    s_day_card[index] = card;
    set_card_selected(index, index == 0);

    s_day_name[index] = ui_label(card, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");
    s_day_icon[index] = ui_weather_icon_create(card, DAY_ICON_SIZE);
    s_day_temp[index] = ui_label(card, UI_FONT_BODY, UI_COLOR_TEXT, "");
    s_day_wind[index] = ui_label(card, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");

    lv_obj_remove_flag(s_day_name[index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_day_temp[index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_day_wind[index], LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *ui_weather_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data)
{
    lv_obj_t *page = ui_page_create(parent, NULL, on_back, user_data);
    lv_obj_set_style_pad_row(page, UI_SPACE_MD, 0);

    create_hero(page);

    s_days = make_box(page, LV_FLEX_FLOW_ROW, UI_SPACE_MD);
    lv_obj_set_height(s_days, DAYS_HEIGHT);
    for (int i = 0; i < WEATHER_DAY_COUNT; i++) {
        create_day(s_days, i);
    }

    ui_hourly_strip_create(page);

    render();
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return page;
}
