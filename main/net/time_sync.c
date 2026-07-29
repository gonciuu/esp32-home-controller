#include "time_sync.h"

#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "time_sync";

#define TZ_WARSAW "CET-1CEST,M3.5.0,M10.5.0/3"

#define TIME_VALID_EPOCH 1704067200

void time_sync_start(void)
{
    setenv("TZ", TZ_WARSAW, 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = true;
    config.server_from_dhcp = false;

    const esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sntp init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "sntp started, tz=%s", TZ_WARSAW);
}

bool time_sync_is_valid(void)
{
    return time(NULL) > TIME_VALID_EPOCH;
}
