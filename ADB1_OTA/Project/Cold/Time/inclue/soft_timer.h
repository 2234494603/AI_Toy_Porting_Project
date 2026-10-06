/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    soft_timer.h
  * @brief   软件定时器模块头文件
  * @author  AI Assistant
  * @date    2025
  ******************************************************************************
  * @attention
  * 基于HAL_GetTick()实现的软件定时器，支持多个定时器实例
  * 每个定时器可以独立设置周期和回调函数
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SOFT_TIMER_H__
#define __SOFT_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 软件定时器最大数量 */
#define SOFT_TIMER_MAX_COUNT  10

/* 软件定时器句柄类型 */
typedef int8_t SoftTimerHandle_t;

/* 定时器回调函数类型 */
typedef void (*SoftTimerCallback_t)(void);

/* 定时器模式 */
typedef enum {
    SOFT_TIMER_MODE_ONE_SHOT = 0,  // 单次触发
    SOFT_TIMER_MODE_PERIODIC = 1   // 周期触发
} SoftTimerMode_t;

/**
 * @brief 初始化软件定时器模块
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_Init(void);

/**
 * @brief 创建一个软件定时器
 * @param period_ms 定时周期（毫秒）
 * @param callback 超时回调函数
 * @param mode 定时器模式（单次/周期）
 * @return 定时器句柄（>=0），失败返回-1
 */
SoftTimerHandle_t SoftTimer_Create(uint32_t period_ms, SoftTimerCallback_t callback, SoftTimerMode_t mode);

/**
 * @brief 启动定时器
 * @param handle 定时器句柄
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_Start(SoftTimerHandle_t handle);

/**
 * @brief 停止定时器
 * @param handle 定时器句柄
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_Stop(SoftTimerHandle_t handle);

/**
 * @brief 重启定时器（重新计时）
 * @param handle 定时器句柄
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_Restart(SoftTimerHandle_t handle);

/**
 * @brief 修改定时器周期
 * @param handle 定时器句柄
 * @param period_ms 新的周期（毫秒）
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_SetPeriod(SoftTimerHandle_t handle, uint32_t period_ms);

/**
 * @brief 删除定时器
 * @param handle 定时器句柄
 * @return 0=成功, -1=失败
 */
int8_t SoftTimer_Delete(SoftTimerHandle_t handle);

/**
 * @brief 软件定时器处理函数（需要在主循环中周期调用）
 */
void SoftTimer_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_TIMER_H__ */

