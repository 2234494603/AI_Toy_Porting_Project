/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    soft_timer.c
  * @brief   软件定时器模块实现
  * @author  AI Assistant
  * @date    2025
  ******************************************************************************
  */
/* USER CODE END Header */

#include "soft_timer.h"
#include "main.h"
#include <string.h>

/* 软件定时器结构体 */
typedef struct {
    bool is_used;                  // 是否被使用
    bool is_running;               // 是否正在运行
    uint32_t period_ms;            // 定时周期（毫秒）
    uint32_t start_tick;           // 启动时的tick值
    SoftTimerCallback_t callback;  // 超时回调函数
    SoftTimerMode_t mode;          // 定时器模式
} SoftTimer_t;

/* 软件定时器数组 */
static SoftTimer_t soft_timers[SOFT_TIMER_MAX_COUNT];

/* 模块是否已初始化 */
static bool is_initialized = false;

/**
 * @brief 初始化软件定时器模块
 */
int8_t SoftTimer_Init(void) {
    if (is_initialized) {
        return 0;
    }
    
    // 清空所有定时器
    memset(soft_timers, 0, sizeof(soft_timers));
    
    is_initialized = true;
    return 0;
}

/**
 * @brief 创建一个软件定时器
 */
SoftTimerHandle_t SoftTimer_Create(uint32_t period_ms, SoftTimerCallback_t callback, SoftTimerMode_t mode) {
    if (!is_initialized || callback == NULL || period_ms == 0) {
        return -1;
    }
    
    // 查找空闲的定时器槽位
    for (int8_t i = 0; i < SOFT_TIMER_MAX_COUNT; i++) {
        if (!soft_timers[i].is_used) {
            // 初始化定时器
            soft_timers[i].is_used = true;
            soft_timers[i].is_running = false;
            soft_timers[i].period_ms = period_ms;
            soft_timers[i].start_tick = 0;
            soft_timers[i].callback = callback;
            soft_timers[i].mode = mode;
            
            return i;  // 返回定时器句柄
        }
    }
    
    return -1;  // 没有空闲槽位
}

/**
 * @brief 启动定时器
 */
int8_t SoftTimer_Start(SoftTimerHandle_t handle) {
    if (handle < 0 || handle >= SOFT_TIMER_MAX_COUNT) {
        return -1;
    }
    
    if (!soft_timers[handle].is_used) {
        return -1;
    }
    
    soft_timers[handle].is_running = true;
    soft_timers[handle].start_tick = HAL_GetTick();
    
    return 0;
}

/**
 * @brief 停止定时器
 */
int8_t SoftTimer_Stop(SoftTimerHandle_t handle) {
    if (handle < 0 || handle >= SOFT_TIMER_MAX_COUNT) {
        return -1;
    }
    
    if (!soft_timers[handle].is_used) {
        return -1;
    }
    
    soft_timers[handle].is_running = false;
    
    return 0;
}

/**
 * @brief 重启定时器
 */
int8_t SoftTimer_Restart(SoftTimerHandle_t handle) {
    if (handle < 0 || handle >= SOFT_TIMER_MAX_COUNT) {
        return -1;
    }
    
    if (!soft_timers[handle].is_used) {
        return -1;
    }
    
    soft_timers[handle].is_running = true;
    soft_timers[handle].start_tick = HAL_GetTick();
    
    return 0;
}

/**
 * @brief 修改定时器周期
 */
int8_t SoftTimer_SetPeriod(SoftTimerHandle_t handle, uint32_t period_ms) {
    if (handle < 0 || handle >= SOFT_TIMER_MAX_COUNT || period_ms == 0) {
        return -1;
    }
    
    if (!soft_timers[handle].is_used) {
        return -1;
    }
    
    soft_timers[handle].period_ms = period_ms;
    
    return 0;
}

/**
 * @brief 删除定时器
 */
int8_t SoftTimer_Delete(SoftTimerHandle_t handle) {
    if (handle < 0 || handle >= SOFT_TIMER_MAX_COUNT) {
        return -1;
    }
    
    if (!soft_timers[handle].is_used) {
        return -1;
    }
    
    // 清空定时器
    memset(&soft_timers[handle], 0, sizeof(SoftTimer_t));
    
    return 0;
}

/**
 * @brief 软件定时器处理函数（需要在主循环中周期调用）
 */
void SoftTimer_Process(void) {
    if (!is_initialized) {
        return;
    }
    
    uint32_t current_tick = HAL_GetTick();
    
    // 遍历所有定时器
    for (int8_t i = 0; i < SOFT_TIMER_MAX_COUNT; i++) {
        if (soft_timers[i].is_used && soft_timers[i].is_running) {
            // 检查是否超时
            uint32_t elapsed = current_tick - soft_timers[i].start_tick;
            
            if (elapsed >= soft_timers[i].period_ms) {
                // 定时器超时，调用回调函数
                if (soft_timers[i].callback != NULL) {
                    soft_timers[i].callback();
                }
                
                // 根据模式处理
                if (soft_timers[i].mode == SOFT_TIMER_MODE_PERIODIC) {
                    // 周期模式：重新计时
                    soft_timers[i].start_tick = current_tick;
                } else {
                    // 单次模式：停止定时器
                    soft_timers[i].is_running = false;
                }
            }
        }
    }
}

