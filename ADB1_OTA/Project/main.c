/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ad_sdk.h"
#include "app_iot.h"
#include "usart_transport.h"
#include "soft_timer.h"
#include "app_ctrl.h"
#include "app_modes.h"
#include "app_ai.h"
#include "app_ota.h"
#include "ota_layout.h"
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// SDK 日志TAG
static const char* TAG = "main";

// LED 闪烁计数器（已不使用，改用软件定时器）
// static uint32_t led_counter = 0;

// 按键状态结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    const char* name;
    uint8_t last_state;         // 上一次的状态 (0=按下, 1=释放)
    uint8_t current_state;      // 当前状态
    uint32_t debounce_time;     // 消抖时间戳
    uint32_t last_release_time; // 上次释放时间
    uint8_t click_count;        // 点击计数
    uint8_t waiting_for_double; // 等待双击标志
} KeyState_t;

// 按键状态数组
static KeyState_t key_states[3];

#define KEY_DEBOUNCE_MS 50       // 消抖时间50ms
#define DOUBLE_CLICK_INTERVAL 300 // 双击时间间隔300ms
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief 调试信息定时器回调（3000ms）
 */
static void Debug_Timer_Callback(void) {
    // AD_LOGI("TIMER", "Soft timer test: 3 seconds elapsed");
}

// ==================== 按键处理函数 ====================

/**
 * @brief 处理KEY1单击事件（唤醒或打断AI）
 */
static void handle_key1_click(void) {
    // 获取当前AI状态和模式
    uint8_t current_mode = app_modes_get_current_mode();
    ad_ai_status_t ai_status = app_ai_get_status();
    
    // 如果当前在AI模式，则根据AI状态决定操作
    if (current_mode == 1) {  // APP_MODE_AI = 1
        // 先尝试打断AI（如果AI正在说话）
        if (ai_status == AD_AI_STATUS_SPEAK) {
          int ret = ad_sdk_ai_interrupt();
          if (ret == 0) {
              AD_LOGI("KEY", "KEY1 clicked: AI interrupted");
              return;
          }
        }
        // 如果AI正在监听，结束监听
        else if (ai_status == AD_AI_STATUS_LISTEN) {
          int ret = ad_sdk_ai_finish_listen();
          if (ret == 0) {
              AD_LOGI("KEY", "KEY1 clicked: AI finish listen");
              return;
          }
        }
        // 如果AI处于空闲状态，重新唤醒AI
        else if (ai_status == AD_AI_STATUS_IDLE) {
          int ret = ad_sdk_ai_start();
          if (ret == 0) {
              AD_LOGI("KEY", "KEY1 clicked: AI wake up (from idle in AI mode)");
              return;
          } else {
              AD_LOGE("KEY", "KEY1 clicked: Failed to wake up AI from idle: %d", ret);
          }
        }
    } else {
        // 不在AI模式，唤醒AI（会自动切换到AI模式）
        int ret = app_modes_wake_up_ai();
        if (ret == 0) {
            AD_LOGI("KEY", "KEY1 clicked: AI wake up triggered (mode switch)");
        } else {
            AD_LOGE("KEY", "KEY1 clicked: Failed to wake up AI: %d", ret);
        }
    }
}

/**
 * @brief 处理按键扫描、单击和双击检测
 */
static void process_keys(void) {
    uint32_t current_tick = HAL_GetTick();
    
    for (uint8_t i = 0; i < 3; i++) {
        KeyState_t* key = &key_states[i];
        
        // 读取当前引脚状态 (0=按下, 1=释放，因为是上拉输入)
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(key->port, key->pin);
        
        // �k��查状态是否变化
        if (pin_state != key->current_state) {
            // 状态发生变化，检查是否超过消抖时间
            if ((current_tick - key->debounce_time) >= KEY_DEBOUNCE_MS) {
                // 更新当前状态
                key->current_state = pin_state;
                key->debounce_time = current_tick;
                
                // 检测按键释放事件（单击/双击判断在释放时进行）
                if (key->current_state == GPIO_PIN_SET && key->last_state == GPIO_PIN_RESET) {
                    // 按键释放事件 (从低到高)
                    key->last_state = GPIO_PIN_SET;
                    
                    // 检查是否在双击时间窗口内
                    uint32_t time_since_last_release = current_tick - key->last_release_time;
                    
                    if (key->waiting_for_double && time_since_last_release <= DOUBLE_CLICK_INTERVAL) {
                        // 这是第二次点击，触发双击事件
                        AD_LOGI("KEY", "%s double-clicked", key->name);
                        key->waiting_for_double = 0;
                        key->click_count = 0;
                    } else {
                        // 这是第一次点击，等待可能的第二次点击
                        key->click_count = 1;
                        key->waiting_for_double = 1;
                        key->last_release_time = current_tick;
                    }
                }
                // 检测按键按下事件
                else if (key->current_state == GPIO_PIN_RESET && key->last_state == GPIO_PIN_SET) {
                    key->last_state = GPIO_PIN_RESET;
                }
            }
        }
        
        // 检查单击超时（等待双击超时）
        if (key->waiting_for_double) {
            uint32_t time_since_last_release = current_tick - key->last_release_time;
            if (time_since_last_release > DOUBLE_CLICK_INTERVAL) {
                // 超时，确认为单击
                AD_LOGI("KEY", "%s single-clicked", key->name);
                
                // 如果是KEY1按下，则唤醒或打断AI对话
                if (i == 0) {
                    handle_key1_click();
                }
                // 如果是KEY2按下，则循环调节音量（2档：50% <-> 100%）
                else if (i == 1) {
                    // 获取当前音量
                    uint8_t current_volume = ad_sdk_get_speaker_volume();
                    uint8_t new_volume;
                    
                    // 2档循环：50% <-> 100%
                    if (current_volume <= 75) {
                        new_volume = 100;  // 当前50%档，切换到100%
                    } else {
                        new_volume = 50;   // 当前100%档，切换到50%
                    }
                    
                    // 设置新音量到模组
                    int ret = ad_sdk_set_speaker_volume(new_volume);
                    if (ret == AD_SDK_SUCCESS) {
                        AD_LOGI("KEY", "KEY2 clicked: volume set to %d%%", new_volume);
                        // 同步本地状态并上报到云端
                        app_iot_set_and_report_speaker_volume(new_volume);
                    } else {
                        AD_LOGE("KEY", "KEY2 clicked: failed to set volume: %d", ret);
                    }
                }
                // 如果是KEY3按下，则重置模组
                else if (i == 2) {
                    AD_LOGI("KEY", "KEY3 single-clicked, resetting module");
                    int ret = ad_sdk_factory_reset_module();
                    if (ret == AD_SDK_SUCCESS) {
                        AD_LOGI("KEY", "Factory reset command sent successfully");
                    } else {
                        AD_LOGE("KEY", "Failed to send factory reset command: %d", ret);
                    }
                }
                
                key->waiting_for_double = 0;
                key->click_count = 0;
            }
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  SCB->VTOR = OTA_APP_BASE_ADDRESS;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  // ========== 初始化按键状态 ==========
  // KEY1 - PB3
  key_states[0].port = GPIOB;
  key_states[0].pin = GPIO_PIN_3;
  key_states[0].name = "KEY1";
  key_states[0].last_state = 1;
  key_states[0].current_state = 1;
  key_states[0].debounce_time = 0;
  key_states[0].last_release_time = 0;
  key_states[0].click_count = 0;
  key_states[0].waiting_for_double = 0;
  
  // KEY2 - PB4
  key_states[1].port = GPIOB;
  key_states[1].pin = GPIO_PIN_4;
  key_states[1].name = "KEY2";
  key_states[1].last_state = 1;
  key_states[1].current_state = 1;
  key_states[1].debounce_time = 0;
  key_states[1].last_release_time = 0;
  key_states[1].click_count = 0;
  key_states[1].waiting_for_double = 0;
  
  // KEY3 - PB5
  key_states[2].port = GPIOB;
  key_states[2].pin = GPIO_PIN_5;
  key_states[2].name = "KEY3";
  key_states[2].last_state = 1;
  key_states[2].current_state = 1;
  key_states[2].debounce_time = 0;
  key_states[2].last_release_time = 0;
  key_states[2].click_count = 0;
  key_states[2].waiting_for_double = 0;

  // ========== 测试 UART2 输出 ==========
  static uint8_t test_msg[] = "UART2 Test: Hello World!\r\n";
  HAL_UART_Transmit(&huart2, test_msg, (uint16_t)(sizeof(test_msg) - 1U), HAL_MAX_DELAY);
  HAL_Delay(100);
  
  // ========== SDK 初始化 ==========
  AD_LOGI(TAG, "MCU SDK starting...");
  
  // 配置SDK
  ad_sdk_config_t config = {
      // 产品信息（使用 config.h 中定义的常量）
      .mcu_pid = MCU_SDK_PID,
      .mcu_firmware_id = MCU_SDK_FIRMWARE_ID,
      .mcu_firmware_version = MCU_SDK_FIRMWARE_VERSION,
      
      // 模组LED配置（不启用LED）
      .module_working_led_pin = MCU_SDK_MODULE_WORKING_LED_PIN,
      .module_working_led_active_high = MCU_SDK_MODULE_WORKING_LED_ACTIVE_HIGH,
      .module_network_led_pin = MCU_SDK_MODULE_NETWORK_LED_PIN,
      .module_network_led_active_high = MCU_SDK_MODULE_NETWORK_LED_ACTIVE_HIGH,
      
      // 音效配置
      .sound_enable = MCU_SDK_SOUND_ENABLE,
      .system_sound_ids = NULL,    // 启用所有系统音效
      .system_sound_count = 0,
      // 配置3个自定义音效：一号灯、二号灯、三号灯
      .custom_sound_ids = MCU_SDK_CUSTOM_SOUND_IDS,
      .custom_sound_count = MCU_SDK_CUSTOM_SOUND_COUNT,
  };
  
  // 初始化SDK
  int ret = ad_sdk_init(&config);
  if (ret != AD_SDK_SUCCESS) {
      AD_LOGE(TAG, "Failed to initialize SDK: %d", ret);
      return EXIT_FAILURE;
  }
  
  AD_LOGI(TAG, "MCU SDK initialized successfully");
  
  // 初始化在线模式管理模块
  ret = app_modes_init();
  if (ret != 0) {
      AD_LOGE(TAG, "Failed to initialize online modes: %d", ret);
      ad_sdk_deinit();
      return EXIT_FAILURE;
  }
  
  // 初始化应用控制模块
  ret = app_ctrl_init();
  if (ret != 0) {
      AD_LOGE(TAG, "Failed to initialize app control: %d", ret);
      app_modes_deinit();
      ad_sdk_deinit();
      return EXIT_FAILURE;
  }
  
  // 初始化IoT业务模块（注册数据点、命令、音量等回调）
  ret = app_iot_init();
  if (ret != 0) {
      AD_LOGE(TAG, "Failed to initialize IoT business module: %d", ret);
      app_ctrl_deinit();
      app_modes_deinit();
      ad_sdk_deinit();
      return EXIT_FAILURE;
  }

  ret = app_ota_init();
  if (ret != 0) {
      AD_LOGE(TAG, "Failed to initialize MCU OTA: %d", ret);
      return EXIT_FAILURE;
  }
  
  AD_LOGI(TAG, "MCU SDK initialized successfully");
  AD_LOGI(TAG, "SDK is running");
  
  // ========== 初始化软件定时器 ==========
  SoftTimer_Init();
  AD_LOGI(TAG, "Soft timer module initialized");
  
  // LED控制已移至 app_iot.c 中的 led_switch 数据点控制
  // 创建LED控制定时器（同频率闪烁：500ms）- 已禁用
  // SoftTimerHandle_t led1_timer = SoftTimer_Create(500, LED1_Timer_Callback, SOFT_TIMER_MODE_PERIODIC);
  // SoftTimerHandle_t led2_timer = SoftTimer_Create(500, LED2_Timer_Callback, SOFT_TIMER_MODE_PERIODIC);
  // SoftTimerHandle_t led3_timer = SoftTimer_Create(500, LED3_Timer_Callback, SOFT_TIMER_MODE_PERIODIC);
  
  // 创建调试定时器（3秒周期）
  SoftTimerHandle_t debug_timer = SoftTimer_Create(3000, Debug_Timer_Callback, SOFT_TIMER_MODE_PERIODIC);
  
  if (debug_timer < 0) {
      AD_LOGE(TAG, "Failed to create soft timers");
      Error_Handler();
  }
  
  // 启动定时器
  // SoftTimer_Start(led1_timer);  // LED控制已禁用
  // SoftTimer_Start(led2_timer);  // LED控制已禁用
  // SoftTimer_Start(led3_timer);  // LED控制已禁用
  SoftTimer_Start(debug_timer);
  
  AD_LOGI(TAG, "Soft timers started (LED control moved to IoT module)");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 记录循环开始时间，用于保证循环周期为10ms
    uint32_t current_time = HAL_GetTick();
    
    // 处理UART接收数据（将缓冲区数据传递给SDK）
    usart_transport_process();
    
    // 调用SDK主循环处理
    ad_sdk_loop();

    app_ota_process();
    
    // 处理软件定时器
    SoftTimer_Process();
    
    // 处理按键扫描
    process_keys();
    
    uint32_t time_elapsed = HAL_GetTick() - current_time;
    if (time_elapsed < 10) {
      HAL_Delay(10 - time_elapsed);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
