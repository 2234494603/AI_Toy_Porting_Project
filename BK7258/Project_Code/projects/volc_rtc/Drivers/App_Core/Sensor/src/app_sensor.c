#include <common/sys_config.h>
#include <os/os.h>

void app_sensor_prepare_sleep(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_GSENSOR_ENABLE
    extern int gsensor_enter_sleep_config(void);
    gsensor_enter_sleep_config();
    rtos_delay_milliseconds(10);
#endif
}
