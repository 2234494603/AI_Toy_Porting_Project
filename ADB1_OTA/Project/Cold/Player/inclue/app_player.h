#ifndef __APP_PLAYER_H__
#define __APP_PLAYER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化在线播放器模式管理模块
 * @return 0=成功, 非0=失败
 */
int app_player_init(void);

/**
 * @brief 反初始化在线播放器模式管理模块
 */
void app_player_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_PLAYER_H__

