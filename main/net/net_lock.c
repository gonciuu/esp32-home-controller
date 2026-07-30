#include "net_lock.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "net_lock";

static SemaphoreHandle_t s_lock;

void net_lock_init(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "no mutex");
    }
}

void net_lock_take(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

void net_lock_give(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}
