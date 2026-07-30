idf_component_register(SRCS "app_main.c"
                            "net/wifi_sta.c"
                            "net/net_lock.c"
                            "net/time_sync.c"
                            "net/weather.c"
                            "net/markets.c"
                            "ui/theme.c"
                            "ui/label.c"
                            "ui/fonts/ui_font_14.c"
                            "ui/fonts/ui_font_18.c"
                            "ui/fonts/ui_font_22.c"
                            "ui/fonts/ui_font_28.c"
                            "ui/tile_card.c"
                            "ui/weather_icon.c"
                            "ui/hourly_strip.c"
                            "ui/top_bar.c"
                            "ui/weather_page.c"
                            "ui/markets_page.c"
                            "ui/page.c"
                            "ui/dashboard.c"
                       PRIV_REQUIRES bsp_waveshare_lcd7 esp_wifi esp_netif esp_timer
                                     nvs_flash esp_http_client esp-tls mbedtls
                       INCLUDE_DIRS "." "ui" "net")

# lv_font_conv output includes "lvgl/lvgl.h" unless this is defined.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
