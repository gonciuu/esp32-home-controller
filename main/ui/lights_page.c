#include "lights_page.h"
#include "ha_lights.h"
#include "label.h"
#include "page.h"
#include "theme.h"
#include "tile_card.h"

#include <stdio.h>
#include <time.h>

/* 720 x 400 inside the page padding: back 44 + 4 x 68 + footer 17 and five 12 px gaps is
 * 393. That caps the page at four rows, so discovery may return more lights than fit -
 * the footer counts the remainder. Going past four means switching to the sysinfo grid.
 * No page heading, for the same reason as markets: the tile that opens this says Lights. */
#define ROW_HEIGHT     68
#define VISIBLE_ROWS   4
#define DOT_SIZE       40
#define TICK_PERIOD_MS 1000
#define TEXT_CAP       48
#define NAME_CAP       24
#define STATE_CAP      8

enum { STATE_OFF, STATE_ON, STATE_UNAVAILABLE };

static lv_obj_t *s_row[VISIBLE_ROWS];
static lv_obj_t *s_dot[VISIBLE_ROWS];
static lv_obj_t *s_name[VISIBLE_ROWS];
static lv_obj_t *s_state[VISIBLE_ROWS];
static lv_obj_t *s_footer;
static lv_obj_t *s_tile;

static char s_name_cache[VISIBLE_ROWS][NAME_CAP];
static char s_state_cache[VISIBLE_ROWS][STATE_CAP];
static char s_footer_cache[TEXT_CAP];
static char s_tile_cache[TEXT_CAP];

static int s_state_dir[VISIBLE_ROWS];
static int s_shown[VISIBLE_ROWS];

static void render(void);

static void set_shown(int i, bool shown)
{
    if (s_shown[i] == (int)shown) {
        return;
    }
    s_shown[i] = (int)shown;

    if (shown) {
        lv_obj_remove_flag(s_row[i], LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(s_row[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_state(int i, int state)
{
    if (s_state_dir[i] == state) {
        return;
    }
    s_state_dir[i] = state;

    const uint32_t dot = (state == STATE_ON) ? UI_COLOR_SUN : UI_COLOR_SURFACE_HI;
    const uint32_t text = (state == STATE_ON)            ? UI_COLOR_SUN
                          : (state == STATE_UNAVAILABLE) ? UI_COLOR_DOWN
                                                         : UI_COLOR_TEXT_MUTED;

    lv_obj_set_style_bg_color(s_dot[i], ui_color(dot), 0);
    lv_obj_set_style_text_color(s_state[i], ui_color(text), 0);
}

static void render_row(int i, const ha_lights_t *lights)
{
    const bool shown = (i < lights->count);

    set_shown(i, shown);
    if (!shown) {
        return;
    }

    const ha_light_t *light = &lights->light[i];

    ui_label_set(s_name[i], s_name_cache[i], NAME_CAP, light->name);

    if (!light->valid) {
        ui_label_set(s_state[i], s_state_cache[i], STATE_CAP, "N/A");
        set_state(i, STATE_UNAVAILABLE);
        return;
    }
    ui_label_set(s_state[i], s_state_cache[i], STATE_CAP, light->on ? "ON" : "OFF");
    set_state(i, light->on ? STATE_ON : STATE_OFF);
}

static void render_footer(const ha_lights_t *lights)
{
    if (!lights->valid) {
        ui_label_set(s_footer, s_footer_cache, TEXT_CAP, "Connecting to Home Assistant");
        return;
    }
    if (lights->count == 0) {
        ui_label_set(s_footer, s_footer_cache, TEXT_CAP, "No lights found");
        return;
    }

    struct tm tm_updated;
    char      stamp[16];
    char      buf[TEXT_CAP];

    localtime_r(&lights->updated, &tm_updated);
    strftime(stamp, sizeof(stamp), "%H:%M", &tm_updated);

    if (lights->count > VISIBLE_ROWS) {
        snprintf(buf, sizeof(buf), "Updated %s  +%d more", stamp, lights->count - VISIBLE_ROWS);
    }
    else {
        snprintf(buf, sizeof(buf), "Updated %s", stamp);
    }
    ui_label_set(s_footer, s_footer_cache, TEXT_CAP, buf);
}

static void render_tile(const ha_lights_t *lights)
{
    if (!s_tile) {
        return;
    }

    if (!lights->valid) {
        ui_label_set(s_tile, s_tile_cache, TEXT_CAP, "--");
        return;
    }
    if (lights->count == 0) {
        ui_label_set(s_tile, s_tile_cache, TEXT_CAP, "No lights");
        return;
    }
    if (lights->on_count == 0) {
        ui_label_set(s_tile, s_tile_cache, TEXT_CAP, "All off");
        return;
    }

    char buf[TEXT_CAP];

    snprintf(buf, sizeof(buf), "%d of %d on", lights->on_count, lights->count);
    ui_label_set(s_tile, s_tile_cache, TEXT_CAP, buf);
}

static void render(void)
{
    ha_lights_t lights = { 0 };

    ha_lights_get(&lights);
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        render_row(i, &lights);
    }
    render_footer(&lights);
    render_tile(&lights);
}

static void tick_cb(lv_timer_t *timer)
{
    render();
}

static void row_event_cb(lv_event_t *e)
{
    const uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    ha_lights_toggle(index);
    render();
}

static void create_row(lv_obj_t *page, int index)
{
    lv_obj_t *row = lv_obj_create(page);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, ui_style_card(), 0);
    lv_obj_add_style(row, ui_style_card_pressed(), LV_STATE_PRESSED);
    lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_style_pad_hor(row, UI_SPACE_XL, 0);
    lv_obj_set_style_pad_ver(row, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_column(row, UI_SPACE_LG, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, ui_color(UI_COLOR_SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *name = ui_label(row, UI_FONT_TITLE, UI_COLOR_TEXT, "");
    lv_obj_set_flex_grow(name, 1);
    lv_obj_remove_flag(name, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *state = ui_label(row, UI_FONT_HEADING, UI_COLOR_TEXT_MUTED, "OFF");
    lv_obj_remove_flag(state, LV_OBJ_FLAG_CLICKABLE);

    s_row[index] = row;
    s_dot[index] = dot;
    s_name[index] = name;
    s_state[index] = state;
    s_state_dir[index] = STATE_OFF;
    s_shown[index] = 1;
}

void ui_lights_bind_tile(lv_obj_t *card)
{
    s_tile = ui_tile_card_info_label(card);
}

void ui_lights_page_refresh(void)
{
    ha_lights_refresh();
    render();
}

lv_obj_t *ui_lights_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data)
{
    lv_obj_t *page = ui_page_create(parent, NULL, on_back, user_data);
    lv_obj_set_style_pad_row(page, UI_SPACE_MD, 0);

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        create_row(page, i);
    }

    s_footer = ui_label(page, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");

    render();
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return page;
}
