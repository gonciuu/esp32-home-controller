#include "esp_err.h"
#include "bsp.h"
#include "dashboard.h"
#include "theme.h"
#include "time_sync.h"
#include "weather.h"
#include "wifi_sta.h"

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start());

    ESP_ERROR_CHECK(wifi_sta_start());
    time_sync_start();
    weather_start();

    if (bsp_display_lock(0)) {
        ui_theme_init();
        ui_dashboard_create();
        bsp_display_unlock();
    }
}
