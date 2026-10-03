#include <stdint.h>

#include <common/sys_config.h>
#include <components/log.h>

#include "app_screen.h"

#define TAG "APP_SCREEN"

#if CONFIG_SYS_CPU0 && \
    (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || \
     CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
extern void lvgl_app_init(void);
extern void lvgl_app_deinit(void);
extern uint8_t lvgl_app_init_flag;
#endif

void app_screen_startup(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_APP_SCREEN_STARTUP && \
    (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || \
     CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
    if (lvgl_app_init_flag == 1)
    {
        BK_LOGI(TAG, "LVGL already started\n");
        return;
    }

    BK_LOGI(TAG, "starting dual-screen display channel\n");
    lvgl_app_init();
#endif
}

void app_screen_shutdown(void)
{
#if CONFIG_SYS_CPU0 && \
    (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || \
     CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
    if (lvgl_app_init_flag == 0)
    {
        return;
    }

    lvgl_app_deinit();
#endif
}

bool app_screen_is_started(void)
{
#if CONFIG_SYS_CPU0 && \
    (CONFIG_DUAL_SCREEN_AVI_PLAY || CONFIG_SINGLE_SCREEN_AVI_PLAY || \
     CONFIG_SINGLE_SCREEN_FONT_DISPLAY)
    return lvgl_app_init_flag == 1;
#else
    return false;
#endif
}
