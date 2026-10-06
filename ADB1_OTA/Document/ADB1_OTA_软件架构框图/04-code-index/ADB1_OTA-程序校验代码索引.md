# ADB1 OTA 程序校验代码说明

## 1. 文档范围

本文基于仓库提交 `848dbfa74b15c13ca8e0ec8faa57a9877910c8f0` 的以下工程生成：

- `ADB1_OTA/Keil/Boot/STM32F103_OTA_Bootloader.uvprojx`
- `ADB1_OTA/Keil/APP/STM32F103_AI.uvprojx`
- `ADB1_OTA/Project`

配套可交互图：

- `../01-architecture/ADB1_OTA-system-architecture.html`：总体代码与底层依赖
- `../02-validation-workflow/ADB1_OTA-program-validation-workflow.html`：程序校验工作流
- `../03-ota-lifecycle/ADB1_OTA-state-lifecycle.html`：OTA 状态生命周期

“程序校验”在当前实现中包括三层：协议帧 CRC16、分片边界/顺序检查、接收数据流 CRC32 与 Cortex-M3 向量表检查。它不包含数字签名、证书、公钥或 HMAC，因此能发现传输损坏，但不能证明固件来源可信。

## 2. Flash 布局与链接约束

| 区域 | 起始地址 | 大小 | 代码依据 | 用途 |
|---|---:|---:|---|---|
| Bootloader | `0x08000000` | `0x4000`，16 KB | `ota_layout.h:6-8`；Boot `.uvprojx:279-280` | 启动判定、收包、擦写、校验、跳转 |
| Application | `0x08004000` | `0xBC00`，47 KB | `ota_layout.h:8-10`；APP `.uvprojx:279-280` | 新 APP 镜像与业务程序 |
| Metadata | `0x0800FC00` | `0x400`，1 KB | `ota_layout.h:9-11` | magic、状态、大小、CRC32、版本号 |
| SRAM | `0x20000000`–`0x20005000` | 20 KB | `ota_layout.h:12-13` | 校验 APP 初始 MSP 的允许范围 |

APP 在 `main.c:258` 把 `SCB->VTOR` 设置为 `0x08004000`。Boot 跳转时再次设置 VTOR，并从 APP 向量表读取 MSP 与 ResetHandler。

## 3. 协议帧与命令

帧格式由 APP SDK 和 Bootloader 共同实现：

| 字段 | 长度 | 规则 |
|---|---:|---|
| Header | 2 B | `0xDA28` |
| Version | 1 B | `0x01` |
| Command | 2 B | 大端 |
| Payload length | 2 B | 大端 |
| Payload | N B | 命令相关 |
| CRC16 | 2 B | CRC16-CCITT，初值 `0xFFFF`，多项式 `0x1021` |

| 命令 | 值 | 处理方 | 作用 |
|---|---:|---|---|
| PREPARE | `0x0D00` | APP 或 Boot | 传入镜像大小、CRC32、版本号 |
| PREPARE_RESP | `0x0D01` | APP 或 Boot | 返回结果、最大镜像空间/分片能力 |
| BOOT_READY | `0x0D02` | Boot | 周期报告 Boot 已就绪、当前进度、最大分片 |
| DATA | `0x0D03` | Boot | 携带 offset、data_len、镜像字节 |
| DATA_ACK | `0x0D04` | Boot | 返回结果和下一偏移 `received_bytes` |
| FINISH | `0x0D05` | Boot | 请求最终校验并结束传输 |
| FINISH_RESP | `0x0D06` | Boot | 返回最终校验结果 |
| STATUS | `0x0D07` | 协议保留 | SDK 可分派；Boot 当前未实现独立处理分支 |

命令定义见 `ad_sdk.h:170-177` 与 `bootloader_main.c:13-20`。

## 4. 完整成功路径（精确到函数）

### 4.1 APP 接收升级准备

1. `HAL_UART_RxCpltCallback()` 最终调用 `usart_transport_rx_callback()`，把 UART1 环形缓冲区写指针前移并继续接收下一字节。
2. `main()` 的 10 ms 循环调用 `usart_transport_process()`；当串口空闲达到 20 ms 后，把本批数据交给 `ad_sdk_handle_rx_data()`。
3. `ad_sdk_handle_rx_data()` 检查帧头、版本、载荷长度、完整帧长度和 CRC16，再调用 `handle_message()`。
4. `handle_message()` 遇到 OTA 命令时调用由 `app_ota_init()` 注册的 `on_ota_message()`。
5. `on_ota_message()` 只处理 PREPARE，并依次检查：
   - payload 不为空且不少于 12 字节；
   - `size != 0`；
   - `size <= OTA_APP_MAX_SIZE`，即不超过 47 KB；
   - `write_pending_metadata()` 成功。
6. `write_pending_metadata()` 擦除 `0x0800FC00` 的元数据页，写入 magic、format、PENDING、size、CRC32、version。
7. APP 发送 PREPARE_RESP；成功时 LED1/LED2 交替闪烁，500 ms 后 `NVIC_SystemReset()`。

代码入口：

- `Project/Cold/Usart/src/usart_transport.c:94-160`
- `Project/Cold/WIFI/src/ad_sdk.c:877-976`
- `Project/Cold/WIFI/src/ad_sdk.c:683-693`
- `Project/Cold/OTA/src/app_ota.c:37-86`
- `Project/main.c:423-448`

### 4.2 Bootloader 建立下载会话

1. `bootloader_main.c:main()` 初始化 HAL、时钟、GPIO、UART1/2。
2. 只有 metadata.magic 正确、state 为 VERIFIED/CONFIRMED 且 `application_valid()` 通过时才跳转 APP。
3. PENDING/DOWNLOADING/无效元数据均进入无限接收循环。
4. `send_ready()` 每秒发送 BOOT_READY：结果、`received_bytes`、最大分片 240 B、状态低 16 位。
5. `receive_and_process()` 逐字节同步 `DA 28`，收到完整帧后检查版本与 CRC16，合法才调用 `handle_frame()`。

代码入口：`Keil/Boot/bootloader_main.c:203-212,272-291,322-347`。

### 4.3 DATA 分片门禁与 Flash 写入

`handle_frame(CMD_DATA)` 与 `program_data()` 执行以下检查：

1. DATA 载荷至少 6 字节。
2. `data_len <= 240`。
3. 实际载荷长度必须等于 `6 + data_len`。
4. 非最后一个分片必须为偶数字节，适应 STM32F1 半字编程。
5. 第一片必须从 `offset == 0` 开始；随后才擦除 APP 的 47 个 1 KB 页。
6. `offset` 必须为偶数。
7. `offset == received_bytes`，所以只能严格顺序写入，不能乱序或跳写。
8. `offset + len` 不能超过 metadata.image_size，也不能超过 47 KB。
9. 每个半字调用 `HAL_FLASH_Program()`；奇数长度的最后一片高字节补 `0xFF`。
10. 只有分片写入全部返回 HAL_OK 后，才更新 `running_crc` 和 `received_bytes`。
11. DATA_ACK 始终带回当前 `received_bytes`，供上游判断下一偏移。

代码入口：`Keil/Boot/bootloader_main.c:135-172,235-253`。

### 4.4 FINISH 最终门禁

`handle_frame(CMD_FINISH)` 按顺序检查：

1. `metadata.magic == OTA_METADATA_MAGIC`。
2. `received_bytes == metadata.image_size`。
3. `calculated_crc = running_crc ^ 0xFFFFFFFF` 与 `metadata.image_crc32` 相等。
4. `application_valid()` 读取 APP 向量表前 8 字节：
   - 初始 MSP 必须位于 `0x20000000`–`0x20005000`；
   - ResetHandler 必须位于 `0x08004000`–`0x0800FBFF`；
   - ResetHandler bit0 必须为 1，表示 Thumb 入口。
5. 全部通过后，`set_state(OTA_STATE_VERIFIED)` 把状态编程为 `0xFFFFFFF8`。
6. 返回 FINISH_RESP 成功，延时 100 ms 后系统复位。

代码入口：`Keil/Boot/bootloader_main.c:69-77,102-108,174-180,256-268`。

### 4.5 Boot 安全跳转与 APP 确认

1. Boot 再次检查元数据状态和 APP 向量。
2. `jump_to_application()`：关中断、反初始化 UART/HAL、关闭 SysTick、清 NVIC 使能和挂起位、设置 VTOR、设置 MSP、恢复中断、调用 APP ResetHandler。
3. APP `app_ota_init()` 看到 VERIFIED 后开启 5 秒试运行计时，三灯同步闪烁。
4. 5 秒后 `confirm_running_image()` 把 state 改为 CONFIRMED (`0xFFFFFFF0`) 并熄灯。

代码入口：

- `Keil/Boot/bootloader_main.c:182-201,330-334`
- `Project/Cold/OTA/src/app_ota.c:56-62,88-120`
- `Project/main.c:253-259,384-388,423-438`

## 5. 状态字与掉电行为

| 状态 | 值 | 谁写入 | Boot 是否允许跳转 | 掉电后的行为 |
|---|---:|---|---|---|
| PENDING | `0xFFFFFFFE` | APP/Boot PREPARE | 否 | 驻留 Boot，等待重新发送 |
| DOWNLOADING | `0xFFFFFFFC` | Boot 首片擦除后 | 否 | 驻留 Boot；RAM 进度丢失，必须从 offset 0 重新开始 |
| VERIFIED | `0xFFFFFFF8` | Boot FINISH 成功 | 是 | 再次启动 APP 试运行，5 秒窗口重新计时 |
| CONFIRMED | `0xFFFFFFF0` | APP 运行 5 秒后 | 是 | 正常启动 APP |

状态值设计为不断清零，能够在同一元数据页内从 PENDING 单向编程到 CONFIRMED。

## 6. 错误码与失败反应

| 错误码 | 含义 | 典型触发条件 | 设备反应 |
|---:|---|---|---|
| `0x00` | OK | 当前门禁通过 | 进入下一阶段 |
| `0x02` | PARAM | size 为 0/越界、offset 不连续、非末片奇数长度 | 拒绝请求或分片 |
| `0x03` | LENGTH | 载荷过短、分片声明长度不匹配、最终总长度不一致 | 返回错误并保持当前状态 |
| `0x04` | CHECKSUM | 最终 CRC32 不一致或向量表非法 | 不写 VERIFIED，停留 Boot |
| `0x05` | BUSY | 擦除、编程或状态写入失败 | 不推进状态，等待上游重试 |

## 7. 每个项目源码文件的职责

### 7.1 自研/工程代码

| 源码文件 | 编入目标 | 关键代码/符号 | 与 OTA/校验的关系 |
|---|---|---|---|
| `Project/main.c` | APP | `main()`, `SystemClock_Config()`, `process_keys()` | 设置 APP VTOR；初始化 SDK/OTA；主循环驱动串口、SDK 与 `app_ota_process()` |
| `Project/Cold/OTA/src/app_ota.c` | APP | `write_pending_metadata()`, `on_ota_message()`, `app_ota_init()`, `app_ota_process()` | APP 侧 OTA 核心：PREPARE 门禁、元数据、复位灯效、5 秒确认 |
| `Project/Cold/WIFI/src/ad_sdk.c` | APP | `calculate_crc16()`, `send_message()`, `handle_message()`, `ad_sdk_handle_rx_data()` | APP 协议编解码、CRC16 与 OTA 回调分派 |
| `Project/Cold/Usart/src/usart_transport.c` | APP | `usart_transport_rx_callback()`, `usart_transport_process()`, `usart_transport_tx()` | UART1 环形接收与协议交付；UART2 日志输出 |
| `Keil/Boot/bootloader_main.c` | Boot | `receive_and_process()`, `handle_frame()`, `erase_application()`, `program_data()`, `application_valid()`, `jump_to_application()` | Boot OTA 主体：协议、擦写、CRC32、向量校验、状态提交与跳转 |
| `Keil/Boot/bootloader_it.c` | Boot | Cortex-M3 异常与 `USART1_IRQHandler()` | Boot 中断入口；当前 Boot 数据接收主体使用轮询 `HAL_UART_Receive()` |
| `Project/Cold/AI/src/app_ai.c` | APP | AI 状态回调与 start/finish/interrupt/query | 业务功能；不参与 OTA 校验 |
| `Project/Cold/Control/src/app_ctrl.c` | APP | 应用联网/模式状态机 | 业务功能；不参与 OTA 校验 |
| `Project/Cold/IOT/src/app_iot.c` | APP | 数据点、命令、音量与 LED 业务 | 复用 PB6/PB7/PB8；OTA 灯效期间可能与业务 LED 共享硬件 |
| `Project/Cold/Mode/src/app_modes.c` | APP | AI/Player 模式切换 | 业务功能；不参与 OTA 校验 |
| `Project/Cold/Player/src/app_player.c` | APP | 播放器状态回调 | 业务功能；不参与 OTA 校验 |
| `Project/Cold/Time/src/soft_timer.c` | APP | 软件定时器创建、启动、处理 | APP 协作式调度；OTA 自身使用 `HAL_GetTick()`，不依赖此定时器 |
| `Project/Cold/Time/src/system_tick.c` | APP | 毫秒 tick 读取/延时 | SDK 时间基准；不承担 Boot OTA 超时控制 |
| `Project/Cold/GPIO/src/gpio.c` | APP/Boot | `MX_GPIO_Init()` | 初始化三路 LED 和三路按键；OTA 用 LED 表示阶段 |
| `Project/Cold/Usart/src/usart.c` | APP/Boot | `MX_USART1_UART_Init()`, `MX_USART2_UART_Init()` | UART1 升级协议；UART2 日志；115200 8N1 |
| `Project/Cold/Driver/src/stm32f1xx_it.c` | APP | 异常、SysTick、USART1 中断 | APP 串口中断通过 HAL 回调进入 transport |
| `Project/Cold/Driver/src/stm32f1xx_hal_msp.c` | APP/Boot | `HAL_MspInit()` | HAL 底层时钟/调试配置 |
| `Project/Cold/Driver/src/system_stm32f1xx.c` | APP/Boot | `SystemInit()`, `SystemCoreClockUpdate()` | Cortex-M3 早期系统初始化 |

### 7.2 STM32Cube HAL/CMSIS 依赖

| 源码文件 | 关键能力 | OTA 使用点 |
|---|---|---|
| `stm32f1xx_hal.c` | HAL 初始化、tick、延时、反初始化 | Boot/APP 初始化；复位前延时；跳转前 `HAL_DeInit()` |
| `stm32f1xx_hal_cortex.c` | NVIC、SysTick 封装 | 中断优先级和系统时基 |
| `stm32f1xx_hal_dma.c` | DMA 通用驱动 | 当前 OTA 路径未直接使用，工程统一编入 |
| `stm32f1xx_hal_exti.c` | 外部中断 | 当前 OTA 路径未直接使用 |
| `stm32f1xx_hal_flash.c` | Flash 解锁、上锁、半字编程 | 元数据和 APP 镜像写入 |
| `stm32f1xx_hal_flash_ex.c` | Flash 页擦除 | 擦元数据页与 APP 的 47 个页 |
| `stm32f1xx_hal_gpio.c` | GPIO 读写/初始化 | OTA LED 反馈 |
| `stm32f1xx_hal_gpio_ex.c` | GPIO 扩展 | 工程统一编入，OTA 无直接业务调用 |
| `stm32f1xx_hal_pwr.c` | 电源控制 | 工程统一编入，OTA 无直接业务调用 |
| `stm32f1xx_hal_rcc.c` | 时钟树配置 | Boot/APP 72 MHz 系统时钟与外设时钟 |
| `stm32f1xx_hal_rcc_ex.c` | 扩展时钟配置 | HAL 时钟依赖 |
| `stm32f1xx_hal_uart.c` | UART 初始化、轮询/中断收发 | APP 中断接收、Boot 轮询接收、双方发送响应 |
| `startup_stm32f103xb.s` | 向量表、Reset_Handler、运行库入口 | Boot 位于 Flash 起点；APP 向量表链接到 `0x08004000` |
| `Drivers/CMSIS/Include/core_cm3.h` 等 | VTOR、MSP、NVIC、SysTick 寄存器与内联函数 | `jump_to_application()` 和异常/启动支持 |

### 7.3 关键头文件契约

| 头文件 | 关键定义 |
|---|---|
| `Project/Cold/OTA/inclue/ota_layout.h` | Flash/SRAM 地址、状态值、`ota_metadata_t` |
| `Project/Cold/OTA/inclue/app_ota.h` | APP OTA 初始化与轮询接口 |
| `Project/Cold/WIFI/inclue/ad_sdk.h` | 协议命令、错误码、OTA 回调与发送接口 |
| `Project/Cold/Driver/inclue/main.h` | 引脚、HAL 公共声明与 Error_Handler |
| `Project/Cold/Usart/inclue/usart.h` | `huart1/huart2` 与 UART 初始化接口 |

## 8. 当前校验能力的边界

以下结论来自代码本身，属于设计边界，不是编译错误：

1. **没有真实性校验**：CRC16/CRC32 不能防止恶意固件被重新计算校验值后下发。若产品需要安全 OTA，应增加签名、公钥和版本策略。
2. **CRC32 校验的是接收数据流，不是 Flash 回读内容**：`running_crc` 在 `program_data()` 中对输入缓冲区计算；代码没有从 `0x08004000` 回读整个镜像再算 CRC。因此它能发现传输差错，但不能完整发现 Flash 静默写入错误。
3. **没有逐半字读回比较**：每次写入只检查 `HAL_FLASH_Program()` 返回值。
4. **没有防降级**：`version_code` 被保存，但 Bootloader 未比较当前版本与目标版本。
5. **Boot 未检查 metadata.format_version**：APP 确认逻辑会检查 format_version，Boot 启动和 FINISH 路径主要检查 magic/state/size/CRC。
6. **5 秒确认是存活确认，不是功能健康检查**：APP 只按时间写 CONFIRMED，没有等待联网、传感器、业务自检或云端确认。
7. **单槽位没有旧版本回滚**：开始擦除 APP 后，恢复手段是让 Bootloader 常驻并重新下发镜像。
8. **中断续传没有跨复位持久化进度**：`received_bytes` 只在 RAM；掉电后重置为 0，重新发送必须从 offset 0 开始。

## 9. 建议的后续增强顺序

1. FINISH 阶段从 Flash 回读整个 APP 分区计算 CRC32，而不是只校验接收流。
2. 增加 ECDSA/Ed25519 等数字签名验证，并把公钥固化在 Bootloader。
3. 对 `version_code` 实施防降级策略，并明确开发模式下的解锁机制。
4. 把“5 秒存活”升级为可配置健康检查：主循环、UART、关键业务、联网或上游确认全部通过后再 CONFIRMED。
5. 若硬件容量允许，改用 A/B 双槽或外部 Flash 暂存，实现旧版本自动回滚。

