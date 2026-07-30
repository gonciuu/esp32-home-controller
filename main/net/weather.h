#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define WEATHER_DAY_COUNT  5
#define WEATHER_DAY_HOURS  24
#define WEATHER_HOUR_COUNT (WEATHER_DAY_COUNT * WEATHER_DAY_HOURS)

typedef struct {
    int   hour;
    float temp_c;
    float wind_kmh;
    int   code;
} weather_hour_t;

typedef struct {
    float max_c;
    float min_c;
    float wind_kmh;
    int   code;
    char  sunrise[6];
    char  sunset[6];
} weather_day_t;

typedef struct {
    float temp_c;
    float wind_kmh;
    int   humidity;
    int   code;
    bool  valid;

    /* daily[0] is today, daily[1] tomorrow. */
    weather_day_t daily[WEATHER_DAY_COUNT];
    int           daily_count;
    bool          daily_valid;
} weather_t;

void weather_start(void);

bool weather_get(weather_t *out);

int weather_get_hours(int day, weather_hour_t *out, int cap);

const char *weather_code_text(int code);

#ifdef __cplusplus
}
#endif
