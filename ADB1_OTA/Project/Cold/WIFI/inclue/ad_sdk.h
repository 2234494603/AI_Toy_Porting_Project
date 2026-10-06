#ifndef __AD_SDK_H__
#define __AD_SDK_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 日志系统 ====================

/**
 * @brief 日志级别定义
 * 
 * 日志架构：
 * 应用层 → 日志宏(LOGE/LOGW/LOGI/LOGD) 
 *        ↓
 * SDK层  → ad_log_write() [格式化消息]
 *        ↓
 * 串口层 → usart_transport_log_output() [输出消息]
 *        ↓
 * 硬件层 → printf/UART/File
 */
typedef enum {
    AD_LOG_LEVEL_NONE = 0,    // 关闭日志
    AD_LOG_LEVEL_ERROR,       // 错误
    AD_LOG_LEVEL_WARN,        // 警告
    AD_LOG_LEVEL_INFO,        // 信息（默认级别）
    AD_LOG_LEVEL_DEBUG        // 调试
} ad_log_level_t;

// 编译时日志级别控制
#ifndef AD_LOG_LEVEL
#define AD_LOG_LEVEL 3
#endif

/**
 * @brief 日志输出函数（内部使用）
 * @param level 日志级别
 * @param tag 日志标签
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void ad_log_write(ad_log_level_t level, 
                  const char *tag, 
                  const char *format, 
                  ...) __attribute__((format(printf, 3, 4)));

// 日志宏定义
#if AD_LOG_LEVEL >= 1
#define AD_LOGE(tag, ...) \
    ad_log_write(AD_LOG_LEVEL_ERROR, tag, __VA_ARGS__)
#else
#define AD_LOGE(tag, ...) ((void)0)
#endif

#if AD_LOG_LEVEL >= 2
#define AD_LOGW(tag, ...) \
    ad_log_write(AD_LOG_LEVEL_WARN, tag, __VA_ARGS__)
#else
#define AD_LOGW(tag, ...) ((void)0)
#endif

#if AD_LOG_LEVEL >= 3
#define AD_LOGI(tag, ...) \
    ad_log_write(AD_LOG_LEVEL_INFO, tag, __VA_ARGS__)
#else
#define AD_LOGI(tag, ...) ((void)0)
#endif

#if AD_LOG_LEVEL >= 4
#define AD_LOGD(tag, ...) \
    ad_log_write(AD_LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
#else
#define AD_LOGD(tag, ...) ((void)0)
#endif

// ==================== 协议常量 ====================

// 帧头
#define AD_PROTO_FRAME_HEADER 0xDA28

// 协议版本
#define AD_PROTO_VERSION 0x01

// 消息最小长度（帧头+版本+命令字+数据长度+校验和）
#define AD_PROTO_MIN_MSG_LEN 9

// 最大消息总长度
#define AD_PROTO_MAX_MSG_LEN 512

// 最大载荷长度（总长度 - 固定开销）
#define AD_PROTO_MAX_PAYLOAD_LEN (AD_PROTO_MAX_MSG_LEN - AD_PROTO_MIN_MSG_LEN)

// ==================== 命令字定义 ====================

// 1. 模组初始化
#define AD_CMD_INIT                0x0100
#define AD_CMD_INIT_RESP           0x0101
#define AD_CMD_WAIT_INIT           0x0102

// 2. 心跳
#define AD_CMD_HEARTBEAT           0x0200
#define AD_CMD_HEARTBEAT_RESP      0x0201

// 4. 联网状态查询/维护
#define AD_CMD_QUERY_NETWORK       0x0400
#define AD_CMD_REPORT_NETWORK      0x0401
#define AD_CMD_QUERY_RSSI          0x0402
#define AD_CMD_REPORT_RSSI         0x0403
#define AD_CMD_CONNECT_NETWORK     0x0404
#define AD_CMD_DISCONNECT_NETWORK  0x0405
#define AD_CMD_START_PAIRING       0x0406
#define AD_CMD_STOP_PAIRING        0x0407

// 5. 模组控制
#define AD_CMD_FACTORY_RESET       0x0500
#define AD_CMD_FACTORY_RESET_RESP  0x0501
#define AD_CMD_SLEEP               0x0502
#define AD_CMD_SLEEP_RESP          0x0503

// 6. 获取时间
#define AD_CMD_GET_TIME            0x0600
#define AD_CMD_REPORT_TIME         0x0601
#define AD_CMD_GET_TIMEZONE        0x0602
#define AD_CMD_REPORT_TIMEZONE     0x0603

// 7. 数据点处理
#define AD_CMD_DATAPOINT_NOTIFY    0x0700
#define AD_CMD_DATAPOINT_REPORT    0x0701
#define AD_CMD_DATAPOINT_QUERY     0x0702

// 8. 命令处理
#define AD_CMD_COMMAND_NOTIFY      0x0800
#define AD_CMD_COMMAND_QUERY       0x0801
#define AD_CMD_COMMAND_REPORT      0x0802

// 9. 音频功能
#define AD_CMD_SET_VOLUME          0x0900
#define AD_CMD_REPORT_VOLUME       0x0901
#define AD_CMD_QUERY_VOLUME        0x0902

// 10. 播放音效
#define AD_CMD_SOUND_ENABLE        0x0a00
#define AD_CMD_SOUND_PLAY          0x0a01
#define AD_CMD_SOUND_STOP          0x0a02
#define AD_CMD_SOUND_STATUS        0x0a03

// 11. AI对话
#define AD_CMD_AI_START            0x0b00
#define AD_CMD_AI_FINISH_LISTEN    0x0b01
#define AD_CMD_AI_INTERRUPT        0x0b02
#define AD_CMD_AI_END              0x0b03
#define AD_CMD_AI_QUERY_STATUS     0x0b10
#define AD_CMD_AI_REPORT_STATUS    0x0b11

// 12. 音乐播放
#define AD_CMD_PLAYER_START         0x0c01
#define AD_CMD_PLAYER_PAUSE         0x0c03
#define AD_CMD_PLAYER_RESUME        0x0c04
#define AD_CMD_PLAYER_NEXT          0x0c05
#define AD_CMD_PLAYER_PREV          0x0c06
#define AD_CMD_PLAYER_END           0x0c07
#define AD_CMD_PLAYER_SEEK          0x0c09
#define AD_CMD_PLAYER_QUERY_STATUS  0x0c10
#define AD_CMD_PLAYER_REPORT_STATUS 0x0c11

#define AD_CMD_MCU_OTA_PREPARE       0x0d00
#define AD_CMD_MCU_OTA_PREPARE_RESP  0x0d01
#define AD_CMD_MCU_OTA_BOOT_READY    0x0d02
#define AD_CMD_MCU_OTA_DATA          0x0d03
#define AD_CMD_MCU_OTA_DATA_ACK      0x0d04
#define AD_CMD_MCU_OTA_FINISH        0x0d05
#define AD_CMD_MCU_OTA_FINISH_RESP   0x0d06
#define AD_CMD_MCU_OTA_STATUS        0x0d07

// ==================== 错误码 ====================

#define AD_RET_OK                  0x00  // 成功
#define AD_RET_UNKNOWN_CMD         0x01  // 未知命令
#define AD_RET_PARAM_ERROR         0x02  // 参数错误
#define AD_RET_LENGTH_ERROR        0x03  // 长度错误
#define AD_RET_CHECKSUM_ERROR      0x04  // 校验失败
#define AD_RET_BUSY                0x05  // 忙/状态不允许
#define AD_RET_TIMEOUT             0x06  // 超时/上游不可达
#define AD_RET_NOT_SUPPORTED       0x07  // 不支持的功能

// ==================== 返回值定义 ====================

#define AD_SDK_SUCCESS           0    // 成功
#define AD_SDK_ERROR            -1    // 通用错误
#define AD_SDK_ERROR_PARAM      -2    // 参数错误
#define AD_SDK_ERROR_TIMEOUT    -3    // 超时
#define AD_SDK_ERROR_NOT_INIT   -4    // 未初始化
#define AD_SDK_ERROR_BUSY       -5    // 忙
#define AD_SDK_ERROR_NO_MEM     -6    // 内存不足

// ==================== 状态值定义 ====================

// 模组状态（包含网络状态）
typedef enum {
    AD_MODULE_STATUS_NOT_PAIRED_INIT         = 0x00,  // 未配网，未初始化
    AD_MODULE_STATUS_NOT_PAIRED_PAIRING      = 0x01,  // 未配网，配网中
    AD_MODULE_STATUS_NOT_PAIRED_FAILED       = 0x02,  // 未配网，配网超时/失败
    AD_MODULE_STATUS_PAIRED_DISCONNECTED     = 0x03,  // 已配网，网络未连接
    AD_MODULE_STATUS_PAIRED_CONNECTING       = 0x04,  // 已配网，网络连接中
    AD_MODULE_STATUS_PAIRED_CONNECTED        = 0x05,  // 已配网，网络已连接
    AD_MODULE_STATUS_PAIRED_RESOURCE_UPDATING = 0x06,  // 已配网，资源更新中
    AD_MODULE_STATUS_PAIRED_FIRMWARE_UPDATING = 0x07,  // 已配网，固件升级中
    
    // SDK内部定义的事件状态
    AD_MODULE_STATUS_RESET                   = 0xFF   // 模组已重置
} ad_module_status_t;

// AI对话状态
typedef enum {
    AD_AI_STATUS_IDLE             = 0x00,  // 空闲
    AD_AI_STATUS_LISTEN           = 0x01,  // 监听
    AD_AI_STATUS_WAIT             = 0x02,  // 等待
    AD_AI_STATUS_SPEAK            = 0x03,  // 说话
    AD_AI_STATUS_SESSION_STARTING = 0x04,  // 会话启动中
} ad_ai_status_t;

// 音乐播放状态
typedef enum {
    AD_PLAYER_STATUS_STOPPED = 0x00,  // 已停止（空闲）
    AD_PLAYER_STATUS_PLAYING = 0x01,  // 播放中
    AD_PLAYER_STATUS_PAUSED  = 0x02,  // 已暂停
} ad_player_status_t;

// 音效播放状态（内部使用）
typedef enum {
    AD_SOUND_STATUS_PLAYING                = 0x00,  // 播放中
    AD_SOUND_STATUS_FAILED_AUDIO_BUSY      = 0x01,  // 播放失败-因为有音频在播放
    AD_SOUND_STATUS_FAILED_MODULE_DISABLED = 0x02,  // 播放失败-因为音效模块未启用
    AD_SOUND_STATUS_FAILED_AUDIO_NOT_FOUND = 0x03,  // 播放失败-因为音频不存在
    AD_SOUND_STATUS_ENDED_INTERRUPTED      = 0x10,  // 完结-因为被打断
    AD_SOUND_STATUS_ENDED_SUCCESS          = 0x11,  // 完结-播放成功
} ad_sound_status_t;

// 音效播放结果枚举（用户API）
typedef enum {
    AD_SOUND_PLAY_COMPLETE = 0,     // 成功播放完成
    AD_SOUND_PLAY_STOPPED = 1,      // 被手动停止
    AD_SOUND_PLAY_ERROR = 2,        // 播放出错
    AD_SOUND_PLAY_INTERRUPTED = 3,  // 被其他播放中断
    AD_SOUND_PLAY_DISABLED = 4,     // 音效模块被禁用
    AD_SOUND_PLAY_SKIPPED = 5,      // 播放被跳过（音频忙）
    AD_SOUND_PLAY_NOT_FOUND = 6     // 音频不存在
} ad_sound_play_result_t;

// 数据点类型
typedef enum {
    AD_DP_TYPE_INT = 1,            // 整数类型
    AD_DP_TYPE_BOOL = 2,           // 布尔类型
    AD_DP_TYPE_ENUM = 3,           // 枚举类型
    AD_DP_TYPE_STRING = 4,         // 字符串类型
    AD_DP_TYPE_BYTES = 5           // 字节数组类型
} ad_dp_type_t;

// 数据点变更来源
typedef enum {
    AD_DP_REASON_CLOUD_CONTROL = 0x01,  // 云端控制
    AD_DP_REASON_AI_CONTROL    = 0x02,  // AI控制
    AD_DP_REASON_FLASH_LOADED  = 0x03,  // 从flash加载
} ad_dp_reason_t;

// 命令来源类型
typedef enum {
    AD_COMMAND_SOURCE_CLOUD = 0x01,  // 云端
    AD_COMMAND_SOURCE_AI = 0x02,      // AI
} ad_command_source_type_t;

// ==================== 回调函数类型定义 ====================

/**
 * @brief 模组状态上报回调
 * @param status 模组状态
 */
typedef void (*ad_on_module_status_cb)(ad_module_status_t status);


/**
 * @brief 音量上报回调
 * @param volume 音量值（0-100）
 */
typedef void (*ad_on_volume_report_cb)(uint8_t volume);

/**
 * @brief 命令参数结构
 */
typedef struct {
    uint16_t param_id;      // 参数ID
    uint16_t param_type;    // 参数类型（同数据点类型）
    uint16_t value_len;     // 值长度（字符串类型时有效）
    const void *value;      // 参数值
} ad_command_param_t;

/**
 * @brief 数据点下发回调
 * @param reason 变更来源（云端控制/AI控制/从flash加载）
 * @param dp_id 数据点ID
 * @param dp_type 数据点类型
 * @param value 数据点值（根据类型解析）
 * @param value_len 值长度（字符串类型时有效）
 */
typedef void (*ad_on_datapoint_notify_cb)(ad_dp_reason_t reason, 
                                           uint16_t dp_id,
                                           ad_dp_type_t dp_type,
                                           const void *value,
                                           uint16_t value_len);

/**
 * @brief 命令下发回调
 * @param reason 命令来源（云端set/AI控制/从flash加载）
 * @param session_id 会话ID
 * @param cmd_id 命令ID
 * @param params 参数数组
 * @param param_count 参数个数
 */
typedef void (*ad_on_command_notify_cb)(uint8_t reason,
                                         uint16_t session_id,
                                         uint16_t cmd_id,
                                         const ad_command_param_t *params,
                                         uint16_t param_count);

/**
 * @brief 音效播放回调函数类型
 * @param result 播放结果
 * @param user_data 用户自定义数据
 */
typedef void (*ad_sound_play_callback_t)(ad_sound_play_result_t result, void* user_data);

/**
 * @brief AI状态上报回调
 * @param status AI状态
 */
typedef void (*ad_on_ai_status_cb)(ad_ai_status_t status);

/**
 * @brief 音乐状态上报回调
 * @param audio_id 音频资源ID（16字节，可能含'\0'）
 * @param progress_ms 播放进度（毫秒）
 * @param duration_ms 总时长（毫秒）
 * @param status 播放状态
 */
typedef void (*ad_on_player_status_cb)(const char *audio_id,
                                       uint32_t progress_ms,
                                       uint32_t duration_ms,
                                       ad_player_status_t status);

typedef void (*ad_on_ota_message_cb)(uint16_t cmd,
                                     const uint8_t *payload,
                                     uint16_t payload_len);

// ==================== 初始化配置 ====================

/**
 * @brief 初始化配置参数
 */
typedef struct {
    // 产品信息
    const char *mcu_pid;                    // MCU产品ID（必填）
    const char *mcu_firmware_id;            // MCU固件ID（必填）
    const char *mcu_firmware_version;       // MCU固件版本（必填）
    
    // 模组LED配置
    uint8_t module_working_led_pin;            // 模组工作LED引脚（0表示不启用）
    uint8_t module_working_led_active_high;    // 模组工作LED高电平有效
    uint8_t module_network_led_pin;            // 模组网络LED引脚（0表示不启用）
    uint8_t module_network_led_active_high;    // 模组网络LED高电平有效
    
    // 音效配置
    bool sound_enable;                  // 是否启用音效模块
    const uint16_t *system_sound_ids;   // 系统音效ID列表（NULL表示所有）
    uint16_t system_sound_count;        // 系统音效数量（0表示所有）
    const uint64_t *custom_sound_ids;   // 自定义音效ID列表
    uint16_t custom_sound_count;        // 自定义音效数量
} ad_sdk_config_t;

// ==================== 核心API ====================

/**
 * @brief 初始化MCU SDK
 * @param config 配置参数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_init(const ad_sdk_config_t *config);

/**
 * @brief 反初始化MCU SDK
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_deinit(void);

/**
 * @brief 处理接收到的UART数据
 * @note 此函数应在UART接收中断或任务中调用
 * @param data 接收到的数据
 * @param len 数据长度
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_handle_rx_data(const uint8_t *data, uint16_t len);

/**
 * @brief SDK主循环处理
 * @note 此函数应在主循环中定期调用，用于超时检测等
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_loop(void);

// ==================== 回调注册API ====================

/**
 * @brief 注册模组状态回调
 * @param callback 回调函数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_register_module_status_callback(ad_on_module_status_cb callback);

/**
 * @brief 注册数据点下发回调
 * @param callback 回调函数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_register_datapoint_notify_callback(ad_on_datapoint_notify_cb callback);

/**
 * @brief 注册命令下发回调
 * @param callback 回调函数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_register_command_notify_callback(ad_on_command_notify_cb callback);

/**
 * @brief 注册AI状态回调
 * @param callback 回调函数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_register_ai_status_callback(ad_on_ai_status_cb callback);

/**
 * @brief 注册音乐播放器状态回调
 * @param callback 回调函数
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_register_player_status_callback(ad_on_player_status_cb callback);

int ad_sdk_register_ota_callback(ad_on_ota_message_cb callback);
int ad_sdk_send_ota_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len);

// ==================== 网络API ====================

/**
 * @brief 查询网络状态
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_query_network_status(void);

/**
 * @brief 查询RSSI
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_query_rssi(void);

/**
 * @brief 获取RSSI值
 * @param rssi 输出：RSSI值（dBm）
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_get_rssi(int8_t *rssi);

/**
 * @brief 连接网络
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_connect_network(void);

/**
 * @brief 断开网络
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_disconnect_network(void);

/**
 * @brief 开始配网
 * @param timeout_ms 配网超时时间（毫秒），0表示使用默认值(180秒)
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_start_pairing(uint32_t timeout_ms);

/**
 * @brief 停止配网
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_stop_pairing(void);

// ==================== 模组控制API ====================

/**
 * @brief 复位模组
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_factory_reset_module(void);

/**
 * @brief 模组休眠
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_sleep_module(void);

// ==================== 时间API ====================

/**
 * @brief 获取时间
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_get_time(void);

/**
 * @brief 获取时区
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_get_timezone(void);

// ==================== 数据点API ====================

/**
 * @brief 字符串类型数据点值结构体
 */
typedef struct {
    const char *data;       // 字符串数据指针
    uint16_t length;        // 字符串长度（不包含'\0'）
} ad_dp_string_t;

/**
 * @brief 字节数组类型数据点值结构体
 */
typedef struct {
    const uint8_t *data;    // 字节数组数据指针
    uint16_t length;        // 字节数组长度
} ad_dp_bytes_t;

/**
 * @brief 数据点值结构
 */
typedef struct {
    uint16_t dp_id;         // 数据点ID
    ad_dp_type_t type;      // 数据点类型
    union {
        bool bool_value;            // 布尔值
        int32_t int_value;          // 整型值
        uint32_t enum_value;        // 枚举值
        ad_dp_string_t string_value;// 字符串值
        ad_dp_bytes_t bytes_value;  // 字节数组值
    } value;
} ad_dp_value_t;

/**
 * @brief 上报数据点变化
 * @param values 数据点值数组
 * @param count 数据点数量
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_report_datapoint(const ad_dp_value_t *values, uint16_t count);

/**
 * @brief 查询数据点值
 * @param dp_ids 数据点ID数组
 * @param count 数据点数量
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_query_datapoint(const uint16_t *dp_ids, uint16_t count);

// ==================== 命令API ====================

/**
 * @brief 命令执行结果
 */
typedef struct {
    uint16_t session_id;    // 会话ID
    uint8_t result;         // 结果（0x00=session不存在/不活跃，0x01=执行中，0x02=执行失败，0x03=执行成功）
} ad_cmd_result_t;

/**
 * @brief 上报命令执行结果
 * @param result 执行结果
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_report_command_result(const ad_cmd_result_t *result);

// ==================== 音频API ====================

/**
 * @brief 设置喇叭音量
 * @param volume 音量值（0-100）
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_set_speaker_volume(uint8_t volume);

/**
 * @brief 查询喇叭音量
 * @note 查询后，模组会通过上报消息更新音量值，使用 ad_sdk_get_speaker_volume() 获取
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_query_speaker_volume(void);

/**
 * @brief 获取当前喇叭音量
 * @return 当前音量值（0-100）
 */
uint8_t ad_sdk_get_speaker_volume(void);

// ==================== 音效API ====================

/**
 * @brief 启用/禁用音效
 * @param enable true=启用，false=禁用
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_sound_enable(bool enable);

/**
 * @brief 播放音效
 * @param session_id 会话ID（用于状态上报）
 * @param sound_id 音效ID
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_sound_play(uint16_t session_id, uint64_t sound_id);

/**
 * @brief 停止音效
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_sound_stop(void);

/**
 * @brief 播放音效
 * @param sound_id 音效ID
 * @param callback 播放结束回调函数，传NULL表示不需要回调
 * @param user_data 用户自定义数据，会在回调时原样传回
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_play_sound(uint64_t sound_id, ad_sound_play_callback_t callback, void* user_data);

// ==================== AI对话API ====================

/**
 * @brief 启动AI对话
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_ai_start(void);

/**
 * @brief 结束AI监听
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_ai_finish_listen(void);

/**
 * @brief 打断AI说话
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_ai_interrupt(void);

/**
 * @brief 结束AI对话
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_ai_end(void);

/**
 * @brief 查询AI状态
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_ai_query_status(void);

// ==================== 音乐播放API ====================

/**
 * @brief 启动音乐会话
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_start(void);

/**
 * @brief 暂停音乐
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_pause(void);

/**
 * @brief 恢复音乐
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_resume(void);

/**
 * @brief 下一首
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_next(void);

/**
 * @brief 上一首
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_prev(void);

/**
 * @brief 结束音乐会话
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_end(void);

/**
 * @brief 跳转到指定位置
 * @param position_ms 跳转位置（毫秒）
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_seek(uint32_t position_ms);

/**
 * @brief 查询音乐状态
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_player_query_status(void);

// ==================== RTC API ====================

/**
 * @brief 初始化RTC模块
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_rtc_init(void);

/**
 * @brief 反初始化RTC模块
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_rtc_deinit(void);

/**
 * @brief 更新RTC时间（在循环中调用）
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
int ad_sdk_rtc_update(void);

/**
 * @brief 获取RTC时间戳（毫秒）
 * @return 时间戳（毫秒）
 */
uint64_t ad_sdk_rtc_get_timestamp_ms(void);

/**
 * @brief 获取RTC时间戳（秒）
 * @return 时间戳（秒）
 */
uint64_t ad_sdk_rtc_get_timestamp_s(void);

/**
 * @brief 获取RTC时区偏移（分钟）
 * @return 时区偏移（分钟）
 */
int32_t ad_sdk_rtc_get_timezone_offset_mins(void);

#ifdef __cplusplus
}
#endif

#endif // __AD_SDK_H__
