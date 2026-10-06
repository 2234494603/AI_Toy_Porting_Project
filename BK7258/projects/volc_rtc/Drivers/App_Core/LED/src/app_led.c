#include <common/sys_config.h>

#if CONFIG_SYS_CPU0 && CONFIG_LED_BLINK
#include "led_blink.h"
#endif

void app_led_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_LED_BLINK
    led_driver_init();
    led_app_set(LED_ON_GREEN, LED_LAST_FOREVER);
#endif
}
