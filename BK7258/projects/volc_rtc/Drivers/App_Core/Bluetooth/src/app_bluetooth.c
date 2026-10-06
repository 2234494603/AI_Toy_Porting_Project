#include "app_bluetooth.h"

#include <common/sys_config.h>
#include <components/log.h>

#define TAG "app_bluetooth"

#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
#include "app_audio.h"
#include "bk_smart_config.h"
#include "bt_manager.h"
#include "hfp_hf_demo.h"

/* Volc RTC publishes this state from volc_rtc_main.c. Remembering it keeps an
 * incoming phone call from starting AI when AI was not running beforehand. */
extern bool byte_runing;

static volatile bool s_call_audio_active = false;
static bool s_resume_ai_after_call = false;

static void app_bluetooth_hfp_audio_state_changed(bool active, void *user_data)
{
    (void)user_data;

    if (active)
    {
        s_resume_ai_after_call = byte_runing;
        s_call_audio_active = true;

        if (s_resume_ai_after_call)
        {
            BK_LOGI(TAG, "HFP audio active; stop AI transport\n");
            bk_sconf_trans_stop();
        }

        if (!app_audio_suspend_for_bluetooth())
        {
            BK_LOGE(TAG, "HFP audio could not take exclusive audio ownership\n");
        }
        return;
    }

    s_call_audio_active = false;
    app_audio_resume_after_bluetooth();
    if (s_resume_ai_after_call)
    {
        s_resume_ai_after_call = false;
        BK_LOGI(TAG, "HFP audio released; restore AI transport\n");
        bk_sconf_trans_start();
    }
}
#endif

void app_bluetooth_init(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
    int ret;

    hfp_hf_demo_register_audio_state_callback(
        app_bluetooth_hfp_audio_state_changed, NULL);

    ret = bt_manager_init(1);
    if (ret != 0)
    {
        BK_LOGE(TAG, "Bluetooth manager init failed: %d\n", ret);
        return;
    }

#if CONFIG_APP_BLUETOOTH_HFP_MSBC
    ret = hfp_hf_demo_init(1);
#else
    ret = hfp_hf_demo_init(0);
#endif
    if (ret != 0)
    {
        BK_LOGE(TAG, "HFP Hands-Free init failed: %d\n", ret);
        return;
    }

    BK_LOGI(TAG, "HFP Hands-Free ready and discoverable\n");
#endif
}

void app_bluetooth_enter_pairing(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
    bk_bt_enter_pairing_mode(1);
#endif
}

void app_bluetooth_answer_call(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
    hfp_demo_answer(1);
#endif
}

void app_bluetooth_reject_or_hang_up(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
    hfp_demo_answer(0);
#endif
}

bool app_bluetooth_call_audio_active(void)
{
#if CONFIG_SYS_CPU0 && CONFIG_BT && CONFIG_HFP_HF_DEMO
    return s_call_audio_active;
#else
    return false;
#endif
}
