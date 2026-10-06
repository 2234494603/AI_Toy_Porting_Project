/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum {
    APP_CTRL_STATE_INIT = 0,          /* 初始化，等待模组状态 */
    APP_CTRL_STATE_PAIRING = 1,       /* 配网中 */
    APP_CTRL_STATE_OFFLINE = 2,       /* 已配网，网络未连接 */
    APP_CTRL_STATE_ONLINE = 3,        /* 网络已连接 */
    APP_CTRL_STATE_AWAKE = 4          /* 在线模式激活 */
} app_ctrl_state_t;

typedef enum {
    APP_CTRL_EVENT_ON_MODULE_STATUS = 0,
    APP_CTRL_EVENT_ON_NETWORK_CONNECTED = 1,
    APP_CTRL_EVENT_ON_NETWORK_DISCONNECTED = 2,
    APP_CTRL_EVENT_ON_ONLINE_MODE_ACTIVE = 3,
    APP_CTRL_EVENT_ON_ONLINE_MODE_IDLE = 4,
} app_ctrl_event_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define MCU_SDK_PID                 "pdtlNSTV"
#define MCU_SDK_FIRMWARE_ID         "fWIEGe2A"
#define MCU_SDK_FIRMWARE_VERSION    "2.0.0"

#define MCU_SDK_CUSTOM_SOUND_COUNT  2
#define AD_CUSTOM_SOUND_ID_ENTER_ONLINE_AI_MODE     86743860
#define AD_CUSTOM_SOUND_ID_ENTER_ONLINE_PLAYER_MODE 95452813

static const uint64_t MCU_SDK_CUSTOM_SOUND_IDS[MCU_SDK_CUSTOM_SOUND_COUNT] = {
    AD_CUSTOM_SOUND_ID_ENTER_ONLINE_AI_MODE,
    AD_CUSTOM_SOUND_ID_ENTER_ONLINE_PLAYER_MODE
};

#define MCU_SDK_MODULE_WORKING_LED_PIN         12
#define MCU_SDK_MODULE_WORKING_LED_ACTIVE_HIGH 0
#define MCU_SDK_MODULE_NETWORK_LED_PIN         13
#define MCU_SDK_MODULE_NETWORK_LED_ACTIVE_HIGH 0

#define MCU_SDK_SOUND_ENABLE            true
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
