#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int     cpu_pct;
    int     cpu_core_pct[2];
    bool    cpu_valid;

    float   temp_c;
    bool    temp_valid;

    size_t  ram_free;
    size_t  ram_min_free;
    size_t  ram_total;
    size_t  psram_free;
    size_t  psram_total;

    int64_t uptime_s;

    bool    wifi_valid;
    int8_t  rssi;
    char    ip[16];
} sysinfo_t;

void sysinfo_init(void);

/* Advances the CPU idle-time delta, so it must have exactly one caller - a second one would
 * consume the first one's interval and both would read near zero. */
void sysinfo_sample(sysinfo_t *out);

const char *sysinfo_chip_text(void);

const char *sysinfo_reset_text(void);

#ifdef __cplusplus
}
#endif
