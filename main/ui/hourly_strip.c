#include "hourly_strip.h"
#include "theme.h"
#include "weather.h"
#include "weather_icon.h"

#include <stdio.h>
#include <string.h>

#define TICK_PERIOD_MS 5000
#define VISIBLE_HOURS  5
#define CELL_ICON_SIZE 26
#define ARROW_W        44
#define ARROW_H        32
#define TEXT_CAP       12

static lv_obj_t *s_cell[VISIBLE_HOURS];
static lv_obj_t *s_hour[VISIBLE_HOURS];
static lv_obj_t *s_icon[VISIBLE_HOURS];
static lv_obj_t *s_temp[VISIBLE_HOURS];
static lv_obj_t *s_prev;
static lv_obj_t *s_next;

static char s_hour_cache[VISIBLE_HOURS][TEXT_CAP];
static char s_temp_cache[VISIBLE_HOURS][TEXT_CAP];

static int s_page;
static int s_base_hour = -1;

static void set_text_if_changed(lv_obj_t *label, char *cache, const char *text)
{
    if (strcmp(cache, text) == 0) {
        return;
    }
    strlcpy(cache, text, TEXT_CAP);
    lv_label_set_text(label, text);
}

static int page_count(const weather_t *w)
{
    if (!w->hourly_valid) {
        return 1;
    }
    const int pages = (w->hourly_count + VISIBLE_HOURS - 1) / VISIBLE_HOURS;
    return (pages < 1) ? 1 : pages;
}

static void set_arrow_enabled(lv_obj_t *arrow, bool enabled)
{
    lv_obj_set_style_text_color(arrow, ui_color(enabled ? UI_COLOR_TEXT : UI_COLOR_BORDER), 0);
    if (enabled) {
        lv_obj_add_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void render(const weather_t *w)
{
    const int pages = page_count(w);
    if (s_page >= pages) {
        s_page = pages - 1;
    }

    for (int i = 0; i < VISIBLE_HOURS; i++) {
        const int index = s_page * VISIBLE_HOURS + i;
        char buf[TEXT_CAP];

        if (w->hourly_valid && index >= w->hourly_count) {
            lv_obj_add_flag(s_cell[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(s_cell[i], LV_OBJ_FLAG_HIDDEN);

        if (!w->hourly_valid) {
            set_text_if_changed(s_hour[i], s_hour_cache[i], "--:--");
            set_text_if_changed(s_temp[i], s_temp_cache[i], "--\xC2\xB0");
            continue;
        }

        snprintf(buf, sizeof(buf), "%02d:00", w->hourly[index].hour);
        set_text_if_changed(s_hour[i], s_hour_cache[i], buf);

        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", w->hourly[index].temp_c);
        set_text_if_changed(s_temp[i], s_temp_cache[i], buf);

        ui_weather_icon_set_code(s_icon[i], w->hourly[index].code);
    }

    set_arrow_enabled(s_prev, s_page > 0);
    set_arrow_enabled(s_next, s_page < pages - 1);
}

static void tick_cb(lv_timer_t *timer)
{
    weather_t w;
    weather_get(&w);

    if (w.hourly_valid && w.hourly[0].hour != s_base_hour) {
        s_base_hour = w.hourly[0].hour;
        s_page = 0;
    }

    render(&w);
}

static void step_cb(lv_event_t *e)
{
    const int delta = (int)(intptr_t)lv_event_get_user_data(e);
    weather_t w;
    weather_get(&w);

    const int page = s_page + delta;
    if (page < 0 || page >= page_count(&w)) {
        return;
    }
    s_page = page;
    render(&w);
}

static lv_obj_t *make_label(lv_obj_t *parent, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(label, ui_color(color), 0);
    return label;
}

static lv_obj_t *create_arrow(lv_obj_t *parent, const char *glyph, int delta)
{
    lv_obj_t *arrow = lv_obj_create(parent);
    lv_obj_remove_style_all(arrow);
    lv_obj_set_size(arrow, ARROW_W, ARROW_H);
    lv_obj_set_style_radius(arrow, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_color(arrow, ui_color(UI_COLOR_SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(arrow, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(arrow, ui_color(UI_COLOR_ACCENT_SOFT), LV_STATE_PRESSED);
    lv_obj_remove_flag(arrow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(arrow);
    lv_label_set_text(label, glyph);
    lv_obj_set_style_text_font(label, UI_FONT_CAPTION, 0);
    lv_obj_center(label);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(arrow, step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
    return arrow;
}

static void create_cell(lv_obj_t *row, int index)
{
    lv_obj_t *cell = lv_obj_create(row);
    lv_obj_remove_style_all(cell);
    lv_obj_add_style(cell, ui_style_transparent(), 0);
    lv_obj_set_height(cell, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(cell, 1);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, UI_SPACE_XS, 0);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    s_cell[index] = cell;
    s_hour[index] = make_label(cell, UI_COLOR_TEXT_MUTED, "--:--");
    s_icon[index] = ui_weather_icon_create(cell, CELL_ICON_SIZE);
    s_temp[index] = make_label(cell, UI_COLOR_TEXT, "--\xC2\xB0");
}

lv_obj_t *ui_hourly_strip_create(lv_obj_t *parent)
{
    lv_obj_t *strip = lv_obj_create(parent);
    lv_obj_remove_style_all(strip);
    lv_obj_add_style(strip, ui_style_transparent(), 0);
    lv_obj_set_size(strip, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(strip, UI_SPACE_SM, 0);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(strip);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, ui_style_transparent(), 0);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, UI_SPACE_SM, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(header, UI_COLOR_TEXT_MUTED, "Next hours");
    lv_obj_set_flex_grow(title, 1);
    s_prev = create_arrow(header, LV_SYMBOL_LEFT, -1);
    s_next = create_arrow(header, LV_SYMBOL_RIGHT, 1);

    lv_obj_t *row = lv_obj_create(strip);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, ui_style_transparent(), 0);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_SPACE_XS, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < VISIBLE_HOURS; i++) {
        create_cell(row, i);
    }

    tick_cb(NULL);
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return strip;
}
