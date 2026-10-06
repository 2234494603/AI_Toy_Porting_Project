#ifndef __APP_MODES_H__
#define __APP_MODES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 在线模式定义（与云平台数据点枚举值对应）
#define APP_MODE_AI      1    // AI语音模式
#define APP_MODE_PLAYER  3    // 在线播放器模式

/**
 * @brief 初始化在线模式管理模块
 * @return 0=成功, 非0=失败
 */
int app_modes_init(void);

/**
 * @brief 反初始化在线模式管理模块
 */
void app_modes_deinit(void);

/**
 * @brief 进入指定的在线模式
 * @param mode 要进入的模式 (1=AI, 3=Player)
 * @return 0=成功, 非0=失败
 */
int app_modes_enter_mode(uint8_t mode);

/**
 * @brief 获取当前激活的在线模式
 * @return 当前模式，如果没有激活模式则返回0
 */
uint8_t app_modes_get_current_mode(void);

/**
 * @brief 检查是否处于在线模式
 * @return true=处于在线模式, false=空闲
 */
bool app_modes_is_active(void);

/**
 * @brief 唤醒AI对话（KEY1单击触发）
 * @return 0=成功, 非0=失败
 */
int app_modes_wake_up_ai(void);

/**
 * @brief 打断AI对话（KEY1单击触发）
 * @return 0=成功, 非0=失败
 */
int app_modes_interrupt_ai(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_MODES_H__

