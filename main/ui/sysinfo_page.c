#include "sysinfo_page.h"
#include "label.h"
#include "page.h"
#include "sysinfo.h"
#include "theme.h"
#include "tile_card.h"

#include <stdio.h>
#include <string.h>

/* 720 x 400 inside the page padding: back 44 + grid 312 + footer 17 and two 12 px gaps is
 * 397. The grid is 5 x 134 + 4 x 12 = 718 across and 2 x 150 + 12 = 312 down. No page
 * heading - the tile that opens this already says System, and the row would not fit.
 * A third row does not fit either, so ten cards is the ceiling: card height would have to
 * drop to ~97 and there is no room for title + value + bar + subtitle in that.
 * 134 wide with UI_SPACE_MD padding leaves 110 for text, and the subtitles are cut to fit
 * it - every one of them is a line that wraps and overflows the card if it grows. */
#define CARD_W         134
#define CARD_H         150
#define GRID_H         (2 * CARD_H + UI_SPACE_MD)
#define BAR_HEIGHT     8
#define TICK_PERIOD_MS 2000
/* Sized for what gcc thinks the strings can be, not for what they are. -Werror=format-truncation
 * bounds every %s by its array size and every %u by ten digits, so the LVGL heap subtitle alone
 * has to fit "4294967295/4294967295 KB * 4294967295%". */
#define TEXT_CAP       48
#define NUM_CAP        16

/* U+2022 BULLET, one of the extra codepoints the ui_font_* set was generated with. */
#define SEP "\xE2\x80\xA2"

enum {
    CARD_CPU,
    CARD_TEMP,
    CARD_POWER,
    CARD_VOLTAGE,
    CARD_UPTIME,
    CARD_RAM,
    CARD_RAM_MIN,
    CARD_PSRAM,
    CARD_LVGL,
    CARD_WIFI,
    CARD_COUNT,
};

static const char *s_titles[CARD_COUNT] = {
    "CPU LOAD",     "TEMPERATURE",   "POWER (EST.)", "RAIL VOLTAGE", "UPTIME",
    "INTERNAL RAM", "RAM LOW-WATER", "PSRAM",        "LVGL HEAP",    "WI-FI",
};

typedef struct {
    lv_obj_t *value;
    lv_obj_t *bar;
    lv_obj_t *sub;
    char      value_cache[TEXT_CAP];
    char      sub_cache[TEXT_CAP];
    int       bar_cache;
    uint32_t  color_cache;
} stat_card_t;

static stat_card_t s_card[CARD_COUNT];
static lv_obj_t   *s_tile;

static char s_tile_cache[TEXT_CAP];

static uint32_t level_color(int pct)
{
    if (pct < 70) {
        return UI_COLOR_UP;
    }
    return pct < 85 ? UI_COLOR_SUN : UI_COLOR_DOWN;
}

static int used_pct(size_t remaining, size_t total)
{
    if (total == 0) {
        return 0;
    }
    return (int)(100 - (uint64_t)remaining * 100 / total);
}

static void set_card(int index, const char *value, const char *sub, int bar_pct, uint32_t color)
{
    stat_card_t *c = &s_card[index];

    ui_label_set(c->value, c->value_cache, TEXT_CAP, value);
    ui_label_set(c->sub, c->sub_cache, TEXT_CAP, sub);

    if (bar_pct < 0) {
        bar_pct = 0;
    }
    else if (bar_pct > 100) {
        bar_pct = 100;
    }

    if (c->bar_cache != bar_pct) {
        c->bar_cache = bar_pct;
        lv_bar_set_value(c->bar, bar_pct, LV_ANIM_OFF);
    }

    if (c->color_cache != color) {
        c->color_cache = color;
        lv_obj_set_style_text_color(c->value, ui_color(color), 0);
        lv_obj_set_style_bg_color(c->bar, ui_color(color), LV_PART_INDICATOR);
    }
}

static void format_size(size_t bytes, char *out, size_t cap)
{
    if (bytes >= 1024 * 1024) {
        snprintf(out, cap, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
        return;
    }
    snprintf(out, cap, "%u KB", (unsigned)(bytes / 1024));
}

/* Strips the unit off the first half of a "198 KB / 320 KB" pair - the two always share it. */
static void format_pair(size_t remaining, size_t total, char *out, size_t cap)
{
    char left[NUM_CAP];
    char right[NUM_CAP];

    format_size(remaining, left, sizeof(left));
    format_size(total, right, sizeof(right));

    char *unit = strchr(left, ' ');
    if (unit) {
        *unit = '\0';
    }
    snprintf(out, cap, "%s / %s", left, right);
}

static void render_memory(int index, size_t remaining, size_t total)
{
    char value[TEXT_CAP];
    char sub[TEXT_CAP];

    const int pct = used_pct(remaining, total);

    snprintf(value, sizeof(value), "%d%%", pct);
    format_pair(remaining, total, sub, sizeof(sub));
    set_card(index, value, sub, pct, level_color(pct));
}

static void render_ram_min(const sysinfo_t *info)
{
    char value[NUM_CAP];

    const int pct = used_pct(info->ram_min_free, info->ram_total);

    format_size(info->ram_min_free, value, sizeof(value));
    set_card(CARD_RAM_MIN, value, "min free", pct, level_color(pct));
}

#define POWER_FULL_SCALE_W  3.0f
#define CURRENT_FULL_SCALE  800.0f

static void render_power(const sysinfo_t *info)
{
    char value[TEXT_CAP];
    char sub[TEXT_CAP];

    snprintf(value, sizeof(value), "%.2f W", info->power_w);
    snprintf(sub, sizeof(sub), "est. @ %.1f V", info->rail_v);
    set_card(CARD_POWER, value, sub, (int)(info->power_w * 100.0f / POWER_FULL_SCALE_W),
             UI_COLOR_SUN);
}

static void render_voltage(const sysinfo_t *info)
{
    char value[TEXT_CAP];
    char sub[TEXT_CAP];

    snprintf(value, sizeof(value), "%.2f V", info->rail_v);
    snprintf(sub, sizeof(sub), "est. %.0f mA", info->current_ma);
    set_card(CARD_VOLTAGE, value, sub, (int)(info->current_ma * 100.0f / CURRENT_FULL_SCALE),
             UI_COLOR_UP);
}

static void render_cpu(const sysinfo_t *info)
{
    char value[TEXT_CAP];
    char sub[TEXT_CAP];

    if (!info->cpu_valid) {
        set_card(CARD_CPU, "--", "sampling", 0, UI_COLOR_TEXT_MUTED);
        return;
    }

    snprintf(value, sizeof(value), "%d%%", info->cpu_pct);
    snprintf(sub, sizeof(sub), "cores %d/%d%%", info->cpu_core_pct[0], info->cpu_core_pct[1]);
    set_card(CARD_CPU, value, sub, info->cpu_pct, level_color(info->cpu_pct));
}

static void render_temp(const sysinfo_t *info)
{
    char value[TEXT_CAP];

    if (!info->temp_valid) {
        set_card(CARD_TEMP, "--", "sensor offline", 0, UI_COLOR_TEXT_MUTED);
        return;
    }

    const int t = (int)info->temp_c;
    const uint32_t color = t < 60 ? UI_COLOR_UP : (t < 75 ? UI_COLOR_SUN : UI_COLOR_DOWN);

    snprintf(value, sizeof(value), "%.1f\xC2\xB0" "C", info->temp_c);
    set_card(CARD_TEMP, value, "SoC internal", (int)((info->temp_c - 20.0f) * 100.0f / 60.0f),
             color);
}

static void render_lvgl(void)
{
    lv_mem_monitor_t mon;
    char value[TEXT_CAP];
    char sub[TEXT_CAP];

    lv_mem_monitor(&mon);

    const int pct = used_pct(mon.free_size, mon.total_size);

    snprintf(value, sizeof(value), "%d%%", pct);
    snprintf(sub, sizeof(sub), "%u/%uKB " SEP " %u%%", (unsigned)(mon.free_size / 1024),
             (unsigned)(mon.total_size / 1024), (unsigned)mon.frag_pct);
    set_card(CARD_LVGL, value, sub, pct, level_color(pct));
}

static void render_wifi(const sysinfo_t *info)
{
    char value[TEXT_CAP];

    if (!info->wifi_valid) {
        set_card(CARD_WIFI, "--", "offline", 0, UI_COLOR_TEXT_MUTED);
        return;
    }

    /* -100 dBm is unusable, -50 is as good as it gets indoors. */
    int quality = 2 * (info->rssi + 100);

    if (quality < 0) {
        quality = 0;
    }
    else if (quality > 100) {
        quality = 100;
    }

    snprintf(value, sizeof(value), "%d dBm", info->rssi);
    /* Inverted: on this card a high number is the good one. */
    set_card(CARD_WIFI, value, info->ip, quality, level_color(100 - quality));
}

static void render_uptime(const sysinfo_t *info)
{
    const int total = (int)info->uptime_s;
    const int days = total / 86400;
    const int hours = (total % 86400) / 3600;
    const int mins = (total % 3600) / 60;
    const int secs = total % 60;

    char value[TEXT_CAP];

    if (days > 0) {
        snprintf(value, sizeof(value), "%dd %dh", days, hours);
    }
    else if (hours > 0) {
        snprintf(value, sizeof(value), "%dh %dm", hours, mins);
    }
    else {
        snprintf(value, sizeof(value), "%dm %ds", mins, secs);
    }

    set_card(CARD_UPTIME, value, sysinfo_reset_text(), 0, UI_COLOR_TEXT);
}

static void render_tile(const sysinfo_t *info)
{
    if (!s_tile) {
        return;
    }

    char buf[TEXT_CAP];

    if (!info->cpu_valid) {
        ui_label_set(s_tile, s_tile_cache, TEXT_CAP, "--");
        return;
    }

    if (info->temp_valid) {
        snprintf(buf, sizeof(buf), "%d%% CPU " SEP " %.0f\xC2\xB0" "C", info->cpu_pct,
                 info->temp_c);
    }
    else {
        snprintf(buf, sizeof(buf), "%d%% CPU", info->cpu_pct);
    }
    ui_label_set(s_tile, s_tile_cache, TEXT_CAP, buf);
}

static void render(void)
{
    sysinfo_t info;

    sysinfo_sample(&info);

    render_cpu(&info);
    render_temp(&info);
    render_power(&info);
    render_voltage(&info);
    render_memory(CARD_RAM, info.ram_free, info.ram_total);
    render_ram_min(&info);
    render_memory(CARD_PSRAM, info.psram_free, info.psram_total);
    render_lvgl();
    render_wifi(&info);
    render_uptime(&info);
    render_tile(&info);
}

static void tick_cb(lv_timer_t *timer)
{
    render();
}

static lv_obj_t *make_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), BAR_HEIGHT);
    lv_obj_set_style_bg_color(bar, ui_color(UI_COLOR_SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar, ui_color(UI_COLOR_UP), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    return bar;
}

static void create_card(lv_obj_t *grid, int index)
{
    stat_card_t *c = &s_card[index];

    lv_obj_t *card = lv_obj_create(grid);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_style_pad_all(card, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_row(card, UI_SPACE_SM, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    ui_label(card, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, s_titles[index]);
    c->value = ui_label(card, UI_FONT_HEADING, UI_COLOR_TEXT, "--");
    c->bar = make_bar(card);
    c->sub = ui_label(card, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");

    c->bar_cache = -1;

    /* Uptime has no meaningful scale. Hidden rather than absent so the builder stays one
     * function; a hidden flex child takes no space. */
    if (index == CARD_UPTIME) {
        lv_obj_add_flag(c->bar, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_sysinfo_bind_tile(lv_obj_t *card)
{
    s_tile = ui_tile_card_info_label(card);
}

lv_obj_t *ui_sysinfo_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data)
{
    lv_obj_t *page = ui_page_create(parent, NULL, on_back, user_data);
    lv_obj_set_style_pad_row(page, UI_SPACE_MD, 0);

    lv_obj_t *grid = lv_obj_create(page);
    lv_obj_remove_style_all(grid);
    lv_obj_add_style(grid, ui_style_transparent(), 0);
    lv_obj_set_size(grid, LV_PCT(100), GRID_H);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_column(grid, UI_SPACE_MD, 0);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < CARD_COUNT; i++) {
        create_card(grid, i);
    }

    /* Fixed for the life of the board, so it is written once and never ticked. */
    ui_label(page, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, sysinfo_chip_text());

    render();
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return page;
}
