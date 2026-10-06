#include <common/sys_config.h>
#include <modules/pm.h>

#if CONFIG_SYS_CPU0 && CONFIG_APP_EVT
#include "app_event.h"
#endif

void app_system_background_init(void)
{
#if CONFIG_SYS_CPU0
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);
#endif
}

void app_system_event_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_APP_EVT
    app_event_init();
#endif
}
