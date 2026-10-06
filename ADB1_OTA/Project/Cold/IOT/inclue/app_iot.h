#ifndef __APP_IOT_H__
#define __APP_IOT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化IoT业务模块
 * @note 注册数据点、命令、音量等回调
 * @return 成功返回0，失败返回错误码
 */
int app_iot_init(void);

/**
 * @brief 反初始化IoT业务模块
 */
void app_iot_deinit(void);

// ==================== 状态查询接口（用于测试） ====================

/**
 * @brief 获取LED总开关状态
 * @return true=开启, false=关闭
 */
bool app_iot_get_led_switch_state(void);

/**
 * @brief 获取喇叭音量
 * @return 音量值 (0-100)
 */
int app_iot_get_speaker_volume(void);

/**
 * @brief 设置喇叭音量并上报到云端
 * @param volume 音量值 (0-100)
 * @return 0=成功, 其他=失败
 */
int app_iot_set_and_report_speaker_volume(uint8_t volume);

/**
 * @brief 获取玩耍模式
 * @return 玩耍模式 (1=ai, 3=player)
 */
uint8_t app_iot_get_play_mode(void);

/**
 * @brief 获取指定LED的状态
 * @param led_number LED编号 (1-3)
 * @param is_on 输出：是否点亮
 * @param flash_times 输出：闪烁次数
 * @param remaining_flashes 输出：剩余闪烁次数
 * @return 0=成功, -1=LED编号无效
 */
int app_iot_get_led_state(uint8_t led_number, bool *is_on, 
                                   uint8_t *flash_times, int *remaining_flashes);

/**
 * @brief 检查上一个命令是否被执行
 * @return true=已执行, false=未执行/被忽略
 */
bool app_iot_get_last_command_executed(void);

/**
 * @brief 获取上一个命令的ID
 * @return 命令ID
 */
uint16_t app_iot_get_last_command_id(void);

/**
 * @brief 重置所有状态（用于测试初始化）
 */
void app_iot_reset_state(void);

/**
 * @brief 手动更新LED状态（模拟定时器到期）
 * @note 在实际应用中，这应该由定时器中断或轮询实现
 */
void app_iot_update_led_timers(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_IOT_H__

