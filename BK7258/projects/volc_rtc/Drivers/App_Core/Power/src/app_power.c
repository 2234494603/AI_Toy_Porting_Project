#include <common/sys_config.h>

#if CONFIG_SYS_CPU0
#include <common/bk_include.h>
#include <components/log.h>
#include <components/system.h>
#include <driver/gpio.h>
#include <modules/pm.h>
#include <os/os.h>

#include "gpio_driver.h"
#include "key_app_service.h"
#include "sys_driver.h"
#include "sys_hal.h"

#include "app_motor.h"
#include "app_power.h"
#include "app_sensor.h"
#include "aidk_board_pins.h"
#endif

#define TAG "APP_POWER"

#if CONFIG_SYS_CPU0 && CONFIG_LDO3V3_ENABLE
#ifdef CONFIG_LDO3V3_CTRL_GPIO
#define APP_LDO3V3_CTRL_GPIO CONFIG_LDO3V3_CTRL_GPIO
#else
#define APP_LDO3V3_CTRL_GPIO AIDK_GPIO_LDO_3V3_ENABLE
#endif
#endif

#if CONFIG_SYS_CPU0
static bk_err_t app_power_release_peripheral_pins(void)
{
    sys_hal_set_ana_reg18_value(0);
    sys_hal_set_ana_reg19_value(0);
    sys_hal_set_ana_reg20_value(0);
    sys_hal_set_ana_reg21_value(0);
    sys_hal_set_ana_reg27_value(0);
    sys_drv_aud_aud_en(0);
    sys_drv_aud_audbias_en(0);
    sys_drv_apll_en(0);

    gpio_dev_unmap(AIDK_GPIO_AUDIO_PA_MUTE);
    gpio_dev_unmap(AIDK_GPIO_LDO_3V3_ENABLE);
    gpio_dev_unmap(AIDK_GPIO_DEBUG_UART_RX);
    gpio_dev_unmap(AIDK_GPIO_DEBUG_UART_TX);
    gpio_dev_unmap(AIDK_GPIO_NFC_UART_TX);
    gpio_dev_unmap(AIDK_GPIO_NFC_UART_RX);
    gpio_dev_unmap(AIDK_GPIO_MOTOR_PWM);
    return BK_OK;
}

static void app_power_wait_for_long_press(void)
{
    uint32_t press_time = 0;

    do
    {
        if (bk_gpio_get_input(AIDK_GPIO_KEY1_POWER) != 0)
        {
            break;
        }

        extern void delay_ms(uint32 num);
        delay_ms(500);
        press_time += 500;

        if (bk_gpio_get_input(AIDK_GPIO_KEY1_POWER) != 0)
        {
            break;
        }
    } while (press_time < LONG_RRESS_TIMR);

    if (press_time < LONG_RRESS_TIMR)
    {
        app_power_enter_deepsleep();
    }
}
#endif

void app_power_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_LDO3V3_ENABLE
    BK_LOG_ON_ERR(gpio_dev_unmap(APP_LDO3V3_CTRL_GPIO));
    bk_gpio_disable_pull(APP_LDO3V3_CTRL_GPIO);
    bk_gpio_enable_output(APP_LDO3V3_CTRL_GPIO);
    bk_gpio_set_output_high(APP_LDO3V3_CTRL_GPIO);
#endif
}

void app_power_handle_wakeup(void)
{
#if CONFIG_SYS_CPU0
    if (bk_misc_get_reset_reason() == RESET_SOURCE_DEEPPS_GPIO &&
        bk_gpio_get_wakeup_gpio_id() == AIDK_GPIO_KEY1_POWER)
    {
        app_motor_power_on_feedback_begin();
        app_power_wait_for_long_press();
        app_motor_power_on_feedback_end();
    }
#endif
}

void app_power_battery_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BAT_MONITOR
    extern void battery_monitor_init(void);
    battery_monitor_init();
#endif
}

void app_power_enter_deepsleep(void)
{
#if CONFIG_SYS_CPU0
    app_sensor_prepare_sleep();
    BK_LOGI(TAG, "RESET_SOURCE_FORCE_DEEPSLEEP\r\n");
    bk_key_register_wakeup_source();
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_AUDP);
    bk_pm_clear_deep_sleep_modules_config(PM_POWER_MODULE_NAME_VIDP);
    app_power_release_peripheral_pins();
    bk_pm_sleep_mode_set(PM_MODE_DEEP_SLEEP);
    rtos_delay_milliseconds(10);
#endif
}
