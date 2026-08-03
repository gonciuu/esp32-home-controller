# home_controller

Touchscreen home-control firmware for an **ESP32-S3** driving a **Waveshare 7" 800×480 RGB LCD** with **GT911** capacitive touch. Built on **ESP-IDF v6.0.1** with **LVGL 9.5** as the GUI toolkit.

This is embedded C firmware, not a web app. There is no HTML/CSS/JS anywhere — "styling" means LVGL style objects in C.

## Build and flash

**Do not run builds. The user builds and flashes, always.** Never invoke `idf.py build`, `flash`, `monitor`, or `reconfigure` — make the code and config changes, then hand back and say what needs rebuilding. Ask the user for build output or serial logs instead of producing them yourself.

For reference, the user's commands (ESP-IDF must be on PATH via `C:\esp\v6.0.1\esp-idf\export.ps1`):

```
idf.py build
idf.py -p COM4 flash monitor
```

Target is `esp32s3`; the serial port (`COM4`) is set in `.vscode/settings.json`.

`sdkconfig` is generated and gitignored. **Board and project config belongs in `sdkconfig.defaults`**; secrets and per-installation values (WiFi, HA token, coordinates) belong in `sdkconfig.local`, which is also gitignored and appended to `SDKCONFIG_DEFAULTS` by the root `CMakeLists.txt`. `sdkconfig.local.example` documents its shape.

Both are *defaults* files, and defaults only seed options that `sdkconfig` does not already carry. Editing either one does **not** change an existing `sdkconfig` — they take effect once, on the first build that sees a given option, and never again. Since every setting this project cares about lives in those two files, the fix is to delete `sdkconfig` and rebuild; nothing is lost. `idf.py menuconfig` also works for a quick experiment, but move the value into one of the two files afterwards or the next deletion drops it.

That one-shot behaviour makes compile-time config a poor home for anything tuned repeatedly — prefer a runtime setting stored in NVS.

## Layout

```
main/
  app_main.c          entry point: bring up display and net, build the UI
  net/
    wifi_sta.h/.c     station bring-up
    net_lock.h/.c     one-at-a-time mutex around the HTTPS pollers
    time_sync.h/.c    SNTP + TZ
    weather.h/.c      Open-Meteo poller, snapshot behind a mutex
    markets.h/.c      Binance poller (BTC/ETH/SOL), snapshot behind a mutex
  system/
    sysinfo.h/.c      local board telemetry: CPU load, SoC temperature, heaps, uptime, link
  ui/
    theme.h/.c        design system — tokens + shared lv_style_t objects
    label.h/.c        ui_label() builder + ui_label_set() cached write
    tile_card.h/.c    reusable touch tile (icon tile, label, subtitle)
    weather_icon.h/.c vector-ish condition icons built from nested lv_obj
    hourly_strip.h/.c paged 24-hour row for one day (singleton, one instance only)
    top_bar.h/.c      56 px bar: clock, date, city, temp button, wifi
    weather_page.h/.c fullscreen weather detail: hero, 5 selectable days, hourly
    markets_page.h/.c fullscreen crypto detail: one price row per asset, plus the live
                      subtitle it writes onto the dashboard's Crypto tile
    sysinfo_page.h/.c fullscreen diagnostics behind the System tile: 4x2 stat cards
    page.h/.c         section page stub (back row + optional heading)
    dashboard.h/.c    the screen shell: top bar over a 3x2 tile grid
components/
  bsp_waveshare_lcd7/   board support: I2C, CH422G expander, RGB panel,
                        GT911 touch, LVGL port bring-up
```

New UI `.c` files must be added to `SRCS` in `main/CMakeLists.txt`.

The telemetry directory is `system/`, not `sys/`: `main` is already on the include path, so a `main/sys/` would sit in front of the toolchain's `<sys/...>` headers.

## Rules that will bite you

**Hold the LVGL lock.** Every `lv_*` call made from outside the LVGL task must be wrapped in `bsp_display_lock(0)` / `bsp_display_unlock()` (see `components/bsp_waveshare_lcd7/include/bsp.h`). Forgetting this produces intermittent corruption, not a clean crash.

**Fonts must be enabled in sdkconfig before use.** Only the sizes with `CONFIG_LV_FONT_MONTSERRAT_<n>=y` are compiled in — currently 14, 18, 22, 28. Referencing any other `lv_font_montserrat_*` fails at link time with an undefined reference.

**No transforms on large objects.** Any `transform_scale != 256`, `transform_rotation`, or skew makes LVGL render that object into an intermediate layer buffer (`calculate_layer_type()` in `lv_obj_style.c`). At RGB565 a full-width card needs tens of KB, which does not fit the 64 KB LVGL heap — and when `lv_draw_layer_alloc_buf` fails the software dispatcher retries in a tight loop until the **task watchdog resets the board**. This has already bitten this project once via a press-scale effect. Use colour changes for press/hover feedback.

**No shadows.** `CONFIG_LV_DRAW_SW_SHADOW_CACHE_SIZE=0`, so shadows are recomputed every frame and will cost visible frames on the RGB panel. Express depth with borders and background tints instead.

**RGB565.** `CONFIG_LV_COLOR_DEPTH_16` — subtle low-contrast colour steps band badly. Pick colours that survive 16-bit quantisation, and keep gradients to 2 stops (`CONFIG_LV_GRADIENT_MAX_STOPS=2`).

**128 KB LVGL heap** (`CONFIG_LV_MEM_SIZE_KILOBYTES`, a static array in internal SRAM). It was 64 KB and that was not enough: with the weather page, the crypto page and six tiles all live, `lv_malloc` started failing *inside draw task creation*, and the software dispatcher then spins retrying until the **task watchdog resets the board**. The giveaway is a watchdog backtrace parked in `lv_draw_dispatch` / `execute_drawing` with no progress, and it appears when the text gets longer (more letters, more draw tasks), so it can look like a data bug. Keep the live object count bounded anyway — every `lv_obj` is ~150-200 bytes and the pages are never destroyed, only hidden.

**Touch only, no keyboard or mouse.** Hit targets should be ≥ 44 px; tile cards are 245x178, the top bar's temperature button and the hourly arrows are 44 px.

**~30 FPS** (`CONFIG_LV_DEF_REFR_PERIOD=33`) with a single PSRAM framebuffer and a 10-line bounce buffer. Avoid full-screen animation.

**The hourly forecast is deliberately not in `weather_t`.** `weather_get()` copies the snapshot by value onto the caller's stack, and the LVGL task only has 7168 bytes (`ESP_LVGL_PORT_INIT_CONFIG`). The 5x24 hourly series is ~2 KB, so it lives in module state behind `weather_get_hours(day, out, cap)`, which copies at most one day. Do not move it back into the struct.

**One HTTPS request at a time.** `weather.c` and `markets.c` each own a task and would otherwise handshake concurrently at boot and again whenever their periods collide. Two mbedTLS sessions plus two cert bundle verifications do not fit, and the failure is *not* a clean allocation error — it surfaces as `esp-x509-crt-bundle: PSA signature verification failed with error 0xffffff73` (that is -141, `PSA_ERROR_INSUFFICIENT_MEMORY`), which reads like a certificate problem. Every `fetch_once()` must sit inside `net_lock_take()` / `net_lock_give()`, and `net_lock_init()` must run in `app_main` before any poller starts. mbedTLS also allocates from PSRAM (`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`) so its 16 KB input buffer is not competing with the LVGL pool for internal SRAM.

**The FreeRTOS idle counter is 32-bit microseconds.** `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` is what makes `ulTaskGetIdleRunTimeCounterForCore()` exist, and its `configRUN_TIME_COUNTER_TYPE` wraps every ~71 minutes. `sysinfo.c` computes CPU load from an *unsigned* subtraction of consecutive reads, which is exactly why the wrap is harmless — do not widen those to 64-bit and do not compare the absolute values. For the same reason `sysinfo_sample()` may only ever have one caller: it consumes the interval since the last call, so a second caller would leave both reading near zero. That option also cannot be hand-mirrored into `sdkconfig` the way the rest can; it unlocks sub-options that are absent from a config generated without it, so it needs a real `idf.py reconfigure`.

**Binance quotes its numbers as strings.** `"lastPrice":"118420.01000000"` — `weather.c`'s `find_number()` walks to the `:` and calls `strtof`, which stops dead on the `"`. `markets.c` has its own `find_quoted_number()` that steps over it. Both parsers depend on the same trap: every key string carries its own closing quote, or `"priceChange"` would match `"priceChangePercent"` (and `"weather_code"` would match `"weather_code_units"`).

## UI conventions

Everything visual goes through `main/ui/theme.h`. Do not hard-code colours, spacing, radii, or fonts in feature code — use the `UI_COLOR_*`, `UI_SPACE_*`, `UI_RADIUS_*`, `UI_FONT_*` tokens and the shared styles (`ui_style_screen()`, `ui_style_surface()`, `ui_style_card()`, …). `ui_theme_init()` must run before any screen is built.

Icons are FontAwesome glyphs baked into the Montserrat fonts — `LV_SYMBOL_*` string macros from `lv_symbol_def.h`, concatenated into label text. There are no image assets and no image decoders enabled. Anything the glyph set does not cover gets assembled from nested `lv_obj` rectangles and circles instead: see `weather_icon.c` and the coin in `markets_page.c`. A tile card built with a NULL icon leaves its icon tile empty for exactly this.

Dashboard sections are declared in one table (`s_sections[]` in `main/ui/dashboard.c`); adding a section is one line plus the pages/cards loop picking it up automatically. A section with a non-NULL `on_click` brings its own page and gets no stub — that is how Climate, Crypto and System escape the table, and it is why `show_grid()` has to skip the `NULL` entries in `s_pages[]`. All three also bind their tile's subtitle to live data via `ui_weather_page_bind_tile()` / `ui_markets_bind_tile()` / `ui_sysinfo_bind_tile()`; those run inside the card loop, *before* the pages exist, so none of them may render from the bind call. Section pages are all created up front and swapped with `LV_OBJ_FLAG_HIDDEN` rather than destroyed and rebuilt. The weather, markets and system pages are siblings of the top bar so they can cover it; all three need the full 440 px for their type sizes, and every one of them must be hidden in `show_grid()` or backing out of a stub page leaves it stacked behind the grid.

Widgets that show live data own their own `lv_timer` and poll (`weather_get()`, `time_sync_is_valid()`, `wifi_sta_is_connected()`) rather than being pushed to. Every label they write goes through `ui_label_set()` (`ui/label.h`) against a `static char` cache — on a 30 FPS RGB panel an unnecessary `lv_label_set_text` costs a real invalidation, and these widgets tick every 1-5 s against data that changes every 10-15 min. Build labels with `ui_label()` rather than a local `make_label` copy.

`UI_FONT_CLOCK` is the *built-in* `lv_font_montserrat_48` and has no degree sign or Polish glyphs; only the custom `ui_font_14/18/22/28` carry them. Those are built over a narrow range too — ASCII, `°` (U+00B0), `•` (U+2022), the Polish letters and a hand-picked FontAwesome set (see the `Opts:` line at the top of `ui/fonts/ui_font_22.c`). Any other codepoint renders as a tofu box, so `·`, `—` and `→` are out; use `•` as the separator.

## Code style

Follows `components/bsp_waveshare_lcd7/bsp.c`:

- 4-space indent, K&R braces, ~110-column lines
- `static` for anything file-local; `s_` prefix for module state (`s_i2c_bus`, `s_active`)
- `static const char *TAG = "...";` for logging
- `ESP_RETURN_ON_ERROR(expr, TAG, "short msg")` / `ESP_ERROR_CHECK` for error propagation
- Headers use `#pragma once` and an `extern "C"` guard

**Comments: only write one if it is genuinely important.** Default to none. The code says what it does; a comment restating it is noise and gets deleted. Never add:

- section banners or dividers (`/* --- Colours --- */`)
- restatements of the next line (`/* Icon tile */` above `lv_obj_t *tile = ...`)
- labels on self-naming things (`/* screen background */` after `UI_COLOR_BG`)
- doc blocks on functions whose name and signature already explain them

Do write a comment when the reader would otherwise get it wrong or undo it: hardware quirks and register meanings, timing and ordering constraints, non-obvious magic numbers, and traps where the obvious change breaks something (e.g. the transform/layer-allocation note in `theme.c`). Explain *why*, never *what*.
