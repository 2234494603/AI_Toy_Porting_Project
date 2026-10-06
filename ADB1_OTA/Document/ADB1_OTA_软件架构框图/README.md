# ADB1 OTA 软件架构与程序校验文档

本目录由 Archify 基于提交 `848dbfa74b15c13ca8e0ec8faa57a9877910c8f0` 的实际代码生成。

## 交付物

1. `01-architecture/ADB1_OTA-system-architecture.html`：总体程序架构、Flash 分区、APP/Bootloader 与 STM32 HAL/CMSIS 依赖。
2. `02-validation-workflow/ADB1_OTA-program-validation-workflow.html`：PREPARE、DATA、FINISH 的逐级校验和失败恢复路径。
3. `03-ota-lifecycle/ADB1_OTA-state-lifecycle.html`：PENDING、DOWNLOADING、VERIFIED、CONFIRMED 生命周期。
4. `04-code-index/ADB1_OTA-程序校验代码索引.md`：逐函数校验说明、全部编译源码职责、风险边界和增强建议。
5. `05-code-program-flow/ADB1_OTA-code-program-flow.html`：按代码执行顺序绘制的 UART 中断、APP 准备、Boot 写入/校验、可信启动与 APP 确认框图。

每个 Archify 子目录同时保留 JSON 规范、HTML 成品、最终校验摘要和浏览器检查凭据。

