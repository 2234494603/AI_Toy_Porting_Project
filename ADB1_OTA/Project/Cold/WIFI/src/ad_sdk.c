#include "ad_sdk.h"
#include "system_tick.h"
#include "usart_transport.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// ==================== 宏定义 ====================

// 最大回调数量
#define MAX_CALLBACKS 2

// 日志消息最大长度
#define AD_LOG_MAX_MESSAGE_LEN 512

// 定义日志TAG
static const char* TAG = "ad_sdk";

// 重置防抖时间（毫秒）
#define AD_RESET_DEBOUNCE_MS 3000

// ==================== 回调列表管理 ====================

typedef struct {
    ad_on_module_status_cb callbacks[MAX_CALLBACKS];
    uint8_t count;
} module_status_callback_list_t;

typedef struct {
    ad_on_datapoint_notify_cb callbacks[MAX_CALLBACKS];
    uint8_t count;
} datapoint_notify_callback_list_t;

typedef struct {
    ad_on_command_notify_cb callbacks[MAX_CALLBACKS];
    uint8_t count;
} command_notify_callback_list_t;

// 音效播放会话结构
typedef struct {
    uint16_t session_id;                         // 会话ID
    uint64_t sound_id;                           // 音效ID
    ad_sound_play_callback_t callback;           // 回调函数
    void* user_data;                             // 用户数据
    bool is_active;                              // 会话是否活跃
} sound_play_session_t;

typedef struct {
    ad_on_ai_status_cb callbacks[MAX_CALLBACKS];
    uint8_t count;
} ai_status_callback_list_t;

typedef struct {
    ad_on_player_status_cb callbacks[MAX_CALLBACKS];
    uint8_t count;
} player_status_callback_list_t;

// ==================== 内部状态 ====================

typedef struct {
    bool is_initialized;                                  // SDK是否已初始化
    ad_sdk_config_t config;                               // 保存的配置信息
    int8_t rssi;                                          // RSSI值（dBm）
    uint8_t speaker_volume;                               // 当前喇叭音量（0-100）
    
    // RTC状态
    uint64_t rtc_timestamp_ms;                           // RTC时间戳（毫秒）
    int32_t rtc_timezone_offset_mins;                    // RTC时区偏移（分钟）
    bool rtc_is_initialized;                             // RTC是否已初始化
    uint64_t rtc_last_update_time_ms;                    // RTC上次更新的系统时间（毫秒）
    
    uint64_t last_reset_time_ms;                         // 上次重置时间（用于防抖）
    
    // 回调列表
    module_status_callback_list_t module_callbacks;      // 模组状态回调列表
    datapoint_notify_callback_list_t datapoint_callbacks;// 数据点回调列表
    command_notify_callback_list_t command_callbacks;    // 命令回调列表
    ai_status_callback_list_t ai_callbacks;              // AI状态回调列表
    player_status_callback_list_t player_callbacks;      // 音乐状态回调列表
    ad_on_ota_message_cb ota_callback;
    
    // 音效播放会话
    sound_play_session_t sound_session;                  // 当前音效播放会话
    uint16_t next_sound_session_id;                      // 下一个会话ID
} ad_sdk_context_t;

static ad_sdk_context_t g_ad_ctx = {
    .is_initialized = false,
};

// ==================== 内部函数声明 ====================

static int send_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len);
static int send_simple_command(uint16_t cmd);
static int handle_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len);
static int send_init_command(void);

// ==================== 内部工具函数 ====================

/**
 * @brief 计算CRC16校验和（CCITT-FALSE算法）
 */
static uint16_t calculate_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;  // 初始值
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8U);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)(((uint32_t)crc << 1U) ^ 0x1021UL);  // 多项式
            } else {
                crc = (uint16_t)((uint32_t)crc << 1U);
            }
        }
    }
    
    return crc;
}

/**
 * @brief 从大端字节序读取uint16
 */
static uint16_t read_u16_be(const uint8_t *data) {
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

/**
 * @brief 从大端字节序读取uint32
 */
static uint32_t read_u32_be(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | 
           ((uint32_t)data[2] << 8) | data[3];
}

/**
 * @brief 从大端字节序读取int32（有符号整数）
 */
static int32_t read_i32_be(const uint8_t *data) {
    // 先按无符号读取，然后转换为有符号（保持补码表示）
    uint32_t unsigned_val = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | 
                            ((uint32_t)data[2] << 8) | data[3];
    return (int32_t)unsigned_val;
}

/**
 * @brief 从大端字节序读取uint64
 */
static uint64_t read_u64_be(const uint8_t *data) {
    return ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) |
           ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32) |
           ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) |
           ((uint64_t)data[6] << 8) | data[7];
}

/**
 * @brief 写入uint16到大端字节序
 */
static void write_u16_be(uint8_t *data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

/**
 * @brief 写入uint32到大端字节序
 */
static void write_u32_be(uint8_t *data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/**
 * @brief 写入int32到大端字节序（有符号整数）
 */
static void write_i32_be(uint8_t *data, int32_t value) {
    // 有符号整数的补码表示，按位操作与无符号相同
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/**
 * @brief 写入uint64到大端字节序
 */
static void write_u64_be(uint8_t *data, uint64_t value) {
    data[0] = (value >> 56) & 0xFF;
    data[1] = (value >> 48) & 0xFF;
    data[2] = (value >> 40) & 0xFF;
    data[3] = (value >> 32) & 0xFF;
    data[4] = (value >> 24) & 0xFF;
    data[5] = (value >> 16) & 0xFF;
    data[6] = (value >> 8) & 0xFF;
    data[7] = value & 0xFF;
}

// ==================== 消息发送 ====================

/**
 * @brief 发送消息到模组
 */
static int send_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len) {
    if (payload_len > AD_PROTO_MAX_PAYLOAD_LEN) {
        AD_LOGE(TAG, "Payload too large: %d", payload_len);
        return AD_SDK_ERROR_PARAM;
    }
    
    // 使用静态缓冲区，避免在嵌入式系统中频繁malloc/free导致的问题
    static uint8_t buffer[AD_PROTO_MAX_MSG_LEN];
    
    // 计算总长度
    uint16_t total_len = AD_PROTO_MIN_MSG_LEN + payload_len;
    
    // 填充消息头
    write_u16_be(&buffer[0], AD_PROTO_FRAME_HEADER);  // 帧头
    buffer[2] = AD_PROTO_VERSION;                         // 版本
    write_u16_be(&buffer[3], cmd);                     // 命令字
    write_u16_be(&buffer[5], payload_len);             // 数据长度
    
    // 填充载荷
    if (payload_len > 0 && payload) {
        memcpy(&buffer[7], payload, payload_len);
    }
    
    // 计算并填充CRC16校验和
    uint16_t crc = calculate_crc16(buffer, 7 + payload_len);
    write_u16_be(&buffer[7 + payload_len], crc);
    
    // 通过UART发送
    int ret = usart_transport_tx(buffer, total_len);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to send UART data");
        return AD_SDK_ERROR;
    }
    
    AD_LOGD(TAG, "Sent message: cmd=0x%04X, payload_len=%d", cmd, payload_len);
    return AD_SDK_SUCCESS;
}

/**
 * @brief 发送简单命令（无载荷）
 */
static int send_simple_command(uint16_t cmd) {
    return send_message(cmd, NULL, 0);
}

// ==================== RTC内部函数 ====================

/**
 * @brief 设置时间戳（内部函数）
 * @param timestamp_ms UTC时间戳（毫秒）
 * @return 0:成功 其他:失败
 */
static int set_timestamp(uint64_t timestamp_ms) {
    if (!g_ad_ctx.rtc_is_initialized) {
        AD_LOGE(TAG, "Failed to set timestamp - RTC not initialized");
        return -1;
    }
    
    AD_LOGD(TAG, "Setting timestamp to %llu ms", timestamp_ms);
    
    // 更新RTC时间戳
    g_ad_ctx.rtc_timestamp_ms = timestamp_ms;
    
    // 更新最后更新时间为当前系统tick
    g_ad_ctx.rtc_last_update_time_ms = system_tick_get_ms();
    
    AD_LOGI(TAG, "Timestamp set successfully");
    return 0;
}

/**
 * @brief 设置时区偏移（内部函数）
 * @param timezone_offset_mins 时区偏移（分钟）
 * @return 0:成功 其他:失败
 */
static int set_timezone_offset_mins(int32_t timezone_offset_mins) {
    if (!g_ad_ctx.rtc_is_initialized) {
        AD_LOGE(TAG, "Failed to set timezone - RTC not initialized");
        return -1;
    }
    
    AD_LOGD(TAG, "Setting timezone offset to %d mins", timezone_offset_mins);
    g_ad_ctx.rtc_timezone_offset_mins = timezone_offset_mins;
    return 0;
}

// ==================== 消息接收和解析 ====================

/**
 * @brief 处理接收到的消息
 */
static int handle_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len) {
    AD_LOGD(TAG, "Handle message: cmd=0x%04X, len=%d", cmd, payload_len);
    
    switch (cmd) {
        // 等待初始化指令
        case AD_CMD_WAIT_INIT: {
            AD_LOGI(TAG, "Received WAIT_INIT from module");
            
            // 防抖处理
            uint64_t current_time = system_tick_get_ms();
            
            // 第一次WAIT_INIT不触发RESET事件
            if (g_ad_ctx.last_reset_time_ms > 0) {
                // 不是第一次WAIT_INIT
                uint64_t time_diff = current_time - g_ad_ctx.last_reset_time_ms;
                AD_LOGD(TAG, "Time since last WAIT_INIT: %llu ms (debounce threshold: %d ms)", 
                        time_diff, AD_RESET_DEBOUNCE_MS);
                
                if (time_diff < AD_RESET_DEBOUNCE_MS) {
                    AD_LOGW(TAG, "Reset notification debounced (diff=%llu ms)", time_diff);
                } else {
                    // 超过防抖时间，这是一次新的模组重启
                    g_ad_ctx.last_reset_time_ms = current_time;
                    
                    // 通知应用层模组已重置
                    AD_LOGI(TAG, "Notifying module reset status (callback count: %d)", 
                            g_ad_ctx.module_callbacks.count);
                    for (uint8_t i = 0; i < g_ad_ctx.module_callbacks.count; i++) {
                        if (g_ad_ctx.module_callbacks.callbacks[i]) {
                            g_ad_ctx.module_callbacks.callbacks[i](AD_MODULE_STATUS_RESET);
                        }
                    }
                }
            } else {
                // 第一次WAIT_INIT，记录时间但不触发RESET事件
                g_ad_ctx.last_reset_time_ms = current_time;
                AD_LOGI(TAG, "First WAIT_INIT received (no reset event)");
            }
            
            // 始终发送初始化命令，响应模组请求
            AD_LOGI(TAG, "Sending init command");
            return send_init_command();
        }
        
        // 初始化响应
        case AD_CMD_INIT_RESP:
            if (payload_len >= 1) {
                int8_t result = (int8_t)payload[0];
                if (result == 0) {
                    AD_LOGI(TAG, "Module initialization successful");
                    // 初始化成功后自动查询网络状态
                    AD_LOGI(TAG, "Querying network status...");
                    int ret = send_simple_command(AD_CMD_QUERY_NETWORK);
                    if (ret != AD_SDK_SUCCESS) {
                        AD_LOGE(TAG, "Failed to query network status: %d", ret);
                    }
                } else {
                    AD_LOGE(TAG, "Module initialization failed with code: %d", result);
                }
            }
            break;
        
        // 心跳（SDK内部自动处理）
        case AD_CMD_HEARTBEAT:
            // 收到心跳，立即回复
            return send_simple_command(AD_CMD_HEARTBEAT_RESP);
        
        // 网络状态上报
        case AD_CMD_REPORT_NETWORK:
            if (payload_len >= 1) {
                ad_module_status_t status = (ad_module_status_t)payload[0];
                for (uint8_t i = 0; i < g_ad_ctx.module_callbacks.count; i++) {
                    if (g_ad_ctx.module_callbacks.callbacks[i]) {
                        g_ad_ctx.module_callbacks.callbacks[i](status);
                    }
                }
            }
            break;
        
        // RSSI上报
        case AD_CMD_REPORT_RSSI:
            if (payload_len >= 1) {
                g_ad_ctx.rssi = (int8_t)payload[0];
                AD_LOGD(TAG, "RSSI updated: %d dBm", g_ad_ctx.rssi);
            }
            break;
        
        // 时间上报
        case AD_CMD_REPORT_TIME:
            if (payload_len >= 9) {
                // 解析同步状态（1字节）
                bool synced = (payload[0] == 0x01);
                (void)synced;
                
                // 解析毫秒级时间戳（8字节）
                uint64_t timestamp_ms = read_u64_be(&payload[1]);
                
                AD_LOGD(TAG, "Time report: synced=%d, timestamp_ms=%llu", synced, timestamp_ms);
                
                // 更新RTC时间（已经是毫秒级别，直接设置）
                set_timestamp(timestamp_ms);
            }
            break;
        
        // 时区上报
        case AD_CMD_REPORT_TIMEZONE:
            if (payload_len >= 2) {
                int16_t timezone = (int16_t)read_u16_be(payload);
                
                // 更新RTC时区（timezone是时区的100倍，需要转换为分钟）
                // timezone = 100 * 小时偏移，转换为分钟: (timezone / 100) * 60 = timezone * 0.6
                int32_t timezone_offset_mins = (int32_t)((timezone * 60) / 100);
                set_timezone_offset_mins(timezone_offset_mins);
                
                AD_LOGD(TAG, "Timezone updated: %d (offset: %d mins)", timezone, timezone_offset_mins);
            }
            break;
        
        // 音量上报
        case AD_CMD_REPORT_VOLUME:
            if (payload_len >= 1) {
                g_ad_ctx.speaker_volume = payload[0];
                AD_LOGD(TAG, "Speaker volume updated: %u", g_ad_ctx.speaker_volume);
            }
            break;
        
        // 数据点下发
        case AD_CMD_DATAPOINT_NOTIFY:
            if (payload_len >= 3 && g_ad_ctx.datapoint_callbacks.count > 0) {
                ad_dp_reason_t reason = (ad_dp_reason_t)payload[0];
                uint16_t n_params = read_u16_be(&payload[1]);
                uint16_t offset = 3;

                // 静态临时变量存储转换后的值
                static int32_t converted_int_value;
                static uint16_t converted_enum_value;

                // 解析数据点列表
                for (uint16_t i = 0; i < n_params && offset + 6 <= payload_len; i++) {
                    uint16_t dp_id = read_u16_be(&payload[offset]);
                    uint16_t raw_dp_type = read_u16_be(&payload[offset + 2]);
                    ad_dp_type_t dp_type = (ad_dp_type_t)raw_dp_type;  // vtype 是 2 字节
                    uint16_t value_len = read_u16_be(&payload[offset + 4]);
                    offset += 6;

                    if (offset + value_len > payload_len) {
                        AD_LOGE(TAG, "Invalid datapoint value length");
                        break;
                    }

                    const void *value = &payload[offset];

                    // 根据类型进行大端序转换
                    switch (dp_type) {
                        case AD_DP_TYPE_INT:
                            if (value_len >= 4) {
                                converted_int_value = read_i32_be(&payload[offset]);
                                value = &converted_int_value;
                            }
                            break;
                        case AD_DP_TYPE_ENUM:
                            if (value_len >= 2) {
                                converted_enum_value = read_u16_be(&payload[offset]);
                                value = &converted_enum_value;
                            }
                            break;
                        case AD_DP_TYPE_BOOL:
                        case AD_DP_TYPE_STRING:
                        case AD_DP_TYPE_BYTES:
                            /* These types are already represented as byte sequences. */
                            break;
                        default:
                            break;
                    }

                    for (uint8_t j = 0; j < g_ad_ctx.datapoint_callbacks.count; j++) {
                        if (g_ad_ctx.datapoint_callbacks.callbacks[j]) {
                            g_ad_ctx.datapoint_callbacks.callbacks[j](reason, dp_id, dp_type,
                                                                       value, value_len);
                        }
                    }
                    offset += value_len;
                }
            }
            break;
        
        // 命令下发
        case AD_CMD_COMMAND_NOTIFY:
            if (payload_len >= 7 && g_ad_ctx.command_callbacks.count > 0) {
                uint8_t reason = payload[0];
                uint16_t session_id = read_u16_be(&payload[1]);
                uint16_t cmd_id = read_u16_be(&payload[3]);
                uint16_t n_params = read_u16_be(&payload[5]);
                uint16_t offset = 7;

                AD_LOGD(TAG, "Command parse: reason=%u, session=%u, cmd=%u, n_params=%u",
                        reason, session_id, cmd_id, n_params);

                // 解析参数列表 - 使用静态缓冲区避免malloc（更安全）
                #define MAX_CMD_PARAMS 8
                static ad_command_param_t params_buf[MAX_CMD_PARAMS];
                // 静态缓冲区存储转换后的参数值
                static int32_t converted_param_int_values[MAX_CMD_PARAMS];
                static uint16_t converted_param_enum_values[MAX_CMD_PARAMS];
                ad_command_param_t *params = NULL;

                // 检查参数数量
                if (n_params > MAX_CMD_PARAMS) {
                    AD_LOGE(TAG, "Too many params: %u (max=%u)", n_params, MAX_CMD_PARAMS);
                    break;
                }

                if (n_params > 0) {
                    params = params_buf;

                    // 初始化params数组为0
                    memset(params, 0, n_params * sizeof(ad_command_param_t));

                    AD_LOGD(TAG, "Parsing %u params, payload_len=%u", n_params, payload_len);

                    for (uint16_t i = 0; i < n_params; i++) {
                        // 检查是否有足够的空间读取参数头（6字节）
                        if (offset + 6 > payload_len) {
                            AD_LOGE(TAG, "Not enough data for param[%u] header: offset=%u, payload_len=%u",
                                    i, offset, payload_len);
                            break;  // 使用break而不是return，确保能继续处理
                        }

                        params[i].param_id = read_u16_be(&payload[offset]);
                        params[i].param_type = read_u16_be(&payload[offset + 2]);
                        params[i].value_len = read_u16_be(&payload[offset + 4]);
                        offset += 6;

                        AD_LOGD(TAG, "  Param[%u]: id=%u, type=%u, len=%u",
                                i, params[i].param_id, params[i].param_type,
                                params[i].value_len);

                        // 检查是否有足够的空间读取参数值
                        if (offset + params[i].value_len > payload_len) {
                            AD_LOGE(TAG, "Not enough data for param[%u] value: need %u bytes at offset %u",
                                    i, params[i].value_len, offset);
                            break;  // 使用break而不是return
                        }

                        // 根据参数类型进行大端序转换
                        switch ((ad_dp_type_t)params[i].param_type) {
                            case AD_DP_TYPE_INT:
                                if (params[i].value_len >= 4) {
                                    converted_param_int_values[i] = read_i32_be(&payload[offset]);
                                    params[i].value = &converted_param_int_values[i];
                                } else {
                                    params[i].value = &payload[offset];
                                }
                                break;
                            case AD_DP_TYPE_ENUM:
                                if (params[i].value_len >= 2) {
                                    converted_param_enum_values[i] = read_u16_be(&payload[offset]);
                                    params[i].value = &converted_param_enum_values[i];
                                } else {
                                    params[i].value = &payload[offset];
                                }
                                break;
                            case AD_DP_TYPE_BOOL:
                            case AD_DP_TYPE_STRING:
                            case AD_DP_TYPE_BYTES:
                                params[i].value = &payload[offset];
                                break;
                            default:
                                params[i].value = &payload[offset];
                                break;
                        }
                        offset += params[i].value_len;
                    }

                    AD_LOGD(TAG, "Params parsed successfully");
                }

                AD_LOGD(TAG, "Calling %u command callback(s)", g_ad_ctx.command_callbacks.count);
                for (uint8_t i = 0; i < g_ad_ctx.command_callbacks.count; i++) {
                    if (g_ad_ctx.command_callbacks.callbacks[i]) {
                        AD_LOGD(TAG, "  Calling callback[%u]", i);
                        g_ad_ctx.command_callbacks.callbacks[i](reason, session_id, cmd_id,
                                                                 params, n_params);
                        AD_LOGD(TAG, "  Callback[%u] returned successfully", i);
                    }
                }
                AD_LOGD(TAG, "Command handling complete, exiting switch case");
            }
            AD_LOGD(TAG, "CMD_COMMAND_NOTIFY case completed");
            break;
        
        // 音效状态
        case AD_CMD_SOUND_STATUS:
            if (payload_len >= 11) {
                uint16_t session_id = read_u16_be(&payload[0]);
                ad_sound_status_t status = (ad_sound_status_t)payload[10];
                
                // 检查是否匹配当前会话
                if (g_ad_ctx.sound_session.is_active && 
                    g_ad_ctx.sound_session.session_id == session_id) {
                    
                    // 转换内部状态到用户API结果
                    ad_sound_play_result_t result = AD_SOUND_PLAY_ERROR;
                    bool should_callback = false;
                    
                    switch (status) {
                        case AD_SOUND_STATUS_PLAYING:
                            // 播放中状态不触发回调
                            break;
                        
                        case AD_SOUND_STATUS_ENDED_SUCCESS:
                            result = AD_SOUND_PLAY_COMPLETE;
                            should_callback = true;
                            break;
                        
                        case AD_SOUND_STATUS_ENDED_INTERRUPTED:
                            result = AD_SOUND_PLAY_INTERRUPTED;
                            should_callback = true;
                            break;
                        
                        case AD_SOUND_STATUS_FAILED_AUDIO_BUSY:
                            result = AD_SOUND_PLAY_SKIPPED;
                            should_callback = true;
                            break;
                        
                        case AD_SOUND_STATUS_FAILED_MODULE_DISABLED:
                            result = AD_SOUND_PLAY_DISABLED;
                            should_callback = true;
                            break;
                        
                        case AD_SOUND_STATUS_FAILED_AUDIO_NOT_FOUND:
                            result = AD_SOUND_PLAY_NOT_FOUND;
                            should_callback = true;
                            break;
                        
                        default:
                            result = AD_SOUND_PLAY_ERROR;
                            should_callback = true;
                            break;
                    }
                    
                    // 触发回调
                    if (should_callback) {
                        // 保存回调信息
                        ad_sound_play_callback_t callback = g_ad_ctx.sound_session.callback;
                        void* user_data = g_ad_ctx.sound_session.user_data;
                        
                        // 清除会话
                        g_ad_ctx.sound_session.is_active = false;
                        g_ad_ctx.sound_session.callback = NULL;
                        g_ad_ctx.sound_session.user_data = NULL;
                        
                        // 调用回调
                        if (callback) {
                            callback(result, user_data);
                        }
                    }
                }
            }
            break;
        
        // AI状态上报
        case AD_CMD_AI_REPORT_STATUS:
            if (payload_len >= 1) {
                ad_ai_status_t status = (ad_ai_status_t)payload[0];
                for (uint8_t i = 0; i < g_ad_ctx.ai_callbacks.count; i++) {
                    if (g_ad_ctx.ai_callbacks.callbacks[i]) {
                        g_ad_ctx.ai_callbacks.callbacks[i](status);
                    }
                }
            }
            break;
        
        // 音乐状态上报
        case AD_CMD_PLAYER_REPORT_STATUS:
            if (payload_len >= 25) {
                char audio_id[17] = {0};
                memcpy(audio_id, payload, 16);
                uint32_t progress_ms = read_u32_be(&payload[16]);
                uint32_t duration_ms = read_u32_be(&payload[20]);
                ad_player_status_t status = (ad_player_status_t)payload[24];
                for (uint8_t i = 0; i < g_ad_ctx.player_callbacks.count; i++) {
                    if (g_ad_ctx.player_callbacks.callbacks[i]) {
                        g_ad_ctx.player_callbacks.callbacks[i](audio_id, progress_ms, duration_ms, status);
                    }
                }
            }
            break;

        case AD_CMD_MCU_OTA_PREPARE:
        case AD_CMD_MCU_OTA_BOOT_READY:
        case AD_CMD_MCU_OTA_DATA:
        case AD_CMD_MCU_OTA_DATA_ACK:
        case AD_CMD_MCU_OTA_FINISH:
        case AD_CMD_MCU_OTA_FINISH_RESP:
        case AD_CMD_MCU_OTA_STATUS:
            if (g_ad_ctx.ota_callback != NULL) {
                g_ad_ctx.ota_callback(cmd, payload, payload_len);
            }
            break;
        
        default:
            AD_LOGW(TAG, "Unknown command: 0x%04X", cmd);
            break;
    }
    
    return AD_SDK_SUCCESS;
}

// ==================== 内部函数实现 ====================

/**
 * @brief 发送初始化命令到模组
 * @return 成功返回AD_SDK_SUCCESS，失败返回错误码
 */
static int send_init_command(void) {
    const ad_sdk_config_t *config = &g_ad_ctx.config;
    
    // 构建初始化命令载荷
    uint8_t payload[AD_PROTO_MAX_PAYLOAD_LEN];
    uint16_t offset = 0;
    
    // 0x01: PID
    payload[offset++] = 0x01;
    uint16_t pid_len = (uint16_t)strlen(config->mcu_pid);
    write_u16_be(&payload[offset], pid_len);
    offset += 2;
    memcpy(&payload[offset], config->mcu_pid, pid_len);
    offset += pid_len;
    
    // 0x02: MCU firmware ID
    payload[offset++] = 0x02;
    uint16_t fw_id_len = (uint16_t)strlen(config->mcu_firmware_id);
    write_u16_be(&payload[offset], fw_id_len);
    offset += 2;
    memcpy(&payload[offset], config->mcu_firmware_id, fw_id_len);
    offset += fw_id_len;
    
    // 0x03: MCU firmware version
    payload[offset++] = 0x03;
    uint16_t fw_ver_len = (uint16_t)strlen(config->mcu_firmware_version);
    write_u16_be(&payload[offset], fw_ver_len);
    offset += 2;
    memcpy(&payload[offset], config->mcu_firmware_version, fw_ver_len);
    offset += fw_ver_len;
    
    // 0x10: module_working_led_pin
    if (config->module_working_led_pin > 0) {
        payload[offset++] = 0x10;
        write_u16_be(&payload[offset], 1);
        offset += 2;
        payload[offset++] = config->module_working_led_pin;
        
        // 0x11: module_working_led_active_high
        payload[offset++] = 0x11;
        write_u16_be(&payload[offset], 1);
        offset += 2;
        payload[offset++] = config->module_working_led_active_high;
    }
    
    // 0x12: module_network_led_pin
    if (config->module_network_led_pin > 0) {
        payload[offset++] = 0x12;
        write_u16_be(&payload[offset], 1);
        offset += 2;
        payload[offset++] = config->module_network_led_pin;
        
        // 0x13: module_network_led_active_high
        payload[offset++] = 0x13;
        write_u16_be(&payload[offset], 1);
        offset += 2;
        payload[offset++] = config->module_network_led_active_high;
    }
    
    // 0x20: sound_enable
    payload[offset++] = 0x20;
    write_u16_be(&payload[offset], 1);
    offset += 2;
    payload[offset++] = config->sound_enable ? 1 : 0;
    
    // 0x21: system_sound_ids
    if (config->system_sound_count > 0 && config->system_sound_ids) {
        payload[offset++] = 0x21;
        uint16_t sys_sound_len = config->system_sound_count * 2;
        write_u16_be(&payload[offset], sys_sound_len);
        offset += 2;
        for (uint16_t i = 0; i < config->system_sound_count; i++) {
            write_u16_be(&payload[offset], config->system_sound_ids[i]);
            offset += 2;
        }
    }
    
    // 0x22: custom_sound_ids
    if (config->custom_sound_count > 0 && config->custom_sound_ids) {
        payload[offset++] = 0x22;
        uint16_t custom_sound_len = config->custom_sound_count * 8;
        write_u16_be(&payload[offset], custom_sound_len);
        offset += 2;
        for (uint16_t i = 0; i < config->custom_sound_count; i++) {
            write_u64_be(&payload[offset], config->custom_sound_ids[i]);
            offset += 8;
        }
    }
    
    // 发送初始化命令
    return send_message(AD_CMD_INIT, payload, offset);
}

// ==================== 核心API实现 ====================

int ad_sdk_init(const ad_sdk_config_t *config) {
    if (g_ad_ctx.is_initialized) {
        AD_LOGW(TAG, "SDK already initialized");
        return AD_SDK_SUCCESS;
    }
    
    if (!config || !config->mcu_pid || !config->mcu_firmware_id || !config->mcu_firmware_version) {
        AD_LOGE(TAG, "Invalid config");
        return AD_SDK_ERROR_PARAM;
    }
    
    AD_LOGI(TAG, "Initializing MCU SDK");
    
    // 初始化UART
    int ret = usart_transport_init();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to initialize UART");
        return AD_SDK_ERROR;
    }
    
    // 初始化RTC模块
    ret = ad_sdk_rtc_init();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to initialize RTC");
        return AD_SDK_ERROR;
    }
    
    // 保存配置信息
    memcpy(&g_ad_ctx.config, config, sizeof(ad_sdk_config_t));
    
    // 初始化RSSI为无效值
    g_ad_ctx.rssi = -128;
    
    // 清空回调列表
    memset(&g_ad_ctx.module_callbacks, 0, sizeof(g_ad_ctx.module_callbacks));
    memset(&g_ad_ctx.datapoint_callbacks, 0, sizeof(g_ad_ctx.datapoint_callbacks));
    memset(&g_ad_ctx.command_callbacks, 0, sizeof(g_ad_ctx.command_callbacks));
    memset(&g_ad_ctx.ai_callbacks, 0, sizeof(g_ad_ctx.ai_callbacks));
    memset(&g_ad_ctx.player_callbacks, 0, sizeof(g_ad_ctx.player_callbacks));
    
    // 初始化音效会话
    memset(&g_ad_ctx.sound_session, 0, sizeof(g_ad_ctx.sound_session));
    g_ad_ctx.next_sound_session_id = 1;
    
    g_ad_ctx.is_initialized = true;
    
    // 注意：不再立即发送初始化命令，而是等待模组的 WAIT_INIT 请求
    AD_LOGI(TAG, "MCU SDK initialized, waiting for module WAIT_INIT request");
    return AD_SDK_SUCCESS;
}

int ad_sdk_deinit(void) {
    if (!g_ad_ctx.is_initialized) {
        AD_LOGW(TAG, "SDK not initialized");
        return AD_SDK_SUCCESS;
    }
    
    AD_LOGI(TAG, "Deinitializing MCU SDK");
    
    // 反初始化RTC模块
    ad_sdk_rtc_deinit();
    
    // 反初始化UART
    usart_transport_deinit();
    
    // 重置所有状态
    g_ad_ctx.is_initialized = false;
    memset(&g_ad_ctx, 0, sizeof(g_ad_ctx));
    
    AD_LOGI(TAG, "MCU SDK deinitialized");
    return AD_SDK_SUCCESS;
}

int ad_sdk_handle_rx_data(const uint8_t *data, uint16_t len) {
    if (!g_ad_ctx.is_initialized) {
        AD_LOGE(TAG, "SDK not initialized");
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    if (!data || len == 0) {
        AD_LOGE(TAG, "Invalid data");
        return AD_SDK_ERROR_PARAM;
    }
    
    uint16_t offset = 0;
    int processed_count = 0;
    
    // 循环处理缓冲区中的所有消息（支持粘连消息）
    while (offset < len) {
        uint16_t remaining = len - offset;
        const uint8_t *current = data + offset;
        
        // 检查是否有足够的数据包含最小消息
        if (remaining < AD_PROTO_MIN_MSG_LEN) {
            if (remaining > 0) {
                AD_LOGW(TAG, "Remaining data too short: %d bytes at offset %d, discarded", remaining, offset);
            }
            break;
        }
        
        // 检查帧头
        uint16_t frame_header = read_u16_be(current);
        if (frame_header != AD_PROTO_FRAME_HEADER) {
            AD_LOGW(TAG, "Invalid frame header at offset %d: 0x%04X, searching for next valid frame", offset, frame_header);
            // 尝试查找下一个有效帧头
            offset++;
            continue;
        }
        
        // 检查版本
        uint8_t version = current[2];
        if (version != AD_PROTO_VERSION) {
            AD_LOGW(TAG, "Unsupported version at offset %d: 0x%02X, skip this frame", offset, version);
            offset++;
            continue;
        }
        
        // 读取命令字和数据长度
        uint16_t cmd = read_u16_be(&current[3]);
        uint16_t payload_len = read_u16_be(&current[5]);
        
        // 检查载荷长度
        if (payload_len > AD_PROTO_MAX_PAYLOAD_LEN) {
            AD_LOGE(TAG, "Payload too large at offset %d: %d, skip this frame", offset, payload_len);
            offset++;
            continue;
        }
        
        // 计算完整消息长度
        uint16_t msg_len = AD_PROTO_MIN_MSG_LEN + payload_len;
        
        // 检查剩余数据是否足够一个完整消息
        if (remaining < msg_len) {
            AD_LOGW(TAG, "Incomplete message at offset %d: need=%d, have=%d, waiting for more data", 
                    offset, msg_len, remaining);
            break;
        }
        
        // 验证CRC校验和
        uint16_t received_crc = read_u16_be(&current[7 + payload_len]);
        uint16_t calculated_crc = calculate_crc16(current, 7 + payload_len);
        
        if (received_crc != calculated_crc) {
            AD_LOGE(TAG, "CRC mismatch at offset %d: received=0x%04X, calculated=0x%04X, skip this frame", 
                    offset, received_crc, calculated_crc);
            offset++;
            continue;
        }
        
        // 处理消息
        const uint8_t *payload = (payload_len > 0) ? &current[7] : NULL;
        int ret = handle_message(cmd, payload, payload_len);
        if (ret != AD_SDK_SUCCESS) {
            AD_LOGW(TAG, "Failed to handle message at offset %d: cmd=0x%04X", offset, cmd);
        }
        
        // 移动到下一个消息
        offset += msg_len;
        processed_count++;
        
        // 如果处理了至少一个消息且还有剩余数据，记录日志
        if (offset < len) {
            AD_LOGI(TAG, "Detected concatenated messages, continue parsing (offset=%d, remaining=%d)", offset, len - offset);
        }
    }
    
    if (processed_count > 0) {
        AD_LOGI(TAG, "Successfully processed %d message(s) from %d bytes", processed_count, len);
        return AD_SDK_SUCCESS;
    } else {
        AD_LOGE(TAG, "No valid message processed from %d bytes", len);
        return AD_SDK_ERROR_PARAM;
    }
}

int ad_sdk_loop(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    // 更新RTC时间
    ad_sdk_rtc_update();
    
    // 可在此处添加超时检测等逻辑
    return AD_SDK_SUCCESS;
}

// ==================== 回调注册API ====================

int ad_sdk_register_module_status_callback(ad_on_module_status_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    if (g_ad_ctx.module_callbacks.count >= MAX_CALLBACKS) return AD_SDK_ERROR_BUSY;
    g_ad_ctx.module_callbacks.callbacks[g_ad_ctx.module_callbacks.count++] = callback;
    return AD_SDK_SUCCESS;
}


int ad_sdk_register_datapoint_notify_callback(ad_on_datapoint_notify_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    if (g_ad_ctx.datapoint_callbacks.count >= MAX_CALLBACKS) return AD_SDK_ERROR_BUSY;
    g_ad_ctx.datapoint_callbacks.callbacks[g_ad_ctx.datapoint_callbacks.count++] = callback;
    return AD_SDK_SUCCESS;
}

int ad_sdk_register_command_notify_callback(ad_on_command_notify_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    if (g_ad_ctx.command_callbacks.count >= MAX_CALLBACKS) return AD_SDK_ERROR_BUSY;
    g_ad_ctx.command_callbacks.callbacks[g_ad_ctx.command_callbacks.count++] = callback;
    return AD_SDK_SUCCESS;
}

int ad_sdk_register_ai_status_callback(ad_on_ai_status_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    if (g_ad_ctx.ai_callbacks.count >= MAX_CALLBACKS) return AD_SDK_ERROR_BUSY;
    g_ad_ctx.ai_callbacks.callbacks[g_ad_ctx.ai_callbacks.count++] = callback;
    return AD_SDK_SUCCESS;
}

int ad_sdk_register_player_status_callback(ad_on_player_status_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    if (g_ad_ctx.player_callbacks.count >= MAX_CALLBACKS) return AD_SDK_ERROR_BUSY;
    g_ad_ctx.player_callbacks.callbacks[g_ad_ctx.player_callbacks.count++] = callback;
    return AD_SDK_SUCCESS;
}

int ad_sdk_register_ota_callback(ad_on_ota_message_cb callback) {
    if (!callback) return AD_SDK_ERROR_PARAM;
    g_ad_ctx.ota_callback = callback;
    return AD_SDK_SUCCESS;
}

int ad_sdk_send_ota_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len) {
    if (cmd < AD_CMD_MCU_OTA_PREPARE || cmd > AD_CMD_MCU_OTA_STATUS) return AD_SDK_ERROR_PARAM;
    return send_message(cmd, payload, payload_len);
}

// ==================== 网络API ====================

int ad_sdk_query_network_status(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_QUERY_NETWORK);
}

int ad_sdk_query_rssi(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_QUERY_RSSI);
}

int ad_sdk_get_rssi(int8_t *rssi) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    if (!rssi) {
        return AD_SDK_ERROR_PARAM;
    }
    *rssi = g_ad_ctx.rssi;
    return AD_SDK_SUCCESS;
}

int ad_sdk_connect_network(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_CONNECT_NETWORK);
}

int ad_sdk_disconnect_network(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_DISCONNECT_NETWORK);
}

int ad_sdk_start_pairing(uint32_t timeout_ms) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    // 如果超时为0，使用默认值180秒
    if (timeout_ms == 0) {
        timeout_ms = 180000;
    }
    
    uint8_t payload[4];
    write_u32_be(payload, timeout_ms);
    
    return send_message(AD_CMD_START_PAIRING, payload, 4);
}

int ad_sdk_stop_pairing(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_STOP_PAIRING);
}

// ==================== 模组控制API ====================

int ad_sdk_factory_reset_module(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    AD_LOGI(TAG, "Sending factory reset command to module");
    return send_simple_command(AD_CMD_FACTORY_RESET);
}

int ad_sdk_sleep_module(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_SLEEP);
}

// ==================== 时间API ====================

int ad_sdk_get_time(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_GET_TIME);
}

int ad_sdk_get_timezone(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_GET_TIMEZONE);
}

// ==================== 数据点API ====================

int ad_sdk_report_datapoint(const ad_dp_value_t *values, uint16_t count) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    if (!values || count == 0) {
        return AD_SDK_ERROR_PARAM;
    }
    
    uint8_t payload[AD_PROTO_MAX_PAYLOAD_LEN];
    uint16_t offset = 0;
    
    // n_params (2 字节)
    write_u16_be(&payload[offset], count);
    offset += 2;
    
    for (uint16_t i = 0; i < count; i++) {
        // dp_id (2 字节)
        write_u16_be(&payload[offset], values[i].dp_id);
        offset += 2;
        
        // vtype (2 字节)
        write_u16_be(&payload[offset], (uint16_t)values[i].type);
        offset += 2;
        
        // 根据类型写入值长度和值
        uint16_t value_len = 0;
        
        switch (values[i].type) {
            case AD_DP_TYPE_BOOL:
                value_len = 1;
                write_u16_be(&payload[offset], value_len);
                payload[offset + 2] = values[i].value.bool_value ? 1 : 0;
                break;
            case AD_DP_TYPE_INT:
                value_len = 4;
                write_u16_be(&payload[offset], value_len);
                write_i32_be(&payload[offset + 2], values[i].value.int_value);
                break;
            case AD_DP_TYPE_ENUM:
                value_len = 2;  // 枚举是 2 字节，不是 4 字节
                write_u16_be(&payload[offset], value_len);
                write_u16_be(&payload[offset + 2], (uint16_t)values[i].value.enum_value);
                break;
            case AD_DP_TYPE_STRING:
                // 字符串类型
                if (!values[i].value.string_value.data || 
                    values[i].value.string_value.length == 0 || 
                    values[i].value.string_value.length > 32) {
                    AD_LOGE(TAG, "Invalid string value: len=%u", values[i].value.string_value.length);
                    return AD_SDK_ERROR_PARAM;
                }
                value_len = values[i].value.string_value.length;
                write_u16_be(&payload[offset], value_len);
                memcpy(&payload[offset + 2], values[i].value.string_value.data, value_len);
                break;
            case AD_DP_TYPE_BYTES:
                // 字节数组类型
                if (!values[i].value.bytes_value.data || 
                    values[i].value.bytes_value.length == 0 || 
                    values[i].value.bytes_value.length > 32) {
                    AD_LOGE(TAG, "Invalid bytes value: len=%u", values[i].value.bytes_value.length);
                    return AD_SDK_ERROR_PARAM;
                }
                value_len = values[i].value.bytes_value.length;
                write_u16_be(&payload[offset], value_len);
                memcpy(&payload[offset + 2], values[i].value.bytes_value.data, value_len);
                break;
            default:
                return AD_SDK_ERROR_PARAM;
        }
        
        // vlen (2 字节) + value
        offset += 2 + value_len;
    }
    
    return send_message(AD_CMD_DATAPOINT_REPORT, payload, offset);
}

int ad_sdk_query_datapoint(const uint16_t *dp_ids, uint16_t count) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    if (!dp_ids || count == 0) {
        return AD_SDK_ERROR_PARAM;
    }
    
    uint8_t payload[AD_PROTO_MAX_PAYLOAD_LEN];
    uint16_t offset = 0;
    
    // n (2 字节) - 要查询的数据点数量
    write_u16_be(&payload[offset], count);
    offset += 2;
    
    // dp_id 列表
    for (uint16_t i = 0; i < count; i++) {
        write_u16_be(&payload[offset], dp_ids[i]);
        offset += 2;
    }
    
    return send_message(AD_CMD_DATAPOINT_QUERY, payload, offset);
}

// ==================== 命令API ====================

int ad_sdk_report_command_result(const ad_cmd_result_t *result) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    if (!result) {
        return AD_SDK_ERROR_PARAM;
    }
    
    uint8_t payload[3];
    write_u16_be(&payload[0], result->session_id);
    payload[2] = result->result;
    
    return send_message(AD_CMD_COMMAND_REPORT, payload, 3);
}

// ==================== 音频API ====================

int ad_sdk_set_speaker_volume(uint8_t volume) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    if (volume > 100) {
        return AD_SDK_ERROR_PARAM;
    }
    
    uint8_t payload[1] = {volume};
    return send_message(AD_CMD_SET_VOLUME, payload, 1);
}

int ad_sdk_query_speaker_volume(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_QUERY_VOLUME);
}

uint8_t ad_sdk_get_speaker_volume(void) {
    return g_ad_ctx.speaker_volume;
}

// ==================== 音效API ====================

int ad_sdk_sound_enable(bool enable) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    uint8_t payload[1] = {enable ? 1 : 0};
    return send_message(AD_CMD_SOUND_ENABLE, payload, 1);
}

int ad_sdk_sound_play(uint16_t session_id, uint64_t sound_id) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    uint8_t payload[10];
    write_u16_be(&payload[0], session_id);
    write_u64_be(&payload[2], sound_id);
    
    return send_message(AD_CMD_SOUND_PLAY, payload, 10);
}

int ad_sdk_sound_stop(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    // 如果有活跃的会话，清除它
    if (g_ad_ctx.sound_session.is_active) {
        // 保存回调信息
        ad_sound_play_callback_t callback = g_ad_ctx.sound_session.callback;
        void* user_data = g_ad_ctx.sound_session.user_data;
        
        // 清除会话
        g_ad_ctx.sound_session.is_active = false;
        g_ad_ctx.sound_session.callback = NULL;
        g_ad_ctx.sound_session.user_data = NULL;
        
        // 调用回调通知停止
        if (callback) {
            callback(AD_SOUND_PLAY_STOPPED, user_data);
        }
    }
    
    return send_simple_command(AD_CMD_SOUND_STOP);
}

int ad_sdk_play_sound(uint64_t sound_id, ad_sound_play_callback_t callback, void* user_data) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    // 如果有活跃的会话，先通知它被中断
    if (g_ad_ctx.sound_session.is_active) {
        ad_sound_play_callback_t old_callback = g_ad_ctx.sound_session.callback;
        void* old_user_data = g_ad_ctx.sound_session.user_data;
        
        // 清除旧会话
        g_ad_ctx.sound_session.is_active = false;
        
        // 通知旧会话被中断
        if (old_callback) {
            old_callback(AD_SOUND_PLAY_INTERRUPTED, old_user_data);
        }
        
        // 发送停止命令
        send_simple_command(AD_CMD_SOUND_STOP);
    }
    
    // 生成新的会话ID
    uint16_t session_id = g_ad_ctx.next_sound_session_id++;
    if (g_ad_ctx.next_sound_session_id == 0) {
        g_ad_ctx.next_sound_session_id = 1;  // 避免使用0
    }
    
    // 创建新会话
    g_ad_ctx.sound_session.session_id = session_id;
    g_ad_ctx.sound_session.sound_id = sound_id;
    g_ad_ctx.sound_session.callback = callback;
    g_ad_ctx.sound_session.user_data = user_data;
    g_ad_ctx.sound_session.is_active = true;
    
    // 发送播放命令
    uint8_t payload[11];
    write_u16_be(&payload[0], session_id);
    write_u64_be(&payload[2], sound_id);
    payload[10] = 0;  // force_play = 0 (不强制播放)
    
    int ret = send_message(AD_CMD_SOUND_PLAY, payload, 11);
    if (ret != AD_SDK_SUCCESS) {
        // 如果发送失败，清除会话并通知错误
        g_ad_ctx.sound_session.is_active = false;
        if (callback) {
            callback(AD_SOUND_PLAY_ERROR, user_data);
        }
        return ret;
    }
    
    return AD_SDK_SUCCESS;
}

// ==================== AI对话API ====================

int ad_sdk_ai_start(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_AI_START);
}

int ad_sdk_ai_finish_listen(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_AI_FINISH_LISTEN);
}

int ad_sdk_ai_interrupt(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_AI_INTERRUPT);
}

int ad_sdk_ai_end(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_AI_END);
}

int ad_sdk_ai_query_status(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_AI_QUERY_STATUS);
}

// ==================== 音乐播放API ====================

int ad_sdk_player_start(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_START);
}

int ad_sdk_player_pause(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_PAUSE);
}

int ad_sdk_player_resume(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_RESUME);
}

int ad_sdk_player_next(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_NEXT);
}

int ad_sdk_player_prev(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_PREV);
}

int ad_sdk_player_end(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_END);
}

int ad_sdk_player_seek(uint32_t position_ms) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    uint8_t payload[4];
    write_u32_be(payload, position_ms);
    
    return send_message(AD_CMD_PLAYER_SEEK, payload, 4);
}

int ad_sdk_player_query_status(void) {
    if (!g_ad_ctx.is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    return send_simple_command(AD_CMD_PLAYER_QUERY_STATUS);
}

// ==================== RTC API ====================

int ad_sdk_rtc_init(void) {
    if (g_ad_ctx.rtc_is_initialized) {
        AD_LOGW(TAG, "RTC already initialized");
        return AD_SDK_SUCCESS;
    }

    g_ad_ctx.rtc_timestamp_ms = 0;
    g_ad_ctx.rtc_timezone_offset_mins = 0;
    g_ad_ctx.rtc_is_initialized = true;
    g_ad_ctx.rtc_last_update_time_ms = system_tick_get_ms();
    
    AD_LOGI(TAG, "RTC initialized");
    return AD_SDK_SUCCESS;
}

int ad_sdk_rtc_deinit(void) {
    g_ad_ctx.rtc_timestamp_ms = 0;
    g_ad_ctx.rtc_timezone_offset_mins = 0;
    g_ad_ctx.rtc_is_initialized = false;
    g_ad_ctx.rtc_last_update_time_ms = 0;
    
    AD_LOGI(TAG, "RTC deinitialized");
    return AD_SDK_SUCCESS;
}

int ad_sdk_rtc_update(void) {
    if (!g_ad_ctx.rtc_is_initialized) {
        return AD_SDK_ERROR_NOT_INIT;
    }
    
    uint64_t current_tick_ms = system_tick_get_ms();
    
    // 检查时间是否向前移动
    if (current_tick_ms >= g_ad_ctx.rtc_last_update_time_ms) {
        uint64_t time_diff_ms = current_tick_ms - g_ad_ctx.rtc_last_update_time_ms;
        g_ad_ctx.rtc_timestamp_ms += time_diff_ms;
    } else {
        // 处理时间溢出情况
        uint64_t max_uint64 = UINT64_MAX;
        uint64_t time_diff_ms = (max_uint64 - g_ad_ctx.rtc_last_update_time_ms) + current_tick_ms + 1;
        g_ad_ctx.rtc_timestamp_ms += time_diff_ms;
    }
    
    g_ad_ctx.rtc_last_update_time_ms = current_tick_ms;
    return AD_SDK_SUCCESS;
}

uint64_t ad_sdk_rtc_get_timestamp_ms(void) {
    if (!g_ad_ctx.rtc_is_initialized) {
        return 0;
    }
    return g_ad_ctx.rtc_timestamp_ms;
}

uint64_t ad_sdk_rtc_get_timestamp_s(void) {
    if (!g_ad_ctx.rtc_is_initialized) {
        return 0;
    }
    return g_ad_ctx.rtc_timestamp_ms / 1000;
}

int32_t ad_sdk_rtc_get_timezone_offset_mins(void) {
    if (!g_ad_ctx.rtc_is_initialized) {
        return 0;
    }
    return g_ad_ctx.rtc_timezone_offset_mins;
}

// ==================== 日志系统实现 ====================

/**
 * @brief 获取日志级别字符串
 */
static const char* get_level_string(ad_log_level_t level) {
    switch (level) {
        case AD_LOG_LEVEL_NONE:    return "NONE";
        case AD_LOG_LEVEL_ERROR:   return "ERROR";
        case AD_LOG_LEVEL_WARN:    return "WARNING";
        case AD_LOG_LEVEL_INFO:    return "INFO";
        case AD_LOG_LEVEL_DEBUG:   return "DEBUG";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief 获取时间戳字符串（HH:MM:SS格式）
 */
static void get_timestamp_string(char* buffer, size_t size) {
    uint64_t ms = system_tick_get_ms();
    uint64_t total_seconds = ms / 1000;
    uint32_t hours = (total_seconds / 3600) % 24;
    uint32_t minutes = (total_seconds / 60) % 60;
    uint32_t seconds = total_seconds % 60;
    snprintf(buffer, size, "%02u:%02u:%02u", hours, minutes, seconds);
}

/**
 * @brief 日志输出函数
 */
void ad_log_write(ad_log_level_t level, 
                  const char *tag, 
                  const char *format, 
                  ...) {
    // 获取时间戳
    char timestamp[16];
    get_timestamp_string(timestamp, sizeof(timestamp));
    
    char message[AD_LOG_MAX_MESSAGE_LEN];
    int offset = 0;
    
    // 格式化日志前缀：[HH:MM:SS][LEVEL]tag: 
    offset = snprintf(message, sizeof(message), "[%s][%s]%s: ",
                      timestamp,
                      get_level_string(level),
                      tag);
    
    if (offset < 0 || offset >= (int)sizeof(message)) {
        return;  // 格式化失败
    }
    
    // 格式化日志内容
    va_list args;
    va_start(args, format);
    int content_len = vsnprintf(message + offset, sizeof(message) - (size_t)offset,
                                format, args);
    va_end(args);
    
    if (content_len < 0) {
        return;  // 格式化失败
    }
    
    // 计算总长度
    int total_len = offset + content_len;
    if (total_len >= (int)sizeof(message)) {
        total_len = sizeof(message) - 1;  // 截断
    }
    
    // 输出完整消息（usart_transport_log_output会自动添加换行）
    usart_transport_log_output(message, (uint32_t)total_len);
}
