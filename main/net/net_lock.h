#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void net_lock_init(void);
void net_lock_take(void);
void net_lock_give(void);

#ifdef __cplusplus
}
#endif
