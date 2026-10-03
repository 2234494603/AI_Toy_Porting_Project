#pragma once

#include <driver/gpio_types.h>

/*
 * AIDK AI toy board (BK7258 QFN88) pin assignment.
 * Source: AIDK_AI toy development board schematic, revision V1.0.
 */

/* Default 2.4 GHz Wi-Fi network used by the product firmware. */
#define AIDK_WIFI_SSID                "Project_Chen"
#define AIDK_WIFI_PASSWORD            "2234494603"

/* VolcEngine RTC local-agent connection for the Project_Chen board. */
#if CONFIG_BK_DEV_STARTUP_AGENT
#define CONFIG_RTC_APP_ID             "6ab7750fd755df017a8abe2f"
#define CONFIG_AGENT_SERVER_HOST      "192.168.137.1:8080"
#endif

/* VolcEngine RTC SDK log path. */
#define DEFAULT_SDK_LOG_PATH          "io.volc.rtc_sdk"

/* MFRC522 NFC: UART1 on the schematic (UART_ID_1 in BK IDK). */
#define AIDK_GPIO_NFC_UART_TX        GPIO_0
#define AIDK_GPIO_NFC_UART_RX        GPIO_1
#define AIDK_GPIO_NFC_IRQ            GPIO_53
#define AIDK_GPIO_NFC_MX             GPIO_54
#define AIDK_GPIO_NFC_DTRQ           GPIO_55

/* LCD1: QSPI1 data path, shared backlight. */
#define AIDK_GPIO_LCD1_CLK           GPIO_2
#define AIDK_GPIO_LCD1_CS            GPIO_3
#define AIDK_GPIO_LCD1_DATA          GPIO_4
#define AIDK_GPIO_LCD1_DC            GPIO_5
#define AIDK_GPIO_LCD1_RESET         GPIO_45

/* LCD2: QSPI0 data path, shared backlight. */
#define AIDK_GPIO_LCD2_RESET         GPIO_6
#define AIDK_GPIO_LCD2_DC            GPIO_7
#define AIDK_GPIO_LCD2_CLK           GPIO_22
#define AIDK_GPIO_LCD2_CS            GPIO_23
#define AIDK_GPIO_LCD2_DATA          GPIO_24
#define AIDK_GPIO_LCD_BACKLIGHT      GPIO_25
#define AIDK_GPIO_LCD_TE             GPIO_44

/* Keys and vibration motor. Keys are active low. */
#define AIDK_GPIO_KEY_S1             GPIO_13
#define AIDK_GPIO_KEY_S2_POWER       GPIO_12
#define AIDK_GPIO_KEY_S3             GPIO_8
#define AIDK_GPIO_MOTOR_PWM          GPIO_9

/* Compatibility aliases used by the existing power/input modules. */
#define AIDK_GPIO_KEY1_POWER         AIDK_GPIO_KEY_S2_POWER
#define AIDK_GPIO_KEY2               AIDK_GPIO_KEY_S1
#define AIDK_GPIO_KEY3               AIDK_GPIO_KEY_S3

/* UART0 download/debug port. */
#define AIDK_GPIO_DEBUG_UART_RX      GPIO_10
#define AIDK_GPIO_DEBUG_UART_TX      GPIO_11

/* SD NAND (SDIO group 1). */
#define AIDK_GPIO_SD_CLK             GPIO_14
#define AIDK_GPIO_SD_CMD             GPIO_15
#define AIDK_GPIO_SD_D0              GPIO_16
#define AIDK_GPIO_SD_D1              GPIO_17
#define AIDK_GPIO_SD_D2              GPIO_18
#define AIDK_GPIO_SD_D3              GPIO_19

/* SC7A20H: hardware I2C0 plus interrupt. */
#define AIDK_GPIO_GSENSOR_SCL        GPIO_20
#define AIDK_GPIO_GSENSOR_SDA        GPIO_21
#define AIDK_GPIO_GSENSOR_INT        GPIO_48

/* Charging and USB power detection. */
#define AIDK_GPIO_CHARGE_FULL_DET    GPIO_26
#define AIDK_GPIO_USB_5V_DET         GPIO_51

/* GC2145 DVP camera. */
#define AIDK_GPIO_DVP_MCLK           GPIO_27
#define AIDK_GPIO_DVP_RESET          GPIO_28
#define AIDK_GPIO_DVP_PCLK           GPIO_29
#define AIDK_GPIO_DVP_HSYNC          GPIO_30
#define AIDK_GPIO_DVP_VSYNC          GPIO_31
#define AIDK_GPIO_DVP_D0             GPIO_32
#define AIDK_GPIO_DVP_D1             GPIO_33
#define AIDK_GPIO_DVP_D2             GPIO_34
#define AIDK_GPIO_DVP_D3             GPIO_35
#define AIDK_GPIO_DVP_D4             GPIO_36
#define AIDK_GPIO_DVP_D5             GPIO_37
#define AIDK_GPIO_DVP_D6             GPIO_38
#define AIDK_GPIO_DVP_D7             GPIO_39
#define AIDK_GPIO_DVP_I2C_SDA        GPIO_43
#define AIDK_GPIO_DVP_I2C_SCL        GPIO_42
#define AIDK_GPIO_DVP_POWER          GPIO_49

/* Status LEDs and touch interface. */
#define AIDK_GPIO_LED_RED            GPIO_40
#define AIDK_GPIO_LED_GREEN          GPIO_41
#define AIDK_GPIO_TOUCH_SCL          GPIO_42
#define AIDK_GPIO_TOUCH_SDA          GPIO_43
#define AIDK_GPIO_TOUCH_INT          GPIO_46
#define AIDK_GPIO_TOUCH_CS           GPIO_47

/* Audio PA and external 3.3 V rail. */
#define AIDK_GPIO_AUDIO_PA_MUTE      GPIO_50
#define AIDK_GPIO_LDO_3V3_ENABLE     GPIO_52

/*
 * Dedicated/analog pins (not regular GPIO): USB_DP/USB_DM, VBAT ADC0,
 * MICBIAS/MIC1P/MIC1N/MIC2P/MIC2N, AUDLP/AUDLN and the RF antenna pin.
 */
