/**
 * @file app_ctrl.c
 * @brief 应用控制模块实现
 * 
 * 管理设备的状态机，包括配网、联网、唤醒等流程
 */

#include "app_ctrl.h"
#include "ad_sdk.h"
#include "app_modes.h"
#include <string.h>

static const char *TAG = "app_ctrl";

// ============================ 状态定义 ============================

// 模块状态
static struct {
    bool initialized;                   // 模块是否已初始化
    app_ctrl_state_t current_state;     // 当前状态机状态
    ad_module_status_t module_status;   // 当前模组状态
    bool waiting_for_network;           // 是否正在等待联网
} g_ctrl = {
    .initialized = false,
    .current_state = APP_CTRL_STATE_INIT,
    .module_status = AD_MODULE_STATUS_NOT_PAIRED_INIT,
    .waiting_for_network = false,
};

// ============================ 前向声明 ============================

static void process_event(app_ctrl_event_t event);
static void enter_pairing_mode(void);
static void connect_network(void);
static void enter_ai_mode(void);
static void on_module_status(ad_module_status_t status);

// ============================ 回调函数 ============================

/**
 * @brief 模组状态回调
 */
static void on_module_status(ad_module_status_t status) {
    const char *status_str[] = {
        "Unpaired-Uninitialized",
        "Unpaired-Pairing",
        "Unpaired-Failed",
        "Paired-Disconnected",
        "Paired-Connecting",
        "Paired-Connected",
        "Paired-ResourceUpdating",
        "Paired-FirmwareUpdating"
    };
    
    if (status == AD_MODULE_STATUS_RESET) {
        AD_LOGI(TAG, "[Callback] Module reset detected");
    } else if (status <= AD_MODULE_STATUS_PAIRED_FIRMWARE_UPDATING) {
        AD_LOGI(TAG, "[Callback] Module status changed: %s", status_str[status]);
    } else {
        AD_LOGI(TAG, "[Callback] Module status changed: Unknown(%d)", status);
    }
    
    // 通知应用控制模块状态变化
    app_ctrl_on_module_status(status);
}

// ============================ 状态机处理函数 ============================

/**
 * @brief 进入配网模式
 */
static void enter_pairing_mode(void) {
    AD_LOGI(TAG, "Entering pairing mode");
    int ret = ad_sdk_start_pairing(0);  // 使用默认超时时间
    if (ret != AD_SDK_SUCCESS) {
        AD_LOGE(TAG, "Failed to start pairing: %d", ret);
    }
}

/**
 * @brief 连接网络
 */
static void connect_network(void) {
    AD_LOGI(TAG, "Connecting to network (current module status: %d)", g_ctrl.module_status);
    g_ctrl.waiting_for_network = true;
    int ret = ad_sdk_connect_network();
    if (ret != AD_SDK_SUCCESS) {
        AD_LOGE(TAG, "Failed to connect network: %d", ret);
        g_ctrl.waiting_for_network = false;
    } else {
        AD_LOGI(TAG, "Network connect command sent successfully, waiting for connection...");
    }
}

/**
 * @brief 进入AI模式
 */
static void enter_ai_mode(void) {
    AD_LOGI(TAG, "Entering AI mode");
    int ret = app_modes_enter_mode(APP_MODE_AI);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to enter AI mode: %d", ret);
    }
}

/**
 * @brief 处理事件
 */
static void process_event(app_ctrl_event_t event) {
    AD_LOGI(TAG, "process_event state=%d, event=%d", g_ctrl.current_state, event);

    switch (g_ctrl.current_state) {
        case APP_CTRL_STATE_INIT:
            if (event == APP_CTRL_EVENT_ON_MODULE_STATUS) {
                // 初始化状态收到网络状态上报
                if (g_ctrl.module_status <= AD_MODULE_STATUS_NOT_PAIRED_FAILED) {
                    // 未配网状态，进入配网模式
                    AD_LOGI(TAG, "Device not paired, entering pairing mode");
                    g_ctrl.current_state = APP_CTRL_STATE_PAIRING;
                    enter_pairing_mode();
                } else if (g_ctrl.module_status == AD_MODULE_STATUS_PAIRED_CONNECTED) {
                    // 已配网且已连接，直接进入在线状态
                    AD_LOGI(TAG, "Device already connected, entering online state");
                    g_ctrl.current_state = APP_CTRL_STATE_ONLINE;
                    // 自动进入AI模式
                    enter_ai_mode();
                } else {
                    // 已配网但未连接，进入离线状态并尝试连接
                    AD_LOGI(TAG, "Device paired but not connected, entering offline state");
                    g_ctrl.current_state = APP_CTRL_STATE_OFFLINE;
                    connect_network();
                }
            }
            break;

        case APP_CTRL_STATE_PAIRING:
            if (event == APP_CTRL_EVENT_ON_MODULE_STATUS) {
                // 配网过程中收到网络状态更新
                if (g_ctrl.module_status == AD_MODULE_STATUS_PAIRED_CONNECTED) {
                    // 配网成功并已连接
                    AD_LOGI(TAG, "Pairing succeeded and connected, state PAIRING -> ONLINE");
                    g_ctrl.current_state = APP_CTRL_STATE_ONLINE;
                    // 自动进入AI模式
                    enter_ai_mode();
                } else if (g_ctrl.module_status >= AD_MODULE_STATUS_PAIRED_DISCONNECTED) {
                    // 配网成功但未连接
                    AD_LOGI(TAG, "Pairing succeeded but not connected, state PAIRING -> OFFLINE");
                    g_ctrl.current_state = APP_CTRL_STATE_OFFLINE;
                    connect_network();
                }
            }
            break;

        case APP_CTRL_STATE_OFFLINE:
            if (event == APP_CTRL_EVENT_ON_MODULE_STATUS) {
                if (g_ctrl.module_status == AD_MODULE_STATUS_PAIRED_CONNECTED) {
                    // 网络已连接
                    AD_LOGI(TAG, "Network connected, state OFFLINE -> ONLINE");
                    g_ctrl.current_state = APP_CTRL_STATE_ONLINE;
                    bool old_waiting_for_network = g_ctrl.waiting_for_network;
                    g_ctrl.waiting_for_network = false;
                    // 如果之前在等待联网，现在自动进入AI模式
                    if (old_waiting_for_network) {
                        enter_ai_mode();
                    }
                } else if (g_ctrl.module_status == AD_MODULE_STATUS_PAIRED_DISCONNECTED) {
                    // 已配网但未连接，如果还没有在等待连接，则尝试连接
                    if (!g_ctrl.waiting_for_network) {
                        AD_LOGI(TAG, "Network disconnected, retrying connection");
                        connect_network();
                    } else {
                        AD_LOGD(TAG, "Network connection in progress, waiting...");
                    }
                } else if (g_ctrl.module_status == AD_MODULE_STATUS_PAIRED_CONNECTING) {
                    // 正在连接中，设置等待标志
                    if (!g_ctrl.waiting_for_network) {
                        AD_LOGI(TAG, "Network connecting, setting wait flag");
                        g_ctrl.waiting_for_network = true;
                    }
                } else if (g_ctrl.module_status <= AD_MODULE_STATUS_NOT_PAIRED_FAILED) {
                    // 变成未配网状态，进入配网模式
                    AD_LOGI(TAG, "Device unpaired, state OFFLINE -> PAIRING");
                    g_ctrl.current_state = APP_CTRL_STATE_PAIRING;
                    g_ctrl.waiting_for_network = false;
                    enter_pairing_mode();
                }
            }
            break;

        case APP_CTRL_STATE_ONLINE:
            if (event == APP_CTRL_EVENT_ON_MODULE_STATUS) {
                if (g_ctrl.module_status != AD_MODULE_STATUS_PAIRED_CONNECTED) {
                    // 网络断开
                    if (g_ctrl.module_status <= AD_MODULE_STATUS_NOT_PAIRED_FAILED) {
                        // 变成未配网状态，进入配网模式
                        AD_LOGI(TAG, "Device unpaired, state ONLINE -> PAIRING");
                        g_ctrl.current_state = APP_CTRL_STATE_PAIRING;
                        enter_pairing_mode();
                    } else {
                        // 网络断开，进入离线状态
                        AD_LOGI(TAG, "Network disconnected, state ONLINE -> OFFLINE");
                        g_ctrl.current_state = APP_CTRL_STATE_OFFLINE;
                        connect_network();
                    }
                }
            } else if (event == APP_CTRL_EVENT_ON_ONLINE_MODE_ACTIVE) {
                // 在线模式激活
                AD_LOGI(TAG, "Online mode active, state ONLINE -> AWAKE");
                g_ctrl.current_state = APP_CTRL_STATE_AWAKE;
            }
            break;

        case APP_CTRL_STATE_AWAKE:
            if (event == APP_CTRL_EVENT_ON_MODULE_STATUS) {
                if (g_ctrl.module_status != AD_MODULE_STATUS_PAIRED_CONNECTED) {
                    // 网络断开，退出唤醒状态
                    if (g_ctrl.module_status <= AD_MODULE_STATUS_NOT_PAIRED_FAILED) {
                        // 变成未配网状态，进入配网模式
                        AD_LOGI(TAG, "Device unpaired, state AWAKE -> PAIRING");
                        g_ctrl.current_state = APP_CTRL_STATE_PAIRING;
                        enter_pairing_mode();
                    } else {
                        // 网络断开，进入离线状态
                        AD_LOGI(TAG, "Network disconnected, state AWAKE -> OFFLINE");
                        g_ctrl.current_state = APP_CTRL_STATE_OFFLINE;
                        connect_network();
                    }
                }
            } else if (event == APP_CTRL_EVENT_ON_ONLINE_MODE_IDLE) {
                // 在线模式空闲
                AD_LOGI(TAG, "Online mode idle, state AWAKE -> ONLINE");
                g_ctrl.current_state = APP_CTRL_STATE_ONLINE;
            }
            break;

        default:
            AD_LOGE(TAG, "Unknown state %d", g_ctrl.current_state);
            break;
    }
}

// ============================ 公共接口实现 ============================

int app_ctrl_init(void) {
    if (g_ctrl.initialized) {
        AD_LOGW(TAG, "App control module already initialized");
        return 0;
    }

    AD_LOGI(TAG, "Initializing app control module");

    // 初始化状态
    g_ctrl.initialized = true;
    g_ctrl.current_state = APP_CTRL_STATE_INIT;
    g_ctrl.module_status = AD_MODULE_STATUS_NOT_PAIRED_INIT;
    g_ctrl.waiting_for_network = false;

    // 注册模组状态回调
    int ret = ad_sdk_register_module_status_callback(on_module_status);
    if (ret != 0) {
        AD_LOGE(TAG, "Failed to register module status callback: %d", ret);
        g_ctrl.initialized = false;
        return -1;
    }

    AD_LOGI(TAG, "Module status callback registered");

    AD_LOGI(TAG, "App control module initialized successfully");
    return 0;
}

void app_ctrl_deinit(void) {
    if (!g_ctrl.initialized) {
        AD_LOGW(TAG, "App control module not initialized");
        return;
    }

    AD_LOGI(TAG, "Deinitializing app control module");

    // 清理状态
    g_ctrl.initialized = false;
    g_ctrl.current_state = APP_CTRL_STATE_INIT;
    g_ctrl.module_status = AD_MODULE_STATUS_NOT_PAIRED_INIT;
    g_ctrl.waiting_for_network = false;

    AD_LOGI(TAG, "App control module deinitialized");
}

void app_ctrl_on_module_status(ad_module_status_t status) {
    if (!g_ctrl.initialized) {
        AD_LOGE(TAG, "App control module not initialized");
        return;
    }

    // 处理RESET事件
    if (status == AD_MODULE_STATUS_RESET) {
        AD_LOGI(TAG, "Module reset event received, resetting state machine");
        // 重置状态机到初始状态
        g_ctrl.current_state = APP_CTRL_STATE_INIT;
        g_ctrl.module_status = AD_MODULE_STATUS_NOT_PAIRED_INIT;
        g_ctrl.waiting_for_network = false;
        // 不需要调用process_event，因为我们已经重置了状态
        return;
    }

    AD_LOGI(TAG, "Module status updated: %d", status);
    g_ctrl.module_status = status;

    // 处理网络状态事件
    process_event(APP_CTRL_EVENT_ON_MODULE_STATUS);
}

void app_ctrl_on_online_mode_active(void) {
    if (!g_ctrl.initialized) {
        AD_LOGE(TAG, "App control module not initialized");
        return;
    }

    AD_LOGI(TAG, "Online mode active");
    process_event(APP_CTRL_EVENT_ON_ONLINE_MODE_ACTIVE);
}

void app_ctrl_on_online_mode_idle(void) {
    if (!g_ctrl.initialized) {
        AD_LOGE(TAG, "App control module not initialized");
        return;
    }

    AD_LOGI(TAG, "Online mode idle");
    process_event(APP_CTRL_EVENT_ON_ONLINE_MODE_IDLE);
}

app_ctrl_state_t app_ctrl_get_state(void) {
    return g_ctrl.current_state;
}

bool app_ctrl_is_awake(void) {
    return g_ctrl.current_state == APP_CTRL_STATE_AWAKE;
}

