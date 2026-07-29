idf_component_register(SRCS "bsp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl esp_lvgl_port esp_lcd esp_lcd_touch_gt911 esp_driver_i2c)
