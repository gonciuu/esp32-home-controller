#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define WEATHER_HOURLY_COUNT 25

typedef struct {
    int   hour;      
    float temp_c;
    int   code;
} weather_hour_t;

typedef struct {
    float temp_c;
    float wind_kmh;
    int   humidity;
    int   code;      
    bool  valid;

    float tomorrow_max;
    float tomorrow_min;
    int   tomorrow_code;
    bool  tomorrow_valid;

    weather_hour_t hourly[WEATHER_HOURLY_COUNT];
    int            hourly_count;   
    bool           hourly_valid;
} weather_t;

void weather_start(void);

bool weather_get(weather_t *out);

const char *weather_code_text(int code);

#ifdef __cplusplus
}
#endif
