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

`sdkconfig` is generated and gitignored. **Board and project config belongs in `sdkconfig.defaults`.** Because `sdkconfig` already contains an explicit value for most options, adding a line to `sdkconfig.defaults` does *not* retroactively change an existing `sdkconfig` — either edit both, or delete `sdkconfig` and run `idf.py reconfigure`.

## Layout

```
main/
  app_main.c        entry point: bring up display, build the UI
  ui/
    theme.h/.c      design system — tokens + shared lv_style_t objects
    menu_card.h/.c  reusable touch row (icon tile, label, chevron)
    page.h/.c       section page stub
    dashboard.h/.c  the screen shell: menu left, content right
components/
  bsp_waveshare_lcd7/   board support: I2C, CH422G expander, RGB panel,
                        GT911 touch, LVGL port bring-up
```

New UI `.c` files must be added to `SRCS` in `main/CMakeLists.txt`.

## Rules that will bite you

**Hold the LVGL lock.** Every `lv_*` call made from outside the LVGL task must be wrapped in `bsp_display_lock(0)` / `bsp_display_unlock()` (see `components/bsp_waveshare_lcd7/include/bsp.h`). Forgetting this produces intermittent corruption, not a clean crash.

**Fonts must be enabled in sdkconfig before use.** Only the sizes with `CONFIG_LV_FONT_MONTSERRAT_<n>=y` are compiled in — currently 14, 18, 22, 28. Referencing any other `lv_font_montserrat_*` fails at link time with an undefined reference.

**No transforms on large objects.** Any `transform_scale != 256`, `transform_rotation`, or skew makes LVGL render that object into an intermediate layer buffer (`calculate_layer_type()` in `lv_obj_style.c`). At RGB565 a full-width card needs tens of KB, which does not fit the 64 KB LVGL heap — and when `lv_draw_layer_alloc_buf` fails the software dispatcher retries in a tight loop until the **task watchdog resets the board**. This has already bitten this project once via a press-scale effect. Use colour changes for press/hover feedback.

**No shadows.** `CONFIG_LV_DRAW_SW_SHADOW_CACHE_SIZE=0`, so shadows are recomputed every frame and will cost visible frames on the RGB panel. Express depth with borders and background tints instead.

**RGB565.** `CONFIG_LV_COLOR_DEPTH_16` — subtle low-contrast colour steps band badly. Pick colours that survive 16-bit quantisation, and keep gradients to 2 stops (`CONFIG_LV_GRADIENT_MAX_STOPS=2`).

**64 KB LVGL heap** (`CONFIG_LV_MEM_SIZE_KILOBYTES=64`). Keep the live object count bounded; watch the serial log for LVGL heap warnings.

**Touch only, no keyboard or mouse.** Hit targets should be ≥ 44 px; menu cards are 68 px.

**~30 FPS** (`CONFIG_LV_DEF_REFR_PERIOD=33`) with a single PSRAM framebuffer and a 10-line bounce buffer. Avoid full-screen animation.

## UI conventions

Everything visual goes through `main/ui/theme.h`. Do not hard-code colours, spacing, radii, or fonts in feature code — use the `UI_COLOR_*`, `UI_SPACE_*`, `UI_RADIUS_*`, `UI_FONT_*` tokens and the shared styles (`ui_style_screen()`, `ui_style_surface()`, `ui_style_card()`, …). `ui_theme_init()` must run before any screen is built.

Icons are FontAwesome glyphs baked into the Montserrat fonts — `LV_SYMBOL_*` string macros from `lv_symbol_def.h`, concatenated into label text. There are no image assets and no image decoders enabled.

Dashboard sections are declared in one table (`s_sections[]` in `main/ui/dashboard.c`); adding a section is one line plus the pages/cards loop picking it up automatically. Section pages are all created up front and swapped with `LV_OBJ_FLAG_HIDDEN` rather than destroyed and rebuilt.

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

## Stale files

`README.md` and `pytest_hello_world.py` are leftover ESP-IDF "Hello World" example boilerplate. They do not describe this project and the pytest no longer matches the firmware. Ignore them; don't treat them as documentation.
