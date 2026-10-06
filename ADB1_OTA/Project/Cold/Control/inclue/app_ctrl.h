/**
 * @file app_ctrl.h
 * @brief 应用控制模块头文件
 * 
 * 管理设备的状态机，包括配网、联网、唤醒等流程
 */
#ifndef __APP_CTRL_H__
#define __APP_CTRL_H__

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "ad_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================== 公共接口 ===============================

/**
 * @brief 初始化应用控制模块
 * @return 0=成功, 非0=失败
 */
int app_ctrl_init(void);

/**
 * @brief 反初始化应用控制模块
 */
void app_ctrl_deinit(void);

/**
 * @brief 处理网络状态更新
 * @param status 网络状态
 */
void app_ctrl_on_module_status(ad_module_status_t status);

/**
 * @brief 通知在线模式激活
 */
void app_ctrl_on_online_mode_active(void);

/**
 * @brief 通知在线模式空闲
 */
void app_ctrl_on_online_mode_idle(void);

/**
 * @brief 获取当前应用控制状态
 * @return 当前状态
 */
app_ctrl_state_t app_ctrl_get_state(void);

/**
 * @brief 获取当前是否处于唤醒状态
 * @return true=处于唤醒状态, false=未唤醒
 */
bool app_ctrl_is_awake(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_CTRL_H__

