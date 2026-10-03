#include <common/sys_config.h>
#include <components/log.h>
#include <driver/pwr_clk.h>
#include <modules/pm.h>
#include <os/os.h>
#include <stdbool.h>

#include "app_event.h"
#include "audio_engine.h"
#include "media_service.h"

#if CONFIG_SYS_CPU0
#include "key_app_service.h"
#endif

#define TAG "APP_AUDIO"

#if CONFIG_SYS_CPU0
static beken_mutex_t s_audio_owner_mutex = NULL;
static volatile bool s_bluetooth_audio_suspended = false;
#endif

void app_audio_background_init(void)
{
#if CONFIG_SYS_CPU0
    audio_engine_init();
#endif
}

void app_audio_service_init(void)
{
    media_service_init();
}

void app_audio_frontend_init(void)
{
#if CONFIG_SYS_CPU0
    if (s_audio_owner_mutex == NULL)
    {
        bk_err_t mutex_ret = rtos_init_mutex(&s_audio_owner_mutex);
        if (mutex_ret != BK_OK)
        {
            s_audio_owner_mutex = NULL;
            BK_LOGE(TAG, "create audio owner mutex failed: %d\n", mutex_ret);
        }
    }

    volume_init();

#if CONFIG_AUD_INTF_SUPPORT_PROMPT_TONE
    bk_err_t ret = audio_turn_on();
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "audio turn on failed: %d\n", ret);
    }
#endif
#elif CONFIG_SYS_CPU1
    audio_engine_init();
#endif
}

bool app_audio_suspend_for_bluetooth(void)
{
#if CONFIG_SYS_CPU0
    bk_err_t ret;

    app_event_set_prompt_tone_suppressed(true);
    if (s_audio_owner_mutex != NULL)
    {
        rtos_lock_mutex(&s_audio_owner_mutex);
    }

    if (s_bluetooth_audio_suspended)
    {
        if (s_audio_owner_mutex != NULL)
        {
            rtos_unlock_mutex(&s_audio_owner_mutex);
        }
        return true;
    }

    ret = audio_turn_off();
    if (ret == BK_OK)
    {
        s_bluetooth_audio_suspended = true;
        BK_LOGI(TAG, "AI audio suspended for Bluetooth call\n");
    }
    else
    {
        BK_LOGE(TAG, "suspend AI audio for Bluetooth failed: %d\n", ret);
        app_event_set_prompt_tone_suppressed(false);
    }

    if (s_audio_owner_mutex != NULL)
    {
        rtos_unlock_mutex(&s_audio_owner_mutex);
    }
    return ret == BK_OK;
#else
    return false;
#endif
}

void app_audio_resume_after_bluetooth(void)
{
#if CONFIG_SYS_CPU0
    bk_err_t ret;

    if (s_audio_owner_mutex != NULL)
    {
        rtos_lock_mutex(&s_audio_owner_mutex);
    }

    if (!s_bluetooth_audio_suspended)
    {
        if (s_audio_owner_mutex != NULL)
        {
            rtos_unlock_mutex(&s_audio_owner_mutex);
        }
        return;
    }

    ret = audio_turn_on();
    if (ret == BK_OK)
    {
        s_bluetooth_audio_suspended = false;
        BK_LOGI(TAG, "AI audio restored after Bluetooth call\n");
    }
    else
    {
        BK_LOGE(TAG, "restore AI audio after Bluetooth failed: %d\n", ret);
    }

    if (s_audio_owner_mutex != NULL)
    {
        rtos_unlock_mutex(&s_audio_owner_mutex);
    }
    app_event_set_prompt_tone_suppressed(s_bluetooth_audio_suspended);
#endif
}

void app_audio_start_coprocessor(void)
{
#if CONFIG_SYS_CPU0
    bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO,
                                    PM_POWER_MODULE_STATE_ON);
#endif
}
