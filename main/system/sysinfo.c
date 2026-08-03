#include "sysinfo.h"

#include <stdio.h>
#include <string.h>

#include "driver/temperature_sensor.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "wifi_sta.h"

static const char *TAG = "sysinfo";

#define CORE_COUNT 2

static temperature_sensor_handle_t s_tsens;

static uint32_t s_idle_prev[CORE_COUNT];
static int64_t  s_wall_prev;

static char s_chip[96];

static void read_idle(uint32_t *out)
{
    for (int c = 0; c < CORE_COUNT; c++) {
        out[c] = (uint32_t)ulTaskGetIdleRunTimeCounterForCore(c);
    }
}

static void start_tsens(void)
{
    const temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    if (temperature_sensor_install(&cfg, &s_tsens) != ESP_OK) {
        ESP_LOGW(TAG, "temperature sensor unavailable");
        s_tsens = NULL;
        return;
    }
    if (temperature_sensor_enable(s_tsens) != ESP_OK) {
        ESP_LOGW(TAG, "temperature sensor enable failed");
        temperature_sensor_uninstall(s_tsens);
        s_tsens = NULL;
    }
}

void sysinfo_init(void)
{
    start_tsens();

    read_idle(s_idle_prev);
    s_wall_prev = esp_timer_get_time();

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    /* Deliberately smaller than idf_ver's own char[32]: gcc bounds the %s below by the array
     * size, and 31 bytes does not provably fit alongside the rest. */
    char idf[16];
    strlcpy(idf, esp_app_get_description()->idf_ver, sizeof(idf));

    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);

    snprintf(s_chip, sizeof(s_chip),
             "ESP32-S3 rev%d.%d \xE2\x80\xA2 %d cores \xE2\x80\xA2 %s \xE2\x80\xA2 %u MB flash",
             chip.revision / 100, chip.revision % 100, chip.cores, idf,
             (unsigned)(flash / (1024 * 1024)));
}

static void sample_cpu(sysinfo_t *out)
{
    uint32_t idle[CORE_COUNT];
    const int64_t now = esp_timer_get_time();
    const int64_t wall = now - s_wall_prev;

    read_idle(idle);

    /* Too short an interval and the quantisation swamps the answer; keep the previous
     * reading rather than printing noise. */
    if (wall < 100000) {
        return;
    }

    int total = 0;

    for (int c = 0; c < CORE_COUNT; c++) {
        /* The counter is configRUN_TIME_COUNTER_TYPE: 32-bit microseconds, so it wraps every
         * ~71 minutes. The unsigned subtraction is what makes the wrap harmless - never
         * compare or widen the absolute values. */
        const uint32_t delta = idle[c] - s_idle_prev[c];
        int busy = 100 - (int)((int64_t)delta * 100 / wall);

        if (busy < 0) {
            busy = 0;   /* idle accounting can overshoot wall time by a hair */
        }
        else if (busy > 100) {
            busy = 100;
        }
        out->cpu_core_pct[c] = busy;
        total += busy;
    }

    out->cpu_pct = total / CORE_COUNT;
    out->cpu_valid = true;

    memcpy(s_idle_prev, idle, sizeof(idle));
    s_wall_prev = now;
}

static void sample_wifi(sysinfo_t *out)
{
    if (!wifi_sta_is_connected()) {
        return;
    }

    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        out->rssi = ap.rssi;
        out->wifi_valid = true;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;

    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip.ip));
    }
}

void sysinfo_sample(sysinfo_t *out)
{
    memset(out, 0, sizeof(*out));

    sample_cpu(out);

    if (s_tsens && temperature_sensor_get_celsius(s_tsens, &out->temp_c) == ESP_OK) {
        out->temp_valid = true;
    }

    /* heap_caps_get_info() would walk every block, which is far too heavy for a 2 s tick. */
    out->ram_free     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->ram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    out->ram_total    = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    out->psram_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->psram_total  = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    out->uptime_s = esp_timer_get_time() / 1000000;

    sample_wifi(out);
}

const char *sysinfo_chip_text(void)
{
    return s_chip;
}

const char *sysinfo_reset_text(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_EXT:      return "reset pin";
    case ESP_RST_SW:       return "software reset";
    case ESP_RST_PANIC:    return "panic";
    case ESP_RST_INT_WDT:  return "interrupt wdt";
    case ESP_RST_TASK_WDT: return "task wdt";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP:return "deep sleep";
    case ESP_RST_USB:      return "usb";
    case ESP_RST_JTAG:     return "jtag";
    default:               return "unknown";
    }
}
