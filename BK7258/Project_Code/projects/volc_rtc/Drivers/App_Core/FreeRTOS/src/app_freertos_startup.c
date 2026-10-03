#include <stdbool.h>

#include <common/sys_config.h>
#include <components/log.h>

#include "app_core.h"
#include "app_freertos_startup.h"

#define TAG "APP_FREERTOS"

void app_freertos_startup(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_APP_FREERTOS_STARTUP
    static bool started = false;

    if (started)
    {
        BK_LOGW(TAG, "application task already started\n");
        return;
    }

    started = true;
    BK_LOGI(TAG, "starting application FreeRTOS task\n");
    app_core_background_init();
#endif
}
