#include "esp_err.h"
#include "bsp.h"
#include "dashboard.h"
#include "theme.h"

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start());

    if (bsp_display_lock(0)) {
        ui_theme_init();
        ui_dashboard_create();
        bsp_display_unlock();
    }
}
