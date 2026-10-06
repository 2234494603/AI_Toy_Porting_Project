#include <common/sys_config.h>

#if CONFIG_SYS_CPU0 && CONFIG_MOTOR
#include "motor.h"
#endif

void app_motor_power_on_feedback_begin(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_MOTOR
    motor_open(PWM_MOTOR_CH_3);
#endif
}

void app_motor_power_on_feedback_end(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_MOTOR
    motor_close(PWM_MOTOR_CH_3);
#endif
}
