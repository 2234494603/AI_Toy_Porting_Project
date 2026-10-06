#include "bk_private/bk_init.h"
#include <common/sys_config.h>
#include <components/system.h>
#include <os/os.h>

#include "app_core.h"
#include "app_freertos_startup.h"
#include "app_power.h"

extern void rtos_set_user_app_entry(beken_thread_function_t entry);


int main(void)
{
    if (bk_misc_get_reset_reason() != RESET_SOURCE_FORCE_DEEPSLEEP)
    {
#if CONFIG_SYS_CPU0
        rtos_set_user_app_entry((beken_thread_function_t)app_freertos_startup);
#endif
        bk_init();
        app_core_boot_init();
    }
    else
    {
#if CONFIG_SYS_CPU0
        bk_init();
        app_power_enter_deepsleep();
#endif
    }

    return 0;
}
