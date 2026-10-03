#include <common/sys_config.h>

#if CONFIG_SYS_CPU0
#include "key_app_service.h"
#endif

void app_input_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BUTTON
    bk_key_service_init();
#endif
}
