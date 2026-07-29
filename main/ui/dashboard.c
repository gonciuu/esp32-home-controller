#include "dashboard.h"
#include "page.h"
#include "status_panel.h"
#include "theme.h"
#include "tile_card.h"

/* 20 + 360 + 20 + 380 = 780, plus the screen's 20 right pad. The grid runs edge
 * to edge in the right column: 2*183 + 12 = 378 of 380 across, 3*138 + 2*12 =
 * 438 of the 440 available down. Nothing scrolls. */
#define CONTENT_WIDTH 380

typedef struct {
    const char *icon;
    const char *label;
    const char *info;
    uint32_t    accent;
} ui_section_t;

static const ui_section_t s_sections[] = {
    { LV_SYMBOL_HOME,     "Home",     "All quiet",   0x4C8DFF },
    { LV_SYMBOL_CHARGE,   "Lights",   "3 on",        0xFFB020 },
    { LV_SYMBOL_TINT,     "Climate",  "21.5\xC2\xB0" "C", 0x35C6E8 },
    { LV_SYMBOL_AUDIO,    "Media",    "Idle",        0xA46BFF },
    { LV_SYMBOL_SETTINGS, "Settings", "Up to date",  0x8A93A3 },
};

#define UI_SECTION_COUNT (sizeof(s_sections) / sizeof(s_sections[0]))

static lv_obj_t *s_grid;
static lv_obj_t *s_pages[UI_SECTION_COUNT];

static void show_grid(void)
{
    for (uint32_t i = 0; i < UI_SECTION_COUNT; i++) {
        lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
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

void ui_dashboard_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, ui_style_screen(), 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_status_panel_create(screen);

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_add_style(content, ui_style_transparent(), 0);
    lv_obj_set_size(content, CONTENT_WIDTH, LV_PCT(100));
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    s_grid = lv_obj_create(content);
    lv_obj_remove_style_all(s_grid);
    lv_obj_add_style(s_grid, ui_style_transparent(), 0);
    lv_obj_set_size(s_grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_grid, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_column(s_grid, UI_SPACE_MD, 0);
    lv_obj_remove_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);

    for (uint32_t i = 0; i < UI_SECTION_COUNT; i++) {
        ui_tile_card_create(s_grid, s_sections[i].icon, s_sections[i].label,
                            s_sections[i].info, s_sections[i].accent, tile_event_cb,
                            (void *)(uintptr_t)i);
        s_pages[i] = ui_page_create(content, s_sections[i].label, back_event_cb, NULL);
    }

    show_grid();
}
