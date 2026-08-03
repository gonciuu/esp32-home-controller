#include "dashboard.h"
#include "lights_page.h"
#include "markets_page.h"
#include "page.h"
#include "screensaver.h"
#include "sysinfo_page.h"
#include "theme.h"
#include "tile_card.h"
#include "top_bar.h"
#include "weather_page.h"

/* 800x480 less the screen's 20 pad is 760x440: top bar 56 + 16 gap + 368 content.
 * The grid runs edge to edge, 3*245 + 2*12 = 759 of 760 across and 2*178 + 12 = 368
 * of the 368 down. Nothing scrolls. */
#define CONTENT_HEIGHT (440 - UI_TOP_BAR_HEIGHT - UI_SPACE_LG)

static void weather_event_cb(lv_event_t *e);
static void markets_event_cb(lv_event_t *e);
static void sysinfo_event_cb(lv_event_t *e);
static void lights_event_cb(lv_event_t *e);

typedef struct {
    const char   *icon;
    const char   *label;
    const char   *info;
    uint32_t      accent;
    lv_event_cb_t on_click;   
} ui_section_t;

static const ui_section_t s_sections[] = {
    { LV_SYMBOL_HOME,     "Home",     "All quiet",   0x4C8DFF, NULL },
    { LV_SYMBOL_CHARGE,   "Lights",   "--",          0xFFB020, lights_event_cb },
    { LV_SYMBOL_TINT,     "Climate",  "--",          0x35C6E8, weather_event_cb },
    { LV_SYMBOL_SD_CARD,  "System",   "--",          0xA46BFF, sysinfo_event_cb },
    { LV_SYMBOL_SETTINGS, "Settings", "Up to date",  0x8A93A3, NULL },
    { NULL,               "Crypto",   "--",          UI_COLOR_BTC, markets_event_cb },
};

#define UI_SECTION_COUNT (sizeof(s_sections) / sizeof(s_sections[0]))

static lv_obj_t *s_top_bar;
static lv_obj_t *s_content;
static lv_obj_t *s_weather;
static lv_obj_t *s_markets;
static lv_obj_t *s_sysinfo;
static lv_obj_t *s_lights;
static lv_obj_t *s_grid;
static lv_obj_t *s_pages[UI_SECTION_COUNT];

static void show_grid(void)
{
    for (uint32_t i = 0; i < UI_SECTION_COUNT; i++) {
        if (s_pages[i]) {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_add_flag(s_markets, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_sysinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_lights, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
}

static void tile_event_cb(lv_event_t *e)
{
    const uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    lv_obj_add_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_pages[index], LV_OBJ_FLAG_HIDDEN);
}

static void back_event_cb(lv_event_t *e)
{
    show_grid();
}

static void show_overlay(lv_obj_t *page)
{
    lv_obj_add_flag(s_top_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_HIDDEN);
}

static void hide_overlay(lv_obj_t *page)
{
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_top_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_HIDDEN);
    show_grid();
}

static void weather_event_cb(lv_event_t *e)
{
    ui_weather_page_select_today();
    show_overlay(s_weather);
}

static void weather_back_cb(lv_event_t *e)
{
    hide_overlay(s_weather);
}

static void markets_event_cb(lv_event_t *e)
{
    show_overlay(s_markets);
}

static void markets_back_cb(lv_event_t *e)
{
    hide_overlay(s_markets);
}

static void sysinfo_event_cb(lv_event_t *e)
{
    show_overlay(s_sysinfo);
}

static void sysinfo_back_cb(lv_event_t *e)
{
    hide_overlay(s_sysinfo);
}

static void lights_event_cb(lv_event_t *e)
{
    ui_lights_page_refresh();
    show_overlay(s_lights);
}

static void lights_back_cb(lv_event_t *e)
{
    hide_overlay(s_lights);
}

void ui_dashboard_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, ui_style_screen(), 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(screen, UI_SPACE_LG, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    s_top_bar = ui_top_bar_create(screen, weather_event_cb, NULL);

    s_content = lv_obj_create(screen);
    lv_obj_remove_style_all(s_content);
    lv_obj_add_style(s_content, ui_style_transparent(), 0);
    lv_obj_set_size(s_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    s_grid = lv_obj_create(s_content);
    lv_obj_remove_style_all(s_grid);
    lv_obj_add_style(s_grid, ui_style_transparent(), 0);
    lv_obj_set_size(s_grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_grid, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_column(s_grid, UI_SPACE_MD, 0);
    lv_obj_remove_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);

    for (uint32_t i = 0; i < UI_SECTION_COUNT; i++) {
        const bool own_page = s_sections[i].on_click != NULL;

        lv_obj_t *card = ui_tile_card_create(s_grid, s_sections[i].icon, s_sections[i].label,
                                             s_sections[i].info, s_sections[i].accent,
                                             own_page ? s_sections[i].on_click : tile_event_cb,
                                             (void *)(uintptr_t)i);
        s_pages[i] = own_page
                         ? NULL
                         : ui_page_create(s_content, s_sections[i].label, back_event_cb, NULL);

        if (s_sections[i].on_click == markets_event_cb) {
            ui_markets_bind_tile(card);
        }
        else if (s_sections[i].on_click == weather_event_cb) {
            ui_weather_page_bind_tile(card);
        }
        else if (s_sections[i].on_click == sysinfo_event_cb) {
            ui_sysinfo_bind_tile(card);
        }
        else if (s_sections[i].on_click == lights_event_cb) {
            ui_lights_bind_tile(card);
        }
    }

    s_weather = ui_weather_page_create(screen, weather_back_cb, NULL);
    lv_obj_add_flag(s_weather, LV_OBJ_FLAG_HIDDEN);

    s_markets = ui_markets_page_create(screen, markets_back_cb, NULL);
    s_sysinfo = ui_sysinfo_page_create(screen, sysinfo_back_cb, NULL);
    s_lights = ui_lights_page_create(screen, lights_back_cb, NULL);

    show_grid();

    /* Last child, so it covers the top bar and every page. It is deliberately not part of
     * s_pages[] — waking must restore whatever was open, not fall back to the grid. */
    ui_screensaver_create(screen);
}
