#pragma once

#include <stdbool.h>

/* 通过现有媒体服务控制两块物理屏幕及其 LVGL 显示通道。 */
void app_screen_startup(void);
void app_screen_shutdown(void);
bool app_screen_is_started(void);
