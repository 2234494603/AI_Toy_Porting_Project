#ifndef __APP_AI_H__
#define __APP_AI_H__

#include <stdint.h>
#include <stdbool.h>
#include "ad_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化AI在线模式管理模块
 * @return 0=成功, 非0=失败
 */
int app_ai_init(void);

/**
 * @brief 反初始化AI在线模式管理模块
 */
void app_ai_deinit(void);

/**
 * @brief 唤醒AI对话（进入监听状态）
 * @return 0=成功, 非0=失败
 */
int app_ai_wake_up(void);

/**
 * @brief 打断AI对话
 * @return 0=成功, 非0=失败
 */
int app_ai_interrupt(void);

/**
 * @brief 获取当前AI状态
 * @return 当前AI状态
 */
ad_ai_status_t app_ai_get_status(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_AI_H__

