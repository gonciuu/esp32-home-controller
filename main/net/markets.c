#include "markets.h"
#include "net_lock.h"
#include "wifi_sta.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "markets";

#define MARKETS_URL                                                                    \
    "https://api.binance.com/api/v3/ticker/24hr"                                       \
    "?symbols=%5B%22BTCUSDT%22%2C%22ETHUSDT%22%2C%22SOLUSDT%22%5D"

#define RESPONSE_CAP     4096
#define REFRESH_PERIOD_S (15 * 60)
#define RETRY_PERIOD_S   60
#define POLL_PERIOD_S    10

static const struct {
    const char *anchor;
    const char *symbol;
    const char *name;
} s_assets[MARKETS_ASSET_COUNT] = {
    { "\"symbol\":\"BTCUSDT\"", "BTC", "Bitcoin" },
    { "\"symbol\":\"ETHUSDT\"", "ETH", "Ethereum" },
    { "\"symbol\":\"SOLUSDT\"", "SOL", "Solana" },
};

static char              s_body[RESPONSE_CAP];
static size_t            s_body_len;
static markets_t         s_snapshot;
static SemaphoreHandle_t s_lock;

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_CONNECTED) {
        s_body_len = 0;
        s_body[0] = '\0';
        return ESP_OK;
    }
    if (evt->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
    const size_t room = RESPONSE_CAP - 1 - s_body_len;
    const size_t take = (evt->data_len < room) ? (size_t)evt->data_len : room;
    memcpy(s_body + s_body_len, evt->data, take);
    s_body_len += take;
    s_body[s_body_len] = '\0';
    return ESP_OK;
}


static bool find_quoted_number(const char *json, const char *key, float *out)
{
    const char *at = strstr(json, key);
    if (!at) {
        return false;
    }
    at = strchr(at + strlen(key), ':');
    if (!at) {
        return false;
    }
    at++;
    if (*at == '"') {
        at++;
    }

    char *end = NULL;
    const float value = strtof(at, &end);
    if (end == at) {
        return false;
    }
    *out = value;
    return true;
}


static bool parse_body(markets_t *out)
{
    for (int i = 0; i < MARKETS_ASSET_COUNT; i++) {
        const char *at = strstr(s_body, s_assets[i].anchor);
        if (!at) {
            continue;
        }

        market_asset_t *asset = &out->asset[i];
        float value;

        if (!find_quoted_number(at, "\"lastPrice\"", &asset->price)) {
            continue;
        }
        asset->change_pct = find_quoted_number(at, "\"priceChangePercent\"", &value) ? value : 0.0f;
        asset->high = find_quoted_number(at, "\"highPrice\"", &value) ? value : 0.0f;
        asset->low = find_quoted_number(at, "\"lowPrice\"", &value) ? value : 0.0f;
        asset->valid = true;
        out->valid = true;
    }
    return out->valid;
}

static bool fetch_once(void)
{
    s_body_len = 0;
    s_body[0] = '\0';

    const esp_http_client_config_t config = {
        .url = MARKETS_URL,
        .event_handler = http_event_cb,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    net_lock_take();

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        net_lock_give();
        return false;
    }

    bool ok = false;
    const esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "request failed: %s", esp_err_to_name(err));
    }
    else {
        const int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGW(TAG, "http %d", status);
        }
        else {
            if (s_body_len == RESPONSE_CAP - 1) {
                ESP_LOGW(TAG, "response filled the %d byte buffer; assets may be missing",
                         RESPONSE_CAP);
            }
            markets_t fresh = { 0 };
            if (parse_body(&fresh)) {
                fresh.updated = time(NULL);

                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_snapshot = fresh;
                xSemaphoreGive(s_lock);

                for (int i = 0; i < MARKETS_ASSET_COUNT; i++) {
                    ESP_LOGI(TAG, "%s %.2f %+.2f%%, 24h %.2f/%.2f", s_assets[i].symbol,
                             fresh.asset[i].price, fresh.asset[i].change_pct,
                             fresh.asset[i].high, fresh.asset[i].low);
                }
                ok = true;
            }
            else {
                ESP_LOGE(TAG, "no asset parsed from %d bytes", (int)s_body_len);
            }
        }
    }

    esp_http_client_cleanup(client);
    net_lock_give();
    return ok;
}

static void markets_task(void *arg)
{
    time_t next_fetch = 0;

    while (true) {
        if (wifi_sta_is_connected()) {
            const time_t now = time(NULL);

            if (now >= next_fetch) {
                const bool ok = fetch_once();
                next_fetch = now + (ok ? REFRESH_PERIOD_S : RETRY_PERIOD_S);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_S * 1000));
    }
}

void markets_start(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "no mutex");
        return;
    }
    xTaskCreate(markets_task, "markets", 8192, NULL, 4, NULL);
}

bool markets_get(markets_t *out)
{
    if (!s_lock) {
        out->valid = false;
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_snapshot;
    xSemaphoreGive(s_lock);
    return out->valid;
}

const char *markets_symbol(int index)
{
    return (index >= 0 && index < MARKETS_ASSET_COUNT) ? s_assets[index].symbol : "";
}

const char *markets_name(int index)
{
    return (index >= 0 && index < MARKETS_ASSET_COUNT) ? s_assets[index].name : "";
}
