#include <common/sys_config.h>

void app_storage_init(void)
{
#if CONFIG_USBD_MSC
    extern void msc_storage_init(void);
    msc_storage_init();
#endif
}
