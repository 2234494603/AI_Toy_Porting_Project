/**
 * @file app_ai.c
 * @brief AI在线模式管理实现
 */

#include "app_ai.h"
#include "ad_sdk.h"
#include "app_ctrl.h"
#include <string.h>

static const char *TAG = "app_ai";

// AI在线模式管理状态
static struct {
    bool initialized;           // 是否已初始化
    ad_ai_status_t ai_status;   // 当前AI状态
} g_ai = {
    .initialized = false,
    .ai_status = AD_AI_STATUS_IDLE,
};

// ============================ 内部函数 ============================

/**
 * @brief AI状态回调处理
 */
static void on_ai_status(ad_ai_status_t status) {
    const char *status_str[] = {
        "Idle",
        "Listening",
        "Waiting",
        "Speaking",
        "Session Starting"
    };
    
    if (status <= AD_AI_STATUS_SESSION_STARTING) {
        AD_LOGI(TAG, "[Callback] AI status changed: %s", status_str[status]);
    } else {
        AD_LOGI(TAG, "[Callback] AI status changed: Unknown(%d)", status);
    }
    
    // 保存AI状态
    g_ai.ai_status = status;
    
    // 通知应用控制模块在线模式状态变化
    if (status != AD_AI_STATUS_IDLE) {
        // AI激活（非空闲状态）
        app_ctrl_on_online_mode_active();
    } else {
        // AI空闲
        app_ctrl_on_online_mode_idle();
    }
}

// ============================ 公共接口实现 ============================

int app_ai_init(void) {
    if (g_ai.initialized) {
        AD_LOGW(TAG, "AI online mode manager already initialized");
        return 0;
    }
    
    AD_LOGI(TAG, "Initializing AI online mode manager");
    
    // 注册AI状态回调
    int ret = ad_sdk_register_ai_status_callback(on_ai_status);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to register AI status callback: %d", ret);
        return -1;
    }
    
    g_ai.initialized = true;
    
    AD_LOGI(TAG, "AI online mode manager initialized successfully");
    return 0;
}

void app_ai_deinit(void) {
    if (!g_ai.initialized) {
        AD_LOGW(TAG, "AI online mode manager not initialized");
        return;
    }
    
    AD_LOGI(TAG, "Deinitializing AI online mode manager");
    
    g_ai.initialized = false;
    g_ai.ai_status = AD_AI_STATUS_IDLE;
    
    AD_LOGI(TAG, "AI online mode manager deinitialized");
}

int app_ai_wake_up(void) {
    if (!g_ai.initialized) {
        AD_LOGE(TAG, "AI online mode manager not initialized");
        return -1;
    }
    
    AD_LOGI(TAG, "Waking up AI (starting AI session)");
    
    int ret = ad_sdk_ai_start();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to start AI: %d", ret);
        return ret;
    }
    
    AD_LOGI(TAG, "AI wake up command sent successfully");
    return 0;
}

int app_ai_interrupt(void) {
    if (!g_ai.initialized) {
        AD_LOGE(TAG, "AI online mode manager not initialized");
        return -1;
    }
    
    // 检查AI是否正在说话
    if (g_ai.ai_status != AD_AI_STATUS_SPEAK) {
        AD_LOGW(TAG, "AI is not speaking (status=%d), no need to interrupt", g_ai.ai_status);
        return 0;
    }
    
    AD_LOGI(TAG, "Interrupting AI");
    
    int ret = ad_sdk_ai_interrupt();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to interrupt AI: %d", ret);
        return ret;
    }
    
    AD_LOGI(TAG, "AI interrupt command sent successfully");
    return 0;
}

ad_ai_status_t app_ai_get_status(void) {
    return g_ai.ai_status;
}

