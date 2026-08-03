#pragma once

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HA_LIGHTS_MAX 8

typedef struct {
    char entity[48];
    char name[24];
    bool on;
    bool valid;
} ha_light_t;

typedef struct {
    ha_light_t light[HA_LIGHTS_MAX];
    int        count;
    int        on_count;
    time_t     updated;
    bool       valid;
} ha_lights_t;

void ha_lights_start(void);
bool ha_lights_get(ha_lights_t *out);

void ha_lights_toggle(int index);
void ha_lights_refresh(void);

#ifdef __cplusplus
}
#endif
