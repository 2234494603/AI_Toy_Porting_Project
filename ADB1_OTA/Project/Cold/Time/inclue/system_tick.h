/**
 * @file system_tick.h
 * @brief Time source used by the MCU SDK port layer.
 */
#ifndef __SYSTEM_TICK_H__
#define __SYSTEM_TICK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t system_tick_get_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_TICK_H__ */
