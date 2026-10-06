/**
 * @file app_player.c
 * @brief 在线播放器模式管理实现
 */

#include "app_player.h"
#include "app_ctrl.h"
#include "ad_sdk.h"
#include <string.h>

static const char *TAG = "app_player";

// 在线播放器模式管理状态
static struct {
    bool initialized;    // 是否已初始化
} g_player = {
    .initialized = false,
};

// ============================ 内部函数 ============================

/**
 * @brief 播放器状态回调处理
 */
static void on_player_status(const char *audio_id,
                             uint32_t progress_ms,
                             uint32_t duration_ms,
                             ad_player_status_t status) {
    const char *status_str[] = {
        "Stopped",   // 0x00
        "Playing",   // 0x01
        "Paused"     // 0x02
    };
    
    const char *status_name = (status <= AD_PLAYER_STATUS_PAUSED) ? 
                               status_str[status] : "Unknown";
    
    AD_LOGI(TAG, "[Callback] Player status: audio_id=%.16s, progress=%u ms, duration=%u ms, status=%s",
           audio_id, progress_ms, duration_ms, status_name);
    
    // 通知应用控制模块在线模式状态变化
    if (status == AD_PLAYER_STATUS_PLAYING || status == AD_PLAYER_STATUS_PAUSED) {
        // 播放器激活（播放中或暂停）
        app_ctrl_on_online_mode_active();
    } else {
        // 播放器空闲（停止状态）
        app_ctrl_on_online_mode_idle();
    }
}

// ============================ 公共接口实现 ============================

int app_player_init(void) {
    if (g_player.initialized) {
        AD_LOGW(TAG, "Player online mode manager already initialized");
        return 0;
    }
    
    AD_LOGI(TAG, "Initializing player online mode manager");
    
    // 注册播放器状态回调
    int ret = ad_sdk_register_player_status_callback(on_player_status);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to register player status callback: %d", ret);
        return -1;
    }
    
    g_player.initialized = true;
    
    AD_LOGI(TAG, "Player online mode manager initialized successfully");
    return 0;
}

void app_player_deinit(void) {
    if (!g_player.initialized) {
        AD_LOGW(TAG, "Player online mode manager not initialized");
        return;
    }
    
    AD_LOGI(TAG, "Deinitializing player online mode manager");
    
    g_player.initialized = false;
    
    AD_LOGI(TAG, "Player online mode manager deinitialized");
}

