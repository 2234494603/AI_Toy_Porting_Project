#include <common/sys_config.h>

#if CONFIG_SYS_CPU0
#include <common/bk_include.h>
#include "video_engine.h"
#endif

void app_display_camera_init(void)
{
#if CONFIG_SYS_CPU0
    voide_engine_init();
#endif
}
