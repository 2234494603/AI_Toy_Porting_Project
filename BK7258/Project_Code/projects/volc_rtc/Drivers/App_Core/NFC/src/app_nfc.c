#include <common/sys_config.h>

void app_nfc_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_NFC_ENABLE
    extern void nfc_get_id_task(void);
    nfc_get_id_task();
#endif
}
