#pragma once

/*
 * 应用层 FreeRTOS 启动入口。
 * BK IDK 已经启动调度器，这里只启动产品运行时模块。
 */
void app_freertos_startup(void);
