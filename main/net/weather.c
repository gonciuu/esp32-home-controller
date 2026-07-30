#include "weather.h"
#include "wifi_sta.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "weather";

#define WEATHER_URL                                                                    \
    "https://api.open-meteo.com/v1/forecast"                                           \
    "?latitude=" CONFIG_HOME_WEATHER_LAT                                               \
    "&longitude=" CONFIG_HOME_WEATHER_LON                                              \
    "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"         \
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max,"    \
    "sunrise,sunset"                                                                   \
    "&forecast_days=5"                                                                 \
    "&hourly=temperature_2m,weather_code,wind_speed_10m"                               \
    "&timezone=Europe%2FWarsaw"

#define RESPONSE_CAP     10240
#define REFRESH_PERIOD_S (10 * 60)
#define RETRY_PERIOD_S   60
#define POLL_PERIOD_S    10

static char              s_body[RESPONSE_CAP];
static size_t            s_body_len;
static weather_t         s_snapshot;
static weather_hour_t    s_hours[WEATHER_HOUR_COUNT];
static int               s_hour_count;
static int               s_day_start[WEATHER_DAY_COUNT];
static int               s_day_start_count;
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

static bool find_number(const char *json, const char *key, float *out)
{
    const char *at = strstr(json, key);
    if (!at) {
        return false;
    }
    at = strchr(at + strlen(key), ':');
    if (!at) {
        return false;
    }

    char *end = NULL;
    const float value = strtof(at + 1, &end);
    if (end == at + 1) {
        return false;
    }
    *out = value;
    return true;
}

static const char *array_start(const char *json, const char *key)
{
    const char *at = strstr(json, key);
    if (!at) {
        return NULL;
    }
    at = strchr(at + strlen(key), '[');
    return at ? at + 1 : NULL;
}


static const char *next_element(const char *at)
{
    while (*at && *at != ',' && *at != ']') {
        at++;
    }
    return (*at == ',') ? at + 1 : NULL;
}

static const char *find_array_element(const char *json, const char *key, int index)
{
    const char *at = array_start(json, key);

    for (int i = 0; at && i < index; i++) {
        at = next_element(at);
    }
    return at;
}

static bool find_array_number(const char *json, const char *key, int index, float *out)
{
    const char *at = find_array_element(json, key, index);
    if (!at) {
        return false;
    }

    char *end = NULL;
    const float value = strtof(at, &end);
    if (end == at) {
        return false;
    }
    *out = value;
    return true;
}

static bool find_iso_hour(const char *at, int *out)
{
    at = at ? strchr(at, 'T') : NULL;
    if (!at || at[1] < '0' || at[1] > '9' || at[2] < '0' || at[2] > '9') {
        return false;
    }
    *out = (at[1] - '0') * 10 + (at[2] - '0');
    return true;
}


static bool find_array_time(const char *json, const char *key, int index, char *out, size_t cap)
{
    const char *at = find_array_element(json, key, index);
    at = at ? strchr(at, 'T') : NULL;
    if (!at || cap < 6) {
        return false;
    }
    for (int i = 1; i <= 5; i++) {
        if (at[i] == '\0' || at[i] == '"') {
            return false;
        }
    }
    memcpy(out, at + 1, 5);
    out[5] = '\0';
    return true;
}

static void parse_daily(weather_t *out)
{
    const char *daily = strstr(s_body, "\"daily\":");
    if (!daily) {
        return;
    }

    for (int i = 0; i < WEATHER_DAY_COUNT; i++) {
        weather_day_t *day = &out->daily[i];
        float max, min, value;

        if (!find_array_number(daily, "\"temperature_2m_max\"", i, &max) ||
            !find_array_number(daily, "\"temperature_2m_min\"", i, &min)) {
            break;
        }
        day->max_c = max;
        day->min_c = min;
        day->wind_kmh =
            find_array_number(daily, "\"wind_speed_10m_max\"", i, &value) ? value : 0.0f;
        day->code = find_array_number(daily, "\"weather_code\"", i, &value) ? (int)value : -1;

        find_array_time(daily, "\"sunrise\"", i, day->sunrise, sizeof(day->sunrise));
        find_array_time(daily, "\"sunset\"", i, day->sunset, sizeof(day->sunset));

        out->daily_count = i + 1;
    }
    out->daily_valid = out->daily_count > 0;
}

static void parse_hourly(void)
{
    s_hour_count = 0;
    s_day_start_count = 0;

    const char *hourly = strstr(s_body, "\"hourly\":");
    if (!hourly) {
        return;
    }

    const char *iso  = array_start(hourly, "\"time\"");
    const char *temp = array_start(hourly, "\"temperature_2m\"");
    const char *code = array_start(hourly, "\"weather_code\"");
    const char *wind = array_start(hourly, "\"wind_speed_10m\"");

    for (int i = 0; i < WEATHER_HOUR_COUNT && iso && temp; i++) {
        weather_hour_t *slot = &s_hours[i];
        char *end = NULL;

        if (!find_iso_hour(iso, &slot->hour)) {
            break;
        }
        slot->temp_c = strtof(temp, &end);
        if (end == temp) {
            break;
        }
        slot->wind_kmh = wind ? strtof(wind, NULL) : 0.0f;
        slot->code = code ? (int)strtof(code, NULL) : -1;

        if ((i == 0 || slot->hour == 0) && s_day_start_count < WEATHER_DAY_COUNT) {
            s_day_start[s_day_start_count++] = i;
        }
        s_hour_count = i + 1;

        iso  = next_element(iso);
        temp = next_element(temp);
        code = code ? next_element(code) : NULL;
        wind = wind ? next_element(wind) : NULL;
    }
}

static bool parse_body(weather_t *out)
{
    /* Matches "current": and not "current_units": thanks to the closing quote. */
    const char *current = strstr(s_body, "\"current\":");
    if (!current) {
        ESP_LOGE(TAG, "no current object in payload");
        return false;
    }

    float temp;
    if (!find_number(current, "\"temperature_2m\"", &temp)) {
        ESP_LOGE(TAG, "no temperature in payload");
        return false;
    }
    out->temp_c = temp;

    float value;
    out->humidity = find_number(current, "\"relative_humidity_2m\"", &value) ? (int)value : 0;
    out->wind_kmh = find_number(current, "\"wind_speed_10m\"", &value) ? value : 0.0f;
    out->code = find_number(current, "\"weather_code\"", &value) ? (int)value : -1;
    out->valid = true;

    parse_daily(out);
    return true;
}

static bool fetch_once(void)
{
    s_body_len = 0;
    s_body[0] = '\0';

    const esp_http_client_config_t config = {
        .url = WEATHER_URL,
        .event_handler = http_event_cb,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
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
                ESP_LOGW(TAG, "response filled the %d byte buffer; fields may be missing",
                         RESPONSE_CAP);
            }
            weather_t fresh = { 0 };
            if (parse_body(&fresh)) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_snapshot = fresh;
                parse_hourly();
                const int hours = s_hour_count;
                const int days = s_day_start_count;
                xSemaphoreGive(s_lock);

                ESP_LOGI(TAG, "now %.1f C, %d%%, %.0f km/h, code %d", fresh.temp_c,
                         fresh.humidity, fresh.wind_kmh, fresh.code);
                if (fresh.daily_valid) {
                    ESP_LOGI(TAG, "%d days, today %.0f/%.0f C code %d, sun %s-%s",
                             fresh.daily_count, fresh.daily[0].max_c, fresh.daily[0].min_c,
                             fresh.daily[0].code, fresh.daily[0].sunrise, fresh.daily[0].sunset);
                }
                ESP_LOGI(TAG, "%d hours over %d days, %d bytes", hours, days, (int)s_body_len);
                ok = true;
            }
        }
    }

    esp_http_client_cleanup(client);
    return ok;
}


static void weather_task(void *arg)
{
    time_t next_fetch = 0;
    int fetched_hour = -1;

    while (true) {
        if (wifi_sta_is_connected()) {
            const time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);

            if (now >= next_fetch || tm_now.tm_hour != fetched_hour) {
                const bool ok = fetch_once();
                next_fetch = now + (ok ? REFRESH_PERIOD_S : RETRY_PERIOD_S);
                if (ok) {
                    fetched_hour = tm_now.tm_hour;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_S * 1000));
    }
}

void weather_start(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "no mutex");
        return;
    }
    xTaskCreate(weather_task, "weather", 8192, NULL, 4, NULL);
}

bool weather_get(weather_t *out)
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

int weather_get_hours(int day, weather_hour_t *out, int cap)
{
    if (!s_lock || cap <= 0) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    int start;
    if (day <= 0) {
        const time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        start = 0;
        for (int i = 0; i < s_hour_count; i++) {
            if (s_hours[i].hour == tm_now.tm_hour) {
                start = i + 1;
                break;
            }
        }
    }
    else if (day < s_day_start_count) {
        start = s_day_start[day];
    }
    else {
        xSemaphoreGive(s_lock);
        return 0;
    }

    int count = 0;
    while (count < cap && count < WEATHER_DAY_HOURS && start + count < s_hour_count) {
        out[count] = s_hours[start + count];
        count++;
    }

    xSemaphoreGive(s_lock);
    return count;
}

const char *weather_code_text(int code)
{
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Icy drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66:
    case 67: return "Icy rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80:
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Hail storm";
    default: return "Unknown";
    }
}
