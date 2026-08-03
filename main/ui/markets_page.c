#include "markets_page.h"
#include "label.h"
#include "markets.h"
#include "page.h"
#include "theme.h"
#include "tile_card.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* 720 x 400 inside the page padding: back 44 + 3 x 94 + updated 17 and four 12 px gaps
 * is 391. A row needs its 94 for the 48 px price, which is a 57 px line. No page heading
 * for the same reason - it does not fit, and the tile that opens this already says
 * Crypto. */
#define ROW_HEIGHT     94
#define COIN_SIZE      40
#define NAME_WIDTH     150
#define TICK_PERIOD_MS 5000
#define TEXT_CAP       32
#define PRICE_CAP      12

static lv_obj_t *s_price[MARKETS_ASSET_COUNT];
static lv_obj_t *s_change[MARKETS_ASSET_COUNT];
static lv_obj_t *s_range[MARKETS_ASSET_COUNT];
static lv_obj_t *s_updated;
static lv_obj_t *s_tile;

static char s_price_cache[MARKETS_ASSET_COUNT][TEXT_CAP];
static char s_change_cache[MARKETS_ASSET_COUNT][TEXT_CAP];
static char s_range_cache[MARKETS_ASSET_COUNT][TEXT_CAP];
static char s_updated_cache[TEXT_CAP];
static char s_tile_cache[TEXT_CAP];

static int s_change_dir[MARKETS_ASSET_COUNT];
static int s_tile_dir = -1;

void ui_markets_format_price(float value, char *out, size_t cap)
{
    if (value < 1000.0f) {
        snprintf(out, cap, "%.2f", value);
        return;
    }

    char raw[16];
    snprintf(raw, sizeof(raw), "%.0f", value);

    const size_t len = strlen(raw);
    size_t o = 0;

    for (size_t i = 0; i < len; i++) {
        const bool sep = (i > 0 && (len - i) % 3 == 0);
        if (o + (sep ? 2u : 1u) + 1u > cap) {
            break;
        }
        if (sep) {
            out[o++] = ',';
        }
        out[o++] = raw[i];
    }
    out[o] = '\0';
}

static void set_direction(lv_obj_t *label, int *cache, bool up)
{
    if (*cache == (int)up) {
        return;
    }
    *cache = (int)up;
    lv_obj_set_style_text_color(label, ui_color(up ? UI_COLOR_UP : UI_COLOR_DOWN), 0);
}

static void render_asset(int i, const market_asset_t *asset)
{
    char price[PRICE_CAP];
    char buf[TEXT_CAP];

    if (!asset->valid) {
        ui_label_set(s_price[i], s_price_cache[i], TEXT_CAP, "$--");
        ui_label_set(s_change[i], s_change_cache[i], TEXT_CAP, "");
        ui_label_set(s_range[i], s_range_cache[i], TEXT_CAP, "");
        return;
    }

    ui_markets_format_price(asset->price, price, sizeof(price));
    snprintf(buf, sizeof(buf), "$%s", price);
    ui_label_set(s_price[i], s_price_cache[i], TEXT_CAP, buf);

    const bool up = asset->change_pct >= 0.0f;
    snprintf(buf, sizeof(buf), "%s %+.2f%%", up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
             asset->change_pct);
    ui_label_set(s_change[i], s_change_cache[i], TEXT_CAP, buf);
    set_direction(s_change[i], &s_change_dir[i], up);

    char low[PRICE_CAP];
    ui_markets_format_price(asset->low, low, sizeof(low));
    ui_markets_format_price(asset->high, price, sizeof(price));
    snprintf(buf, sizeof(buf), "24h %s / %s", low, price);
    ui_label_set(s_range[i], s_range_cache[i], TEXT_CAP, buf);
}

static void render_tile(const market_asset_t *asset)
{
    if (!s_tile) {
        return;
    }

    if (!asset->valid) {
        ui_label_set(s_tile, s_tile_cache, TEXT_CAP, "--");
        return;
    }

    char price[PRICE_CAP];
    char buf[TEXT_CAP];

    char symbol[8];
    strlcpy(symbol, markets_symbol(0), sizeof(symbol));

    ui_markets_format_price(asset->price, price, sizeof(price));
    const bool up = asset->change_pct >= 0.0f;
    snprintf(buf, sizeof(buf), "%s $%s  %+.1f%%", symbol, price, asset->change_pct);
    ui_label_set(s_tile, s_tile_cache, TEXT_CAP, buf);
    set_direction(s_tile, &s_tile_dir, up);
}

static void render_updated(const markets_t *m)
{
    if (!m->valid) {
        ui_label_set(s_updated, s_updated_cache, TEXT_CAP, "Waiting for prices");
        return;
    }

    struct tm tm_updated;
    char buf[TEXT_CAP];

    localtime_r(&m->updated, &tm_updated);
    strftime(buf, sizeof(buf), "Updated %H:%M", &tm_updated);
    ui_label_set(s_updated, s_updated_cache, TEXT_CAP, buf);
}

static void render(void)
{
    markets_t m = { 0 };

    markets_get(&m);
    for (int i = 0; i < MARKETS_ASSET_COUNT; i++) {
        render_asset(i, &m.asset[i]);
    }
    render_tile(&m.asset[0]);
    render_updated(&m);
}

static void tick_cb(lv_timer_t *timer)
{
    render();
}

static lv_obj_t *make_column(lv_obj_t *parent, lv_flex_align_t cross)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_add_style(box, ui_style_transparent(), 0);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, cross, cross);
    lv_obj_set_style_pad_row(box, UI_SPACE_XS, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static void create_coin(lv_obj_t *tile)
{
    lv_obj_t *coin = lv_obj_create(tile);
    lv_obj_remove_style_all(coin);
    lv_obj_set_size(coin, COIN_SIZE, COIN_SIZE);
    lv_obj_set_style_radius(coin, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(coin, ui_color(UI_COLOR_BTC), 0);
    lv_obj_set_style_bg_opa(coin, LV_OPA_COVER, 0);
    lv_obj_remove_flag(coin, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(coin, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(coin);

    lv_obj_t *mark = ui_label(tile, UI_FONT_CAPTION, UI_COLOR_TEXT, "BTC");
    lv_obj_center(mark);
    lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
}

static void create_row(lv_obj_t *page, int index)
{
    lv_obj_t *row = lv_obj_create(page);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, ui_style_card(), 0);
    lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_style_pad_hor(row, UI_SPACE_XL, 0);
    lv_obj_set_style_pad_ver(row, UI_SPACE_MD, 0);
    lv_obj_set_style_pad_column(row, UI_SPACE_LG, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = make_column(row, LV_FLEX_ALIGN_START);
    lv_obj_set_width(name, NAME_WIDTH);
    ui_label(name, UI_FONT_TITLE, UI_COLOR_TEXT, markets_symbol(index));
    ui_label(name, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, markets_name(index));

    s_price[index] = ui_label(row, UI_FONT_CLOCK, UI_COLOR_TEXT, "$--");
    lv_obj_set_flex_grow(s_price[index], 1);

    lv_obj_t *stats = make_column(row, LV_FLEX_ALIGN_END);
    s_change[index] = ui_label(stats, UI_FONT_BODY, UI_COLOR_UP, "");
    s_range[index] = ui_label(stats, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");
    s_change_dir[index] = 1;
}

void ui_markets_bind_tile(lv_obj_t *card)
{
    s_tile = ui_tile_card_info_label(card);
    create_coin(ui_tile_card_icon_tile(card));
}

lv_obj_t *ui_markets_page_create(lv_obj_t *parent, lv_event_cb_t on_back, void *user_data)
{
    lv_obj_t *page = ui_page_create(parent, NULL, on_back, user_data);
    lv_obj_set_style_pad_row(page, UI_SPACE_MD, 0);

    for (int i = 0; i < MARKETS_ASSET_COUNT; i++) {
        create_row(page, i);
    }

    s_updated = ui_label(page, UI_FONT_CAPTION, UI_COLOR_TEXT_MUTED, "");

    render();
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

    return page;
}
