#include "dashboard.h"
#include "menu_card.h"
#include "page.h"
#include "theme.h"

/* 20 + 384 + 20 + 356 + 20 = 800 across; 52 + 5*68 + 5*8 = 432 of the 440
 * available down the left column, so nothing ever needs to scroll. */
#define MENU_WIDTH      384
#define CONTENT_WIDTH   356
#define MENU_HEADER_H   52

typedef struct {
    const char *icon;
    const char *label;
    uint32_t    accent;
} ui_section_t;

static const ui_section_t s_sections[] = {
    { LV_SYMBOL_HOME,     "Home",     0x4C8DFF },
    { LV_SYMBOL_CHARGE,   "Lights",   0xFFB020 },
    { LV_SYMBOL_TINT,     "Climate",  0x35C6E8 },
    { LV_SYMBOL_AUDIO,    "Media",    0xA46BFF },
    { LV_SYMBOL_SETTINGS, "Settings", 0x8A93A3 },
};

#define UI_SECTION_COUNT (sizeof(s_sections) / sizeof(s_sections[0]))

static lv_obj_t *s_cards[UI_SECTION_COUNT];
static lv_obj_t *s_pages[UI_SECTION_COUNT];
static uint32_t  s_active;

static void select_section(uint32_t index)
{
    if (index == s_active) {
        return;
    }

    ui_menu_card_set_selected(s_cards[s_active], false);
    lv_obj_add_flag(s_pages[s_active], LV_OBJ_FLAG_HIDDEN);

    s_active = index;

    ui_menu_card_set_selected(s_cards[s_active], true);
    lv_obj_remove_flag(s_pages[s_active], LV_OBJ_FLAG_HIDDEN);
}

static void menu_event_cb(lv_event_t *e)
{
    select_section((uint32_t)(uintptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *create_menu_header(lv_obj_t *parent)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, ui_style_transparent(), 0);
    lv_obj_set_size(header, LV_PCT(100), MENU_HEADER_H);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(header, UI_SPACE_XS, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Home Controller");
    lv_obj_set_style_text_font(title, UI_FONT_HEADING, 0);
    lv_obj_set_style_text_color(title, ui_color(UI_COLOR_TEXT), 0);

    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, "Living room panel");
    lv_obj_set_style_text_font(subtitle, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(subtitle, ui_color(UI_COLOR_TEXT_MUTED), 0);

    return header;
}

void ui_dashboard_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, ui_style_screen(), 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *menu = lv_obj_create(screen);
    lv_obj_remove_style_all(menu);
    lv_obj_add_style(menu, ui_style_transparent(), 0);
    lv_obj_set_size(menu, MENU_WIDTH, LV_PCT(100));
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(menu, UI_SPACE_SM, 0);
    lv_obj_remove_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    create_menu_header(menu);

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_add_style(content, ui_style_surface(), 0);
    lv_obj_set_size(content, CONTENT_WIDTH, LV_PCT(100));
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    for (uint32_t i = 0; i < UI_SECTION_COUNT; i++) {
        s_cards[i] = ui_menu_card_create(menu, s_sections[i].icon, s_sections[i].label,
                                         s_sections[i].accent, menu_event_cb,
                                         (void *)(uintptr_t)i);
        s_pages[i] = ui_page_create(content, s_sections[i].label);
        lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_active = 0;
    ui_menu_card_set_selected(s_cards[0], true);
    lv_obj_remove_flag(s_pages[0], LV_OBJ_FLAG_HIDDEN);
}
