idf_component_register(SRCS "app_main.c"
                            "ui/theme.c"
                            "ui/menu_card.c"
                            "ui/page.c"
                            "ui/dashboard.c"
                       PRIV_REQUIRES bsp_waveshare_lcd7
                       INCLUDE_DIRS "." "ui")
