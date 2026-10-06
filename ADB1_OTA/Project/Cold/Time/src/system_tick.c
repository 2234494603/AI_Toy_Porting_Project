/**
 * @file system_tick.c
 * @brief HAL tick adapter for the MCU SDK port layer.
 */
#include "system_tick.h"
#include "main.h"

uint64_t system_tick_get_ms(void) {
    return (uint64_t)HAL_GetTick();
}
