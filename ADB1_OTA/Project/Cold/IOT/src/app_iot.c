/**
 * @file app_iot.c
 * @brief 物联网业务回调实现
 * 
 * 此文件实现物联网相关的回调函数并提供统一注册接口
 * 
 * 测试背景：智能三灯控制系统
 * - Datapoint: led_switch(bool), speaker_volume(int), play_mode(enum)
 * - Command: led_flash(led_number, flash_times)
 * - 3个自定义音效：一号灯、二号灯、三号灯
 */

#include "app_iot.h"
#include "ad_sdk.h"
#include "app_modes.h"
#include "main.h"
#include "gpio.h"
#include "soft_timer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
 
 // 定义日志TAG
 static const char* TAG = "app_iot";
 
 // ==================== LED闪烁状态机 ====================
 
 // LED闪烁状态
 typedef enum {
     LED_FLASH_IDLE = 0,      // 空闲状态
     LED_FLASH_ON,            // LED点亮状态
     LED_FLASH_OFF            // LED熄灭状态
 } led_flash_state_t;
 
  // LED闪烁控制结构
  typedef struct {
      bool is_active;               // 是否激活
      uint8_t led_number;           // LED编号 (1-3)
      uint16_t led_pin;             // GPIO引脚
      uint8_t target_flash_count;   // 目标闪烁次数
      uint8_t current_flash_count;  // 当前已完成的闪烁次数
      led_flash_state_t state;      // 当前状态
      GPIO_PinState initial_state;  // 闪烁前的初始状态
      SoftTimerHandle_t timer;      // 软件定时器句柄
      uint16_t session_id;          // 命令会话ID（用于在完成后上报结果）
      bool need_report_result;      // 是否需要上报结果
  } led_flash_ctrl_t;
 
  // LED闪烁控制器数组（每个LED一个控制器）
  static led_flash_ctrl_t g_led_flash_ctrl[3] = {
      {.is_active = false, .timer = -1, .led_number = 1, .session_id = 0, .need_report_result = false},
      {.is_active = false, .timer = -1, .led_number = 2, .session_id = 0, .need_report_result = false},
      {.is_active = false, .timer = -1, .led_number = 3, .session_id = 0, .need_report_result = false}
  };
 
// LED闪烁定时器回调（500ms周期）- LED1
static void LED1_Flash_Timer_Callback(void) {
    led_flash_ctrl_t* ctrl = &g_led_flash_ctrl[0];  // LED1
    if (!ctrl->is_active) {
        return;
    }
    
    switch (ctrl->state) {
        case LED_FLASH_IDLE:
            // 不应该到这里
            break;
            
        case LED_FLASH_ON:
            // 当前是点亮状态，现在熄灭
            HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_SET);
            ctrl->state = LED_FLASH_OFF;
            ctrl->current_flash_count++;
            
            AD_LOGD(TAG, "[LED_FLASH] LED%u OFF - Flash %u/%u completed", 
                    ctrl->led_number,
                    ctrl->current_flash_count,
                    ctrl->target_flash_count);
            
            // 不要在这里立即检查完成，让熄灭状态持续完整的500ms
            break;
            
         case LED_FLASH_OFF:
             // 检查是否已完成所有闪烁（在熄灭状态持续500ms后）
             if (ctrl->current_flash_count >= ctrl->target_flash_count) {
                 // 闪烁完成，恢复初始状态
                 HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, ctrl->initial_state);
                 
                 // 停止定时器
                 SoftTimer_Stop(ctrl->timer);
                 ctrl->is_active = false;
                 ctrl->state = LED_FLASH_IDLE;
                 
                 AD_LOGI(TAG, "[LED_FLASH] LED%u flash sequence completed, restored to initial state (%s)", 
                         ctrl->led_number,
                         ctrl->initial_state == GPIO_PIN_RESET ? "ON" : "OFF");
                 
                 // 闪烁完成后上报命令执行结果
                 if (ctrl->need_report_result) {
                     ad_cmd_result_t result = {
                         .session_id = ctrl->session_id,
                         .result = 0x03  // 0x03=执行成功
                     };
                     ad_sdk_report_command_result(&result);
                     AD_LOGI(TAG, "[LED_FLASH] Reported command execution success (session_id=%u)", 
                             ctrl->session_id);
                     ctrl->need_report_result = false;
                 }
             } else {
                 // 还没完成，继续闪烁 - 从熄灭状态切换到点亮
                 HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_RESET);
                 ctrl->state = LED_FLASH_ON;
                 
                 AD_LOGD(TAG, "[LED_FLASH] LED%u ON - Starting flash %u/%u", 
                         ctrl->led_number,
                         ctrl->current_flash_count + 1,
                         ctrl->target_flash_count);
             }
             break;
    }
}

// LED闪烁定时器回调（500ms周期）- LED2
static void LED2_Flash_Timer_Callback(void) {
    led_flash_ctrl_t* ctrl = &g_led_flash_ctrl[1];  // LED2
    if (!ctrl->is_active) {
        return;
    }
    
    switch (ctrl->state) {
        case LED_FLASH_IDLE:
            break;
            
        case LED_FLASH_ON:
            HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_SET);
            ctrl->state = LED_FLASH_OFF;
            ctrl->current_flash_count++;
            AD_LOGD(TAG, "[LED_FLASH] LED%u OFF - Flash %u/%u completed", 
                    ctrl->led_number, ctrl->current_flash_count, ctrl->target_flash_count);
            break;
            
         case LED_FLASH_OFF:
             if (ctrl->current_flash_count >= ctrl->target_flash_count) {
                 HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, ctrl->initial_state);
                 SoftTimer_Stop(ctrl->timer);
                 ctrl->is_active = false;
                 ctrl->state = LED_FLASH_IDLE;
                 AD_LOGI(TAG, "[LED_FLASH] LED%u flash sequence completed, restored to initial state (%s)", 
                         ctrl->led_number, ctrl->initial_state == GPIO_PIN_RESET ? "ON" : "OFF");
                 
                if (ctrl->need_report_result) {
                    ad_cmd_result_t result = {.session_id = ctrl->session_id, .result = 0x03};
                    ad_sdk_report_command_result(&result);
                    AD_LOGI(TAG, "[LED_FLASH] Reported command execution success (session_id=%u)", ctrl->session_id);
                    ctrl->need_report_result = false;
                }
            } else {
                HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_RESET);
                ctrl->state = LED_FLASH_ON;
                AD_LOGD(TAG, "[LED_FLASH] LED%u ON - Starting flash %u/%u", 
                        ctrl->led_number, ctrl->current_flash_count + 1, ctrl->target_flash_count);
            }
            break;
    }
}

// LED闪烁定时器回调（500ms周期）- LED3
static void LED3_Flash_Timer_Callback(void) {
    led_flash_ctrl_t* ctrl = &g_led_flash_ctrl[2];  // LED3
    if (!ctrl->is_active) {
        return;
    }
    
    switch (ctrl->state) {
        case LED_FLASH_IDLE:
            break;
            
        case LED_FLASH_ON:
            HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_SET);
            ctrl->state = LED_FLASH_OFF;
            ctrl->current_flash_count++;
            AD_LOGD(TAG, "[LED_FLASH] LED%u OFF - Flash %u/%u completed", 
                    ctrl->led_number, ctrl->current_flash_count, ctrl->target_flash_count);
            break;
            
         case LED_FLASH_OFF:
             if (ctrl->current_flash_count >= ctrl->target_flash_count) {
                 HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, ctrl->initial_state);
                 SoftTimer_Stop(ctrl->timer);
                 ctrl->is_active = false;
                 ctrl->state = LED_FLASH_IDLE;
                 AD_LOGI(TAG, "[LED_FLASH] LED%u flash sequence completed, restored to initial state (%s)", 
                         ctrl->led_number, ctrl->initial_state == GPIO_PIN_RESET ? "ON" : "OFF");
                 
                 if (ctrl->need_report_result) {
                     ad_cmd_result_t result = {.session_id = ctrl->session_id, .result = 0x03};
                     ad_sdk_report_command_result(&result);
                     AD_LOGI(TAG, "[LED_FLASH] Reported command execution success (session_id=%u)", ctrl->session_id);
                     ctrl->need_report_result = false;
                 }
             } else {
                 HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, GPIO_PIN_RESET);
                 ctrl->state = LED_FLASH_ON;
                 AD_LOGD(TAG, "[LED_FLASH] LED%u ON - Starting flash %u/%u", 
                         ctrl->led_number, ctrl->current_flash_count + 1, ctrl->target_flash_count);
             }
             break;
    }
}
 
  // 启动LED闪烁
  static int start_led_flash(uint8_t led_number, uint8_t flash_times, uint16_t session_id, bool need_report) {
      // 验证LED编号
      if (led_number < 1 || led_number > 3) {
          AD_LOGE(TAG, "[LED_FLASH] Invalid LED number: %u", led_number);
          return -1;
      }
      
      int led_index = led_number - 1;  // 1->0, 2->1, 3->2
      led_flash_ctrl_t* ctrl = &g_led_flash_ctrl[led_index];
      
      // 如果该LED已有闪烁在进行，先停止并恢复初始状态
      if (ctrl->is_active && ctrl->timer >= 0) {
          SoftTimer_Stop(ctrl->timer);
          // 恢复之前的初始状态
          HAL_GPIO_WritePin(GPIOB, ctrl->led_pin, ctrl->initial_state);
          
          // 如果之前有未完成的命令需要上报，先上报失败
          if (ctrl->need_report_result) {
              ad_cmd_result_t result = {
                  .session_id = ctrl->session_id,
                  .result = 1  // 1=被中断
              };
              ad_sdk_report_command_result(&result);
              AD_LOGI(TAG, "[LED_FLASH] LED%u previous flash interrupted, reported failure", led_number);
          }
      }
     
     // 确定LED引脚
     uint16_t led_pin;
     switch (led_number) {
         case 1: led_pin = GPIO_PIN_6; break;  // LED1 - PB6
         case 2: led_pin = GPIO_PIN_7; break;  // LED2 - PB7
         case 3: led_pin = GPIO_PIN_8; break;  // LED3 - PB8
         default: 
             AD_LOGE(TAG, "[LED_FLASH] Invalid LED number: %u", led_number);
             return -1;
     }
     
     // 读取并保存LED的当前状态（闪烁前的初始状态）
     GPIO_PinState initial_state = HAL_GPIO_ReadPin(GPIOB, led_pin);
     
     AD_LOGI(TAG, "[LED_FLASH] LED%u initial state: %s", led_number,
             initial_state == GPIO_PIN_RESET ? "ON" : "OFF");
     
     // 如果定时器还未创建，创建它
     if (ctrl->timer < 0) {
         // 为每个LED使用不同的回调函数
         void (*callback)(void) = NULL;
         switch (led_number) {
             case 1: callback = LED1_Flash_Timer_Callback; break;
             case 2: callback = LED2_Flash_Timer_Callback; break;
             case 3: callback = LED3_Flash_Timer_Callback; break;
         }
         
         ctrl->timer = SoftTimer_Create(500, callback, SOFT_TIMER_MODE_PERIODIC);
         if (ctrl->timer < 0) {
             AD_LOGE(TAG, "[LED_FLASH] Failed to create LED%u flash timer", led_number);
             return -1;
         }
         AD_LOGI(TAG, "[LED_FLASH] LED%u flash timer created (handle=%d)", led_number, ctrl->timer);
     }
     
      // 设置闪烁参数
      ctrl->led_number = led_number;
      ctrl->led_pin = led_pin;
      ctrl->target_flash_count = flash_times;
      ctrl->current_flash_count = 0;
      ctrl->state = LED_FLASH_OFF;  // 初始状态为OFF，定时器第一次触发时会点亮
      ctrl->initial_state = initial_state;  // 保存初始状态
      ctrl->is_active = true;
      ctrl->session_id = session_id;  // 保存会话ID
      ctrl->need_report_result = need_report;  // 设置是否需要上报结果
     
     AD_LOGI(TAG, "[LED_FLASH] Starting LED%u flash sequence: %u times", led_number, flash_times);
     
     // 启动定时器
     int ret = SoftTimer_Start(ctrl->timer);
     if (ret != 0) {
         AD_LOGE(TAG, "[LED_FLASH] Failed to start LED%u flash timer", led_number);
         ctrl->is_active = false;
         return -1;
     }
     
     return 0;
 }
 
 // ==================== 状态定义 ====================
 
 // Datapoint ID 定义（基于云平台配置）
 #define DP_ID_LED_SWITCH     21  // bool: LED开关状态
 #define DP_ID_SPEAKER_VOLUME 1   // int: 喇叭音量 (0-100)
 #define DP_ID_PLAY_MODE      6   // enum: 玩耍模式 (1=ai, 3=player)

 #define DP_ID_DEVICE_NAME    200  // string: 设备名称
 #define DP_ID_CUSTOM_DATA    201  // bytes: 自定义数据
 // Command ID 定义（基于云平台配置）
 #define CMD_ID_LED_FLASH 1  // LED闪烁
 #define CMD_ID_NAME_CHANGE 200  // 修改设备名称
 #define CMD_ID_BYTES_CHANGE 201  // 修改自定义数据
 
 // Command 参数 ID（基于云平台配置）
 #define CMD_PARAM_LED_NUMBER  1  // enum: LED编号 (1=led1, 2=led2, 3=led3)
 #define CMD_PARAM_FLASH_TIMES 2  // int: 闪烁次数（1-5次）
 
 // 玩耍模式定义（与云平台枚举值对应）
 #define PLAY_MODE_AI      1  // AI模式
 #define PLAY_MODE_PLAYER  3  // 播放器模式
 
 // LED 状态结构
 typedef struct {
     bool is_on;              // 是否点亮
     uint8_t flash_times;     // 闪烁次数 (1-5次)
     time_t turn_off_time;    // 熄灭时间戳
     uint16_t remaining_flashes; // 剩余闪烁次数
 } led_state_t;
 
 // 全局状态
 static struct {
     bool led_switch;             // LED总开关
     int speaker_volume;          // 喇叭音量
     uint8_t play_mode;           // 玩耍模式 (1=ai, 3=player)
     led_state_t leds[3];         // 3个LED的状态 (索引0=led1, 1=led2, 2=led3)
     bool last_command_executed;  // 上一个命令是否被执行
     uint16_t last_command_id;    // 上一个命令的ID
     char device_name[33];        // 设备名称 (最大32字节 + '\0')
     uint8_t custom_data[32];     // 自定义数据 (最大32字节)
     uint16_t custom_data_len;    // 自定义数据长度
 } g_state = {
     .led_switch = true,                      // 默认开启
     .speaker_volume = 80,                    // 默认音量80
     .play_mode = PLAY_MODE_AI,               // 默认AI模式
     .device_name = "name",             // 默认设备名称
     .custom_data = {0},
     .custom_data_len = 0,
     .leds = {{false, 0, 0, 0}, {false, 0, 0, 0}, {false, 0, 0, 0}},
     .last_command_executed = false,
     .last_command_id = 0
 };
 
// ==================== 回调函数实现 ====================

/**
  * @brief 数据点下发回调
  */
 static void on_datapoint_notify(ad_dp_reason_t reason,
                                 uint16_t dp_id,
                                 ad_dp_type_t dp_type,
                                 const void *value,
                                 uint16_t value_len) {
     const char *reason_str[] = {"", "Cloud Control", "AI Control", "Flash Loaded"};
     
     AD_LOGI(TAG, "[Callback] Datapoint notify: source=%s, dp_id=%u, type=%d, len=%u\n",
            (reason <= AD_DP_REASON_FLASH_LOADED) ? reason_str[reason] : "Unknown",
            dp_id, dp_type, value_len);
     
     // 处理不同的 Datapoint
     switch (dp_id) {
        case DP_ID_LED_SWITCH:  // led_switch (bool)
            if (dp_type == AD_DP_TYPE_BOOL && value_len >= 1) {
                bool new_value = *(const uint8_t *)value != 0;
                g_state.led_switch = new_value;
                AD_LOGI(TAG, "[DP] led_switch = %s\n", new_value ? "true" : "false");
                
                // 控制所有 LED（PB6, PB7, PB8）
                // 注意：LED是低电平点亮，高电平熄灭
                GPIO_PinState led_state = new_value ? GPIO_PIN_RESET : GPIO_PIN_SET;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, led_state);  // LED1
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, led_state);  // LED2
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, led_state);  // LED3
                AD_LOGI(TAG, "[DP] LED1/LED2/LED3 turned %s\n", new_value ? "ON" : "OFF");
                
               if (reason != AD_DP_REASON_FLASH_LOADED) {
                   // 上报 DP 状态
                   ad_dp_value_t dp_value = {
                       .dp_id = DP_ID_LED_SWITCH,
                       .type = AD_DP_TYPE_BOOL,
                       .value.bool_value = new_value
                   };
                   ad_sdk_report_datapoint(&dp_value, 1);
                   AD_LOGI(TAG, "          [DP] Reported led_switch status\n");
               }
           }
           break;
             
        case DP_ID_SPEAKER_VOLUME:  // speaker_volume (int)
            if (dp_type == AD_DP_TYPE_INT && value_len >= 4) {
                int32_t new_value = *(const int32_t *)value;
                
                // 限制音量范围 0-100
                if (new_value < 0) new_value = 0;
                if (new_value > 100) new_value = 100;
                
                g_state.speaker_volume = new_value;
                AD_LOGI(TAG, "          [DP] speaker_volume = %d\n", new_value);
                
                // 调用SDK设置模组音量
                int ret = ad_sdk_set_speaker_volume((uint8_t)new_value);
                if (ret == 0) {
                    AD_LOGI(TAG, "          [DP] Set module speaker volume to %d\n", new_value);
                } else {
                    AD_LOGE(TAG, "          [DP] Failed to set module speaker volume: %d\n", ret);
                }

                if (reason != AD_DP_REASON_FLASH_LOADED) {
                    
                    // 上报 DP 状态
                    ad_dp_value_t dp_value = {
                        .dp_id = DP_ID_SPEAKER_VOLUME,
                        .type = AD_DP_TYPE_INT,
                        .value.int_value = new_value
                    };
                    ad_sdk_report_datapoint(&dp_value, 1);
                    AD_LOGI(TAG, "          [DP] Reported speaker_volume status\n");
                }
            }
            break;
             
         case DP_ID_PLAY_MODE:  // play_mode (enum)
             if (dp_type == AD_DP_TYPE_ENUM && value_len >= 2) {
                 uint16_t new_value = *(const uint16_t *)value;
                 g_state.play_mode = (uint8_t)new_value;
                 
                 const char *mode_str = (new_value == PLAY_MODE_AI) ? "ai" :
                                       (new_value == PLAY_MODE_PLAYER) ? "player" : "unknown";
                 AD_LOGI(TAG, "          [DP] play_mode = %u (%s)\n", new_value, mode_str);
                 
                if (reason != AD_DP_REASON_FLASH_LOADED) {
                    // 切换在线模式
                    if (new_value == PLAY_MODE_AI || new_value == PLAY_MODE_PLAYER) {
                        int ret = app_modes_enter_mode((uint8_t)new_value);
                        if (ret == 0) {
                            AD_LOGI(TAG, "          [DP] Successfully switched to mode: %s\n", mode_str);
                        } else {
                            AD_LOGE(TAG, "          [DP] Failed to switch mode: %d\n", ret);
                        }
                    }
                    
                    // 上报 DP 状态
                    ad_dp_value_t dp_value = {
                        .dp_id = DP_ID_PLAY_MODE,
                        .type = AD_DP_TYPE_ENUM,
                        .value.enum_value = new_value
                    };
                    ad_sdk_report_datapoint(&dp_value, 1);
                    AD_LOGI(TAG, "          [DP] Reported play_mode status\n");
                }
             }
             break;
        
        case DP_ID_DEVICE_NAME:  // device_name (string)
            // 检查 value 是否为非空，并且长度不超过最大限制
            if (dp_type == AD_DP_TYPE_STRING && value != NULL && value_len > 0 && value_len <= 32) {
                // 为确保内存安全，先清空目标缓冲区
                memset(g_state.device_name, 0, sizeof(g_state.device_name));
                memcpy(g_state.device_name, value, value_len);
                g_state.device_name[(value_len < sizeof(g_state.device_name)) ? value_len : (sizeof(g_state.device_name)-1)] = '\0';  // 保证结尾有\0，并越界保护

                AD_LOGI(TAG, "          [DP] device_name = \"%s\" (len=%u)\n",
                        g_state.device_name, value_len);

                if (reason != AD_DP_REASON_FLASH_LOADED) {
                    // 上报 DP 状态
                    ad_dp_value_t dp_value;
                    memset(&dp_value, 0, sizeof(dp_value));
                    dp_value.dp_id = DP_ID_DEVICE_NAME;
                    dp_value.type = AD_DP_TYPE_STRING;
                    dp_value.value.string_value.data = g_state.device_name;
                    dp_value.value.string_value.length = (uint16_t)strlen(g_state.device_name);
                    ad_sdk_report_datapoint(&dp_value, 1);
                    AD_LOGI(TAG, "          [DP] Reported device_name status\n");
                }
            }
            break;

        case DP_ID_CUSTOM_DATA:  // custom_data (bytes)
            // 检查类型和范围
            if (dp_type == AD_DP_TYPE_BYTES && value != NULL && value_len > 0 && value_len <= sizeof(g_state.custom_data)) {
                // 先清0再拷贝
                memset(g_state.custom_data, 0, sizeof(g_state.custom_data));
                memcpy(g_state.custom_data, value, value_len);
                g_state.custom_data_len = value_len;

                // 打印字节数据（hex格式）不使用printf
                char hex_buf[3 * sizeof(g_state.custom_data) + 1] = {0}; // 每字节两位+空格，最后一个结束符
                char *p = hex_buf;
                for (uint16_t i = 0; i < value_len; i++) {
                    // 每个字节按"%02X "格式输出到缓冲区
                    if ((p - hex_buf) < (int)(sizeof(hex_buf) - 4)) {
                        int n = snprintf(p, 4, "%02X ", g_state.custom_data[i]);
                        if (n > 0) p += n;
                    }
                }
                AD_LOGI(TAG, "          [DP] custom_data (len=%u): %s", value_len, hex_buf);

                if (reason != AD_DP_REASON_FLASH_LOADED) {
                    // 上报 DP 状态
                    ad_dp_value_t dp_value;
                    memset(&dp_value, 0, sizeof(dp_value));
                    dp_value.dp_id = DP_ID_CUSTOM_DATA;
                    dp_value.type = AD_DP_TYPE_BYTES;
                    dp_value.value.bytes_value.data = g_state.custom_data;
                    dp_value.value.bytes_value.length = g_state.custom_data_len;
                    ad_sdk_report_datapoint(&dp_value, 1);
                    AD_LOGI(TAG, "          [DP] Reported custom_data status\n");
                }
            }
            break;
             
         default:
             AD_LOGI(TAG, "          [DP] Unknown dp_id: %u\n", dp_id);
             break;
     }
 }
 
 /**
  * @brief 命令下发回调
  */
 static void on_command_notify(uint8_t reason,
                               uint16_t session_id,
                               uint16_t cmd_id,
                               const ad_command_param_t *params,
                               uint16_t param_count) {
     AD_LOGI(TAG, "[Callback] Command notify: source=%u, session_id=%u, cmd_id=%u, param_count=%u\n",
            reason, session_id, cmd_id, param_count);
     
    // 记录命令信息
    g_state.last_command_id = cmd_id;
    g_state.last_command_executed = false;
    
   // 处理不同的命令
   if (cmd_id == CMD_ID_LED_FLASH) {
        // LED闪烁命令需要检查LED总开关
        if (!g_state.led_switch) {
            AD_LOGI(TAG, "          [CMD] LED is off (led_switch=false), ignoring LED_FLASH command\n");
            
            // 上报命令执行结果（被忽略）
            ad_cmd_result_t result = {
                .session_id = session_id,
                .result = 1  // 1=失败/被忽略
            };
            ad_sdk_report_command_result(&result);
            return;
        }
        
        AD_LOGI(TAG, "========================================");
        AD_LOGI(TAG, "[CMD] Processing LED_FLASH command");
        
        // 解析参数
        uint8_t led_number = 0;
        int32_t flash_times = 0;
        bool has_led_number = false;
        bool has_flash_times = false;
        
        for (uint16_t i = 0; i < param_count; i++) {
            AD_LOGI(TAG, "     Param[%u]: param_id=%u, type=%u, len=%u",
                   i, params[i].param_id, params[i].param_type, params[i].value_len);
            
            if (params[i].param_id == CMD_PARAM_LED_NUMBER && 
                params[i].param_type == AD_DP_TYPE_ENUM && 
                params[i].value_len >= 2) {
                led_number = (uint8_t)(*(const uint16_t *)params[i].value);
                has_led_number = true;
                AD_LOGI(TAG, "          => LED number = %u", led_number);
            } else if (params[i].param_id == CMD_PARAM_FLASH_TIMES && 
                       params[i].param_type == AD_DP_TYPE_INT && 
                       params[i].value_len >= 4) {
                flash_times = *(const int32_t *)params[i].value;
                has_flash_times = true;
                AD_LOGI(TAG, "          => Flash times = %d", flash_times);
            }
        }
        
        AD_LOGI(TAG, "----------------------------------------");
        AD_LOGI(TAG, "[CMD] Parsed: LED%u flash %d times", led_number, flash_times);
        
        // 参数验证
        if (!has_led_number || !has_flash_times) {
            AD_LOGI(TAG, "[CMD] Incomplete parameters, command execution failed");
            ad_cmd_result_t result = {
                .session_id = session_id,
                .result = 2  // 参数错误
            };
            ad_sdk_report_command_result(&result);
            return;
        }
        
        if (led_number < 1 || led_number > 3) {
            AD_LOGI(TAG, "[CMD] Invalid LED number: %u (valid range: 1-3)", led_number);
            ad_cmd_result_t result = {
                .session_id = session_id,
                .result = 2  // 参数错误
            };
            ad_sdk_report_command_result(&result);
            return;
        }
        
        if (flash_times < 1 || flash_times > 5) {
            AD_LOGI(TAG, "[CMD] Invalid flash times: %d (valid range: 1-5)", flash_times);
            ad_cmd_result_t result = {
                .session_id = session_id,
                .result = 2  // 参数错误
            };
            ad_sdk_report_command_result(&result);
            return;
        }
        
        AD_LOGI(TAG, "[CMD] Parameters validated successfully");
        
        // 执行命令：LED闪烁
        int led_index = led_number - 1;  // 1->0, 2->1, 3->2
        g_state.leds[led_index].is_on = true;
        g_state.leds[led_index].flash_times = (uint8_t)flash_times;
        g_state.leds[led_index].remaining_flashes = (uint16_t)flash_times;
        g_state.leds[led_index].turn_off_time = 0;  // 暂时不使用时间戳
        
        AD_LOGI(TAG, "----------------------------------------");
        AD_LOGI(TAG, "[CMD] Executing: LED%u flash %d times", led_number, flash_times);
        
         // 使用软件定时器实现LED闪烁（500ms周期，非阻塞）
         int flash_ret = start_led_flash(led_number, (uint8_t)flash_times, session_id, true);
         if (flash_ret != 0) {
             AD_LOGE(TAG, "[CMD] Failed to start LED flash: %d", flash_ret);
             // 启动失败时立即上报错误
             ad_cmd_result_t result = {
                 .session_id = session_id,
                 .result = 6  // 6=执行失败
             };
             ad_sdk_report_command_result(&result);
         } else {
             AD_LOGI(TAG, "[CMD] LED flash timer started successfully");
             AD_LOGI(TAG, "[CMD] Command result will be reported after flash completes");
             AD_LOGI(TAG, "========================================");
         }
          
          // 播放对应的语音提示（暂时注释）
          //  AD_LOGI(TAG, "          [CMD] Preparing to play sound...\n");
          //  uint64_t sound_id = led_number;  // 使用 led_number 作为 sound_id (1=led1, 2=led2, 3=led3)
          //  uint16_t sound_session_id = session_id + 1000;  // 使用不同的 session_id 用于音效播放
          //  AD_LOGI(TAG, "          [CMD] Calling ad_sdk_sound_play...\n");
          //  int ret = ad_sdk_sound_play(sound_session_id, sound_id);
          //  if (ret == 0) {
          //      AD_LOGI(TAG, "          [CMD] Playing sound prompt: sound_id=%llu (session_id=%u)\n", sound_id, sound_session_id);
          //  } else {
          //      AD_LOGI(TAG, "          [CMD] Failed to play sound prompt: ret=%d\n", ret);
          //  }
          
          // 标记命令已执行
          AD_LOGI(TAG, "          [CMD] Marking command as executed\n");
          g_state.last_command_executed = true;
          
          // 注意：命令执行结果将在LED闪烁完成后自动上报（通过定时器回调）
          
    }
    
    else if (cmd_id == CMD_ID_NAME_CHANGE) {
        AD_LOGI(TAG, "[CMD] Processing NAME_CHANGE command");
        
        // 解析参数
        bool has_name = false;
        bool has_bytes = false;
        for (uint16_t i = 0; i < param_count; i++) {
            AD_LOGI(TAG, "     Param[%u]: param_id=%u, type=%u, len=%u",
                   i, params[i].param_id, params[i].param_type, params[i].value_len);
            
            // 处理NAME参数 (param_id=200, type=STRING)
            if (params[i].param_id == CMD_ID_NAME_CHANGE &&
                params[i].param_type == AD_DP_TYPE_STRING &&
                params[i].value_len > 0) 
            {
                char name_buf[33] = {0}; // 32字节+结束符
                uint16_t copy_len = params[i].value_len > 32 ? 32 : params[i].value_len;
                memcpy(name_buf, params[i].value, copy_len);
                name_buf[copy_len] = '\0';  // 确保字符串结束
                
                AD_LOGI(TAG, "[CMD] Parsed: New name = \"%s\"", name_buf);
                
                // 保存到全局状态
                memset(g_state.device_name, 0, sizeof(g_state.device_name));
                memcpy(g_state.device_name, name_buf, copy_len);
                g_state.device_name[copy_len] = '\0';
                
                has_name = true;
            }
            // 处理BYTES参数 (param_id=201, type=BYTES) - 如果命令200中同时包含BYTES参数
            else if (params[i].param_id == CMD_ID_BYTES_CHANGE &&
                     params[i].param_type == AD_DP_TYPE_BYTES &&
                     params[i].value_len > 0 &&
                     params[i].value_len <= sizeof(g_state.custom_data))
            {
                AD_LOGI(TAG, "[CMD] Parsed: New bytes (len=%u):", params[i].value_len);

                // 构造十六进制字符串缓冲区
                char hex_buf[3 * 32 + 1] = {0};
                char *p = hex_buf;
                const uint8_t* bytes = (const uint8_t*)params[i].value;
                for (uint16_t j = 0; j < params[i].value_len; j++) {
                    if ((p - hex_buf) < (int)(sizeof(hex_buf) - 4)) {
                        int n = snprintf(p, 4, "%02X ", bytes[j]);
                        if (n > 0) p += n;
                    }
                }
                AD_LOGI(TAG, "          [CMD] Bytes(hex): %s", hex_buf);
                
                // 保存到全局状态
                memset(g_state.custom_data, 0, sizeof(g_state.custom_data));
                memcpy(g_state.custom_data, bytes, params[i].value_len);
                g_state.custom_data_len = params[i].value_len;
                
                has_bytes = true;
            }
        }
        
        // 上报命令执行结果（只要有NAME或BYTES任一成功即可）
        ad_cmd_result_t result = {
            .session_id = session_id,
            .result = (has_name || has_bytes) ? 0x03 : 2  // 0x03=成功, 2=参数错误
        };
        ad_sdk_report_command_result(&result);
        
        // 分别打印NAME和BYTES的处理结果
        if (has_name) {
            AD_LOGI(TAG, "[CMD] NAME_CHANGE command executed successfully (name updated)");
        } else {
            AD_LOGI(TAG, "[CMD] NAME_CHANGE command failed (no valid name)");
        }
        
        // 如果命令200中同时处理了BYTES参数，也打印BYTES处理结果
        if (has_bytes) {
            AD_LOGI(TAG, "[CMD] BYTES_CHANGE command executed successfully (bytes updated)");
        }
        
        g_state.last_command_executed = (has_name || has_bytes);
    } 

     else if (cmd_id == CMD_ID_BYTES_CHANGE) {
        AD_LOGI(TAG, "[CMD] Processing BYTES_CHANGE command");
        
        // 解析参数
        bool has_bytes = false;
        for (uint16_t i = 0; i < param_count; i++) {
            AD_LOGI(TAG, "     Param[%u]: param_id=%u, type=%u, len=%u",
                   i, params[i].param_id, params[i].param_type, params[i].value_len);
            
            if (params[i].param_id == CMD_ID_BYTES_CHANGE &&
                params[i].param_type == AD_DP_TYPE_BYTES &&
                params[i].value_len > 0 &&
                params[i].value_len <= sizeof(g_state.custom_data))
            {
                AD_LOGI(TAG, "[CMD] Parsed: New bytes (len=%u):", params[i].value_len);

                // 构造十六进制字符串缓冲区（每字节两位+空格，最多32个字节, 最后加结束符)
                char hex_buf[3 * 32 + 1] = {0};
                char *p = hex_buf;
                const uint8_t* bytes = (const uint8_t*)params[i].value;
                for (uint16_t j = 0; j < params[i].value_len; j++) {
                    // 保证不会越界
                    if ((p - hex_buf) < (int)(sizeof(hex_buf) - 4)) {
                        int n = snprintf(p, 4, "%02X ", bytes[j]);
                        if (n > 0) p += n;
                    }
                }
                AD_LOGI(TAG, "          [CMD] Bytes(hex): %s", hex_buf);
                
                // 保存到全局状态
                memset(g_state.custom_data, 0, sizeof(g_state.custom_data));
                memcpy(g_state.custom_data, bytes, params[i].value_len);
                g_state.custom_data_len = params[i].value_len;
                
                has_bytes = true;
            }
        }
        
        // 上报命令执行结果
        ad_cmd_result_t result = {
            .session_id = session_id,
            .result = has_bytes ? 0x03 : 2  // 0x03=成功, 2=参数错误
        };
        ad_sdk_report_command_result(&result);
        AD_LOGI(TAG, "[CMD] BYTES_CHANGE command %s", has_bytes ? "executed successfully" : "failed (no valid bytes)");
        
        g_state.last_command_executed = has_bytes;
     } 
     else {
         AD_LOGI(TAG, "          [CMD] Unknown command ID: %u\n", cmd_id);
         
         // 上报未知命令
         ad_cmd_result_t result = {
             .session_id = session_id,
             .result = 4  // 未知命令
         };
         ad_sdk_report_command_result(&result);
     }
 }
 
// ==================== 公共API ====================

/**
 * @brief 初始化IoT业务模块
 */
int app_iot_init(void) {
    int ret;
    int error_count = 0;
    
    AD_LOGI(TAG, "[IoT] Initializing IoT business module...\n");
    
    // 注册数据点回调
    ret = ad_sdk_register_datapoint_notify_callback(on_datapoint_notify);
    if (ret != 0) {
        AD_LOGI(TAG, "[IoT] [FAIL] Failed to register datapoint callback: %d\n", ret);
        error_count++;
    } else {
        AD_LOGI(TAG, "[IoT] [OK] Datapoint callback registered\n");
    }
    
    // 注册命令回调
    ret = ad_sdk_register_command_notify_callback(on_command_notify);
    if (ret != 0) {
        AD_LOGI(TAG, "[IoT] [FAIL] Failed to register command callback: %d\n", ret);
        error_count++;
    } else {
        AD_LOGI(TAG, "[IoT] [OK] Command callback registered\n");
    }
    
    // 注意：AI和Player状态回调在online_ai.c和online_player.c中注册
    // 注意：网络状态回调在app_ctrl.c中注册
    
    if (error_count == 0) {
        AD_LOGI(TAG, "[IoT] IoT business module initialized successfully!\n");
        return 0;
    } else {
        AD_LOGI(TAG, "[IoT] %d callback(s) failed to register\n", error_count);
        return -1;
    }
}

/**
 * @brief 反初始化IoT业务模块
 */
void app_iot_deinit(void) {
    AD_LOGI(TAG, "[IoT] Deinitializing IoT business module\n");
    // SDK会在deinit时自动清理所有回调
}
 
 // ==================== 状态查询接口（用于测试） ====================
 
 /**
  * @brief 获取LED总开关状态
  * @return true=开启, false=关闭
  */
 bool app_iot_get_led_switch_state(void) {
     return g_state.led_switch;
 }
 
/**
 * @brief 获取喇叭音量
 * @return 音量值 (0-100)
 */
int app_iot_get_speaker_volume(void) {
    return g_state.speaker_volume;
}

/**
 * @brief 设置喇叭音量并上报到云端
 * @param volume 音量值 (0-100)
 * @return 0=成功, 其他=失败
 */
int app_iot_set_and_report_speaker_volume(uint8_t volume) {
    // 限制音量范围 0-100
    if (volume > 100) {
        volume = 100;
    }
    
    // 更新本地状态
    g_state.speaker_volume = volume;
    
    // 上报 DP 状态到云端
    ad_dp_value_t dp_value = {
        .dp_id = DP_ID_SPEAKER_VOLUME,
        .type = AD_DP_TYPE_INT,
        .value.int_value = volume
    };
    int ret = ad_sdk_report_datapoint(&dp_value, 1);
    if (ret == 0) {
        AD_LOGI(TAG, "[KEY] Reported speaker_volume = %d to cloud", volume);
    } else {
        AD_LOGE(TAG, "[KEY] Failed to report speaker_volume: %d", ret);
    }
    
    return ret;
}

/**
 * @brief 获取玩耍模式
  * @return 玩耍模式 (1=ai, 3=player)
  */
 uint8_t app_iot_get_play_mode(void) {
     return g_state.play_mode;
 }
 
 /**
  * @brief 获取指定LED的状态
  * @param led_number LED编号 (1-3)
  * @param is_on 输出：是否点亮
  * @param flash_times 输出：闪烁次数
  * @param remaining_flashes 输出：剩余闪烁次数
  * @return 0=成功, -1=LED编号无效
  */
 int app_iot_get_led_state(uint8_t led_number, bool *is_on, 
                                    uint8_t *flash_times, int *remaining_flashes) {
     if (led_number < 1 || led_number > 3) {
         return -1;  // LED编号无效
     }
     
     int led_index = led_number - 1;
     
     if (is_on) {
         *is_on = g_state.leds[led_index].is_on;
     }
     
     if (flash_times) {
         *flash_times = g_state.leds[led_index].flash_times;
     }
     
     if (remaining_flashes) {
         *remaining_flashes = (int)g_state.leds[led_index].remaining_flashes;
     }
     
     return 0;
 }
 
 /**
  * @brief 检查上一个命令是否被执行
  * @return true=已执行, false=未执行/被忽略
  */
 bool app_iot_get_last_command_executed(void) {
     return g_state.last_command_executed;
 }
 
 /**
  * @brief 获取上一个命令的ID
  * @return 命令ID
  */
 uint16_t app_iot_get_last_command_id(void) {
     return g_state.last_command_id;
 }
 
 /**
  * @brief 重置所有状态（用于测试初始化）
  */
 void app_iot_reset_state(void) {
     g_state.led_switch = true;
     g_state.speaker_volume = 80;
     g_state.play_mode = PLAY_MODE_AI;
     
     for (int i = 0; i < 3; i++) {
         g_state.leds[i].is_on = false;
         g_state.leds[i].flash_times = 0;
         g_state.leds[i].turn_off_time = 0;
         g_state.leds[i].remaining_flashes = 0;
     }
     
     g_state.last_command_executed = false;
     g_state.last_command_id = 0;
     
     AD_LOGI(TAG, "[Simulate] State reset\n");
 }
 
 /**
  * @brief 手动更新LED状态（模拟定时器到期）
  * @note 在实际应用中，这应该由定时器中断或轮询实现
  */
 void app_iot_update_led_timers(void) {
     time_t now = time(NULL);
     
     for (int i = 0; i < 3; i++) {
         if (g_state.leds[i].is_on && now >= g_state.leds[i].turn_off_time) {
             g_state.leds[i].is_on = false;
             AD_LOGI(TAG, "          [Timer] LED%d OFF (timer expired)\n", i + 1);
         }
     }
 }
