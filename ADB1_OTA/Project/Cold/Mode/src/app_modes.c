/**
 * @file app_modes.c
 * @brief 在线模式管理实现
 * 
 * 负责管理AI模式和在线播放器模式的切换
 */

#include "main.h"
#include "app_modes.h"
#include "app_ai.h"
#include "app_player.h"
#include "ad_sdk.h"
#include <string.h>

static const char *TAG = "app_modes";

// 在线模式管理状态
static struct {
    bool initialized;                   // 是否已初始化
    uint8_t current_mode;              // 当前模式 (0=无, 1=AI, 3=Player)
    // 音效播放状态
    bool waiting_for_sound_complete;   // 是否正在等待音效播放完成
    uint8_t pending_mode;              // 等待进入的模式
} g_modes = {
    .initialized = false,
    .current_mode = 0,
    .waiting_for_sound_complete = false,
    .pending_mode = 0,
};

// ============================ 内部函数 ============================

// 前向声明
static int enter_mode_after_sound(uint8_t mode);
static void on_mode_sound_complete(ad_sound_play_result_t result, void* user_data);

/**
 * @brief 音效播放完成回调函数
 * @param result 播放结果
 * @param user_data 用户数据（模式ID）
 */
static void on_mode_sound_complete(ad_sound_play_result_t result, void* user_data) {
    uint8_t mode = (uint8_t)(uintptr_t)user_data;
    
    AD_LOGI(TAG, "Mode sound complete, result=%d, mode=%d", result, mode);
    
    if (!g_modes.waiting_for_sound_complete) {
        AD_LOGW(TAG, "Unexpected sound complete callback");
        return;
    }
    
    g_modes.waiting_for_sound_complete = false;
    
    if (result == AD_SOUND_PLAY_COMPLETE) {
        // 音效播放成功完成，现在进入对应模式
        AD_LOGI(TAG, "Mode sound played successfully, entering mode %d", mode);
        enter_mode_after_sound(mode);
    } else {
        // 音效播放失败或被中断，直接进入模式
        AD_LOGW(TAG, "Mode sound play failed/interrupted (result=%d), entering mode %d anyway", result, mode);
        enter_mode_after_sound(mode);
    }
}

/**
 * @brief 音效播放完成后实际进入模式
 * @param mode 要进入的模式
 * @return 0 成功, -1 失败
 */
static int enter_mode_after_sound(uint8_t mode) {
    int ret = 0;
    
    switch (mode) {
        case APP_MODE_AI:
            AD_LOGI(TAG, "Starting AI mode after sound");
            ret = ad_sdk_ai_start();
            if (ret == 0) {
                g_modes.current_mode = APP_MODE_AI;
            } else {
                AD_LOGE(TAG, "Failed to start AI mode: %d", ret);
            }
            break;
            
        case APP_MODE_PLAYER:
            AD_LOGI(TAG, "Starting player mode after sound");
            ret = ad_sdk_player_start();
            if (ret == 0) {
                g_modes.current_mode = APP_MODE_PLAYER;
            } else {
                AD_LOGE(TAG, "Failed to start player mode: %d", ret);
            }
            break;
            
        default:
            AD_LOGE(TAG, "Unknown online mode: %d", mode);
            ret = -1;
            break;
    }
    
    return ret;
}

/**
 * @brief 退出当前在线模式
 */
static void exit_current_mode(void) {
    if (g_modes.current_mode == APP_MODE_AI) {
        AD_LOGI(TAG, "Exiting AI mode");
        ad_sdk_ai_end();
    } else if (g_modes.current_mode == APP_MODE_PLAYER) {
        AD_LOGI(TAG, "Exiting player mode");
        ad_sdk_player_end();
    }
    
    g_modes.current_mode = 0;
}

// ============================ 公共接口实现 ============================

int app_modes_init(void) {
    if (g_modes.initialized) {
        AD_LOGW(TAG, "Online modes manager already initialized");
        return 0;
    }
    
    AD_LOGI(TAG, "Initializing online modes manager");
    
    // 初始化AI模式管理模块
    int ret = app_ai_init();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to initialize AI mode manager");
        return ret;
    }
    
    // 初始化播放器模式管理模块
    ret = app_player_init();
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to initialize player mode manager");
        app_ai_deinit();
        return ret;
    }
    
    g_modes.initialized = true;
    g_modes.current_mode = 0;
    
    AD_LOGI(TAG, "Online modes manager initialized successfully");
    return 0;
}

void app_modes_deinit(void) {
    if (!g_modes.initialized) {
        AD_LOGW(TAG, "Online modes manager not initialized");
        return;
    }
    
    AD_LOGI(TAG, "Deinitializing online modes manager");
    
    // 退出当前模式
    exit_current_mode();
    
    // 反初始化子模块
    app_player_deinit();
    app_ai_deinit();
    
    g_modes.initialized = false;
    g_modes.current_mode = 0;
    
    AD_LOGI(TAG, "Online modes manager deinitialized");
}

int app_modes_enter_mode(uint8_t mode) {
    if (!g_modes.initialized) {
        AD_LOGE(TAG, "Online modes manager not initialized");
        return -1;
    }
    
    // 如果正在等待音效播放完成，拒绝新的模式切换请求
    if (g_modes.waiting_for_sound_complete) {
        AD_LOGW(TAG, "Already waiting for sound complete, rejecting new mode switch request");
        return -1;
    }
    
    AD_LOGI(TAG, "Entering online mode: %d", mode);
    
    // 先退出当前模式
    if (g_modes.current_mode != 0) {
        exit_current_mode();
    }
    
    // 获取对应模式的音效ID
    uint64_t sound_id = 0;
    switch (mode) {
        case APP_MODE_AI:
            sound_id = AD_CUSTOM_SOUND_ID_ENTER_ONLINE_AI_MODE;
            break;
        case APP_MODE_PLAYER:
            sound_id = AD_CUSTOM_SOUND_ID_ENTER_ONLINE_PLAYER_MODE;
            break;
        default:
            AD_LOGE(TAG, "Unknown online mode: %d", mode);
            return -1;
    }
    
    AD_LOGI(TAG, "Playing mode sound %llu before entering mode %d", sound_id, mode);
    
    // 设置等待状态
    g_modes.waiting_for_sound_complete = true;
    g_modes.pending_mode = mode;
    
    // 播放音效，音效播放完成后会调用回调函数进入模式
    int ret = ad_sdk_play_sound(sound_id, on_mode_sound_complete, (void*)(uintptr_t)mode);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to play mode sound, entering mode directly, ret=%d", ret);
        g_modes.waiting_for_sound_complete = false;
        g_modes.pending_mode = 0;
        // 音效播放失败，直接进入模式
        return enter_mode_after_sound(mode);
    }

    enter_mode_after_sound(mode);
    
    return 0; // 异步操作，返回成功
}

uint8_t app_modes_get_current_mode(void) {
    return g_modes.current_mode;
}

bool app_modes_is_active(void) {
    return g_modes.current_mode != 0;
}

int app_modes_wake_up_ai(void) {
    if (!g_modes.initialized) {
        AD_LOGE(TAG, "Online modes manager not initialized");
        return -1;
    }
    
    AD_LOGI(TAG, "KEY3 pressed: waking up AI");
    
    // 如果当前不在AI模式，先切换到AI模式
    if (g_modes.current_mode != APP_MODE_AI) {
        AD_LOGI(TAG, "Not in AI mode, entering AI mode first");
        int ret = app_modes_enter_mode(APP_MODE_AI);
        if (ret != 0) {
            AD_LOGE(TAG, "Failed to enter AI mode: %d", ret);
            return ret;
        }
    }
    
    // 唤醒AI对话
    return app_ai_wake_up();
}

int app_modes_interrupt_ai(void) {
    if (!g_modes.initialized) {
        AD_LOGE(TAG, "Online modes manager not initialized");
        return -1;
    }
    
    AD_LOGI(TAG, "KEY1 pressed: interrupting AI");
    
    // 检查是否处于AI模式
    if (g_modes.current_mode != APP_MODE_AI) {
        AD_LOGW(TAG, "Not in AI mode (current_mode=%d), cannot interrupt", g_modes.current_mode);
        return -1;
    }
    
    // 打断AI对话，如果AI没在说话会返回0（不打断）
    int ret = app_ai_interrupt();
    
    // 检查是否成功打断或者AI没在说话
    if (ret == 0) {
        // 获取AI状态，判断是否需要唤醒
        ad_ai_status_t status = app_ai_get_status();
        if (status != AD_AI_STATUS_SPEAK) {
            // AI没在说话，打断操作实际上什么都没做，返回失败让调用者唤醒AI
            return -1;
        }
    }
    
    return ret;
}

