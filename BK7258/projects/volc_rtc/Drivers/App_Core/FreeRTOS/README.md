# FreeRTOS 启动说明

BK7258 的 FreeRTOS 调度器由 BK IDK 在进入产品 `main()` 之前启动：

```text
bk_idk/components/bk_startup/freertos/rtos_init.c
  -> rtos_init()
  -> start_app_main_thread()
  -> main()
```

本目录的 `app_freertos_startup.c` 是产品应用任务入口，只启动音频、显示、网络
和 AI 等运行时模块。这里禁止再次调用 `vTaskStartScheduler()`。

可通过 `CONFIG_APP_FREERTOS_STARTUP` 控制应用运行时入口，默认启用。
