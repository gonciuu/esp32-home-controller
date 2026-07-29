#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void time_sync_start(void);

bool time_sync_is_valid(void);

#ifdef __cplusplus
}
#endif
