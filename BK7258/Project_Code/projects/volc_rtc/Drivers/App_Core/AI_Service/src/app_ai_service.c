#include <common/sys_config.h>

#if CONFIG_SYS_CPU0
#include "network_transfer.h"
#endif

void app_ai_service_init(void)
{
#if CONFIG_SYS_CPU0
    network_transfer_init();
#endif
}
