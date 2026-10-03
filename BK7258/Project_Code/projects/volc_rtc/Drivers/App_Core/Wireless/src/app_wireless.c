#include <common/sys_config.h>
#include <components/log.h>
#include <os/str.h>

#define TAG "app_wireless"

#if CONFIG_SYS_CPU0
#include <common/bk_include.h>
#include <components/netif.h>
#include <modules/wifi.h>
#include "aidk_board_pins.h"
#include "bk_factory_config.h"

#if CONFIG_BK_SMART_CONFIG
#include "bk_genie_comm.h"
#include "bk_smart_config.h"
#endif

#if CONFIG_NET_PAN
#include "bluetooth_storage.h"
#endif

static const uint32_t s_user_value2 = 10;
static const uint32_t s_wifi_profile_uninstalled = 0;

#define APP_DEFAULT_WIFI_PROFILE_KEY "app_wifi_profile"
#define APP_DEFAULT_WIFI_PROFILE_VERSION 3U

#if CONFIG_NET_PAN
static const bt_user_storage_t s_bt_factory_storage = {0};
#endif

static const struct factory_config_t s_user_config[] = {
    {"user_key1", (void *)"user_value1", 11, BK_FALSE, 0},
    {"user_key2", (void *)&s_user_value2, 4, BK_TRUE, 4},
    {APP_DEFAULT_WIFI_PROFILE_KEY, (void *)&s_wifi_profile_uninstalled,
     sizeof(s_wifi_profile_uninstalled), BK_TRUE, sizeof(s_wifi_profile_uninstalled)},
#if CONFIG_NET_PAN
    {BT_STORAGE_KEY, (void *)&s_bt_factory_storage, sizeof(s_bt_factory_storage),
     BK_TRUE, sizeof(s_bt_factory_storage)},
#endif
};

#if CONFIG_BK_SMART_CONFIG
static bk_err_t app_wireless_seed_default_wifi(void)
{
    wifi_sta_config_t sta_config = {0};
    uint32_t installed_version = 0;
    bk_err_t ret;

    bk_config_read(APP_DEFAULT_WIFI_PROFILE_KEY, &installed_version,
                   sizeof(installed_version));
    if ((installed_version == APP_DEFAULT_WIFI_PROFILE_VERSION) &&
        bk_sconf_is_wifi_sta_configured()) {
        return BK_OK;
    }

    os_strlcpy(sta_config.ssid, AIDK_WIFI_SSID,
               sizeof(sta_config.ssid));
    os_strlcpy(sta_config.password, AIDK_WIFI_PASSWORD,
               sizeof(sta_config.password));

    BK_LOGW(TAG, "install default Wi-Fi SSID:%s\r\n", sta_config.ssid);
    ret = demo_save_network_auto_restart_info(NETIF_IF_STA, &sta_config);
    if (ret != BK_OK) {
        return ret;
    }

    return bk_config_write(APP_DEFAULT_WIFI_PROFILE_KEY,
                           &((const uint32_t){APP_DEFAULT_WIFI_PROFILE_VERSION}),
                           sizeof(installed_version));
}
#endif
#endif

void app_wireless_factory_init(void)
{
#if CONFIG_SYS_CPU0
    bk_regist_factory_user_config(s_user_config,
                                  sizeof(s_user_config) / sizeof(s_user_config[0]));
    bk_factory_init();
#endif
}

void app_wireless_services_init(void)
{
#if CONFIG_SYS_CPU0
#if CONFIG_BK_BOARDING_SERVICE
    bk_genie_core_init();
#endif
#if CONFIG_BK_SMART_CONFIG
    if (app_wireless_seed_default_wifi() != BK_OK) {
        BK_LOGE(TAG, "default Wi-Fi configuration failed; use BLE provisioning\r\n");
    }
    BK_LOGW(TAG, "Wi-Fi service start (%s)\r\n",
            bk_sconf_is_wifi_sta_configured() ? "saved network" : "BLE provisioning");
    if (bk_smart_config_init() != BK_OK) {
        BK_LOGE(TAG, "Wi-Fi service initialization failed\r\n");
    }
#endif
#endif
}
