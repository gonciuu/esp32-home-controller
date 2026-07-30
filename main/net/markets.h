#pragma once

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MARKETS_ASSET_COUNT 3

typedef struct {
    float price;
    float change_pct;
    float high;
    float low;
    bool  valid;
} market_asset_t;

typedef struct {
    market_asset_t asset[MARKETS_ASSET_COUNT];
    time_t         updated;
    bool           valid;
} markets_t;

void markets_start(void);
bool markets_get(markets_t *out);

const char *markets_symbol(int index);
const char *markets_name(int index);

#ifdef __cplusplus
}
#endif
