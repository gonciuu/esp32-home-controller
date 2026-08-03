#include "ha_lights.h"
#include "net_lock.h"
#include "wifi_sta.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "ha_lights";

#define HA_BASE         "http://" CONFIG_HOME_HA_HOST
#define HA_TEMPLATE_URL HA_BASE "/api/template"

#define TEMPLATE_BODY                                                                      \
    "{\"template\":\"{%- for s in (states.light|list + states.switch|list) -%}"            \
    "{{s.entity_id}}|{{s.name}}|{{s.state}}\\n{% endfor -%}\"}"

#define RESPONSE_CAP     2048
#define LINE_CAP         128
#define REFRESH_PERIOD_S 15
#define RETRY_PERIOD_S   30
#define POLL_PERIOD_S    5
#define CMD_QUEUE_LEN    4

#define SETTLE_MS 400

typedef struct {
    char entity[48];   /* empty means "just refetch" */
    bool on;
} ha_cmd_t;

static char              s_body[RESPONSE_CAP];
static size_t            s_body_len;
static ha_lights_t       s_snapshot;
static SemaphoreHandle_t s_lock;
static QueueHandle_t     s_cmds;

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

static int perform_post(const char *url, const char *body)
{
    s_body_len = 0;
    s_body[0] = '\0';

    const esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_cb,
        .timeout_ms = 5000,
    };

    net_lock_take();

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        net_lock_give();
        return -1;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", "Bearer " CONFIG_HOME_HA_TOKEN);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    int status = -1;

    const esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "request failed: %s", esp_err_to_name(err));
    }
    else {
        status = esp_http_client_get_status_code(client);
    }

    esp_http_client_cleanup(client);
    net_lock_give();
    return status;
}

static void parse_body(ha_lights_t *out)
{
    const char *line = s_body;

    while (*line && out->count < HA_LIGHTS_MAX) {
        const size_t len = strcspn(line, "\n");

        if (len > 0) {
            char         row[LINE_CAP];
            const size_t take = (len < sizeof(row) - 1) ? len : sizeof(row) - 1;

            memcpy(row, line, take);
            row[take] = '\0';

            char *bar1 = strchr(row, '|');
            char *bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;

            if (bar2) {
                *bar1 = '\0';
                *bar2 = '\0';

                ha_light_t *light = &out->light[out->count++];
                const char *state = bar2 + 1;

                strlcpy(light->entity, row, sizeof(light->entity));
                strlcpy(light->name, bar1 + 1, sizeof(light->name));
                light->on = (strcmp(state, "on") == 0);
                light->valid = (strcmp(state, "on") == 0 || strcmp(state, "off") == 0);

                if (light->on) {
                    out->on_count++;
                }
            }
        }

        line += len;
        if (*line == '\n') {
            line++;
        }
    }
    out->valid = true;
}

static bool fetch_once(void)
{
    const int status = perform_post(HA_TEMPLATE_URL, TEMPLATE_BODY);

    if (status != 200) {
        if (status > 0) {
            ESP_LOGW(TAG, "template http %d%s", status,
                     (status == 401) ? " - check HOME_HA_TOKEN" : "");
        }
        return false;
    }

    ha_lights_t fresh = { 0 };
    parse_body(&fresh);
    fresh.updated = time(NULL);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot = fresh;
    xSemaphoreGive(s_lock);

    if (fresh.count == 0) {
        ESP_LOGW(TAG, "no lights parsed from %d bytes: \"%s\"", (int)s_body_len, s_body);
        return true;
    }

    ESP_LOGI(TAG, "%d light(s), %d on", fresh.count, fresh.on_count);
    for (int i = 0; i < fresh.count; i++) {
        ESP_LOGI(TAG, "  %s (%s) %s", fresh.light[i].entity, fresh.light[i].name,
                 fresh.light[i].valid ? (fresh.light[i].on ? "on" : "off") : "unavailable");
    }
    return true;
}

static void send_service(const ha_cmd_t *cmd)
{
    char url[192];
    char body[80];

    const char *domain = (strncmp(cmd->entity, "switch.", 7) == 0) ? "switch" : "light";

    snprintf(url, sizeof(url), HA_BASE "/api/services/%s/turn_%s", domain, cmd->on ? "on" : "off");
    snprintf(body, sizeof(body), "{\"entity_id\":\"%s\"}", cmd->entity);

    const int status = perform_post(url, body);
    if (status != 200) {
        ESP_LOGW(TAG, "turn_%s %s: http %d", cmd->on ? "on" : "off", cmd->entity, status);
        return;
    }
    ESP_LOGI(TAG, "turn_%s %s", cmd->on ? "on" : "off", cmd->entity);
}

static void ha_lights_task(void *arg)
{
    time_t   next_fetch = 0;
    ha_cmd_t cmd;

    while (true) {
        if (xQueueReceive(s_cmds, &cmd, pdMS_TO_TICKS(POLL_PERIOD_S * 1000)) == pdTRUE) {
            if (cmd.entity[0] && wifi_sta_is_connected()) {
                send_service(&cmd);
                vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));
            }
            next_fetch = 0;
        }

        if (wifi_sta_is_connected()) {
            const time_t now = time(NULL);

            if (now >= next_fetch) {
                const bool ok = fetch_once();
                next_fetch = now + (ok ? REFRESH_PERIOD_S : RETRY_PERIOD_S);
            }
        }
    }
}

void ha_lights_start(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "no mutex");
        return;
    }
    s_cmds = xQueueCreate(CMD_QUEUE_LEN, sizeof(ha_cmd_t));
    if (!s_cmds) {
        ESP_LOGE(TAG, "no queue");
        return;
    }
    xTaskCreate(ha_lights_task, "ha_lights", 8192, NULL, 4, NULL);
}

bool ha_lights_get(ha_lights_t *out)
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

void ha_lights_toggle(int index)
{
    if (!s_lock || !s_cmds || index < 0 || index >= HA_LIGHTS_MAX) {
        return;
    }

    ha_cmd_t cmd = { 0 };

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (index < s_snapshot.count && s_snapshot.light[index].valid) {
        ha_light_t *light = &s_snapshot.light[index];

        light->on = !light->on;
        s_snapshot.on_count += light->on ? 1 : -1;

        strlcpy(cmd.entity, light->entity, sizeof(cmd.entity));
        cmd.on = light->on;
    }
    xSemaphoreGive(s_lock);

    if (cmd.entity[0]) {
        xQueueSend(s_cmds, &cmd, 0);
    }
}

void ha_lights_refresh(void)
{
    if (!s_cmds) {
        return;
    }
    const ha_cmd_t nudge = { 0 };
    xQueueSend(s_cmds, &nudge, 0);
}
