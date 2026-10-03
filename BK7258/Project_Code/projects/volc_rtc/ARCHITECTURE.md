# BK7258 AI 语音开发板架构

## 产品目录

```text
projects/volc_rtc/
├─ Drivers/
│  ├─ App_Core/
│  │  ├─ Board/             AIDK V1.0 板级引脚唯一映射表
│  │  ├─ Input/             开关机按键和功能按键
│  │  ├─ Power/             ETA3422、锂电池 ADC、外部 3.3V LDO、深度休眠
│  │  ├─ Audio/             麦克风、AEC/VAD、HT6872 PA、扬声器和语音资源
│  │  ├─ Wireless/          Wi-Fi/BLE 配网、设备身份和工厂数据
│  │  ├─ LED/               两路状态灯
│  │  ├─ Display_Camera/    GC2145 DVP 摄像头和两块 160×160 显示屏
│  │  ├─ FreeRTOS/          应用任务启动入口，不重复启动调度器
│  │  ├─ Screen/            屏幕启停、状态查询和双屏硬件通道控制
│  │  ├─ LVGL/              机器人表情、Designer 页面和 LVGL 兼容层
│  │  ├─ Storage/           SD NAND、SDIO 和 USB 大容量存储
│  │  ├─ NFC/               MFRC522
│  │  ├─ Sensor/            SC7A20H 陀螺仪/加速度传感器
│  │  ├─ Motor/             PWM 马达反馈
│  │  ├─ AI_Service/        火山 RTC、云端 AI 会话
│  │  ├─ App_Config/        固件入口、启动编排、构建配置、分区和音频参数
│  │  ├─ System/            CPU 频率和应用事件总线
│  ├─ Resources/
│  │  ├─ Music/             固件嵌入的开机音和提示音
│  │  ├─ Server/            Windows 笔记本本地智能体服务
│  │  └─ AI_Resources/      本地 AI 配置资料（敏感文件不得提交）
│  └─ BK7258_IDK_Driver/    对仓库级 BK IDK 驱动层的职责映射
│     ├─ CMSIS/
│     ├─ Inc/               GPIO、I2C、PWM、UART、DMA 等驱动头文件
│     ├─ Src/               BK7258 HAL/LL 驱动实现
│     └─ Startup/           CPU、开发板和 RTOS 启动映射
├─ config/                  CPU0、CPU1、CPU2 的板级和功能配置
└─ build/                   构建产物、日志和本地构建依赖
```

## 硬件与模块对应关系

| 硬件 | 接口 | 负责模块 |
| --- | --- | --- |
| 开关机/功能按键 | GPIO | `Input` |
| Type-C、ETA3422、电池、LDO | USB、ADC、GPIO | `Power` |
| CH340 调试桥 | UART0 | `System` / BK IDK UART 驱动 |
| Type-C 数据 | USB DP/DM | `Storage` / BK IDK USB 驱动 |
| 麦克风、HT6872、扬声器 | MICBIAS、ADC/DAC | `Audio` |
| Wi-Fi/BLE | 2.4GHz 射频 | `Wireless` |
| 两路状态灯 | GPIO | `LED` |
| GC2145 摄像头 | DVP | `Display_Camera` |
| LCD1/LCD2 | QSPI0/QSPI1 | `Display_Camera`、`LVGL`、`Screen` |
| SD NAND | SDIO | `Storage` |
| MFRC522 | UART1 | `NFC` |
| SC7A20H | I2C0（原理图 IIC1） | `Sensor` |
| 马达驱动 | PWM | `Motor` |
| DeepSeek/豆包智能体链路 | Opus/火山 RTC | `AI_Service` |

## 规格书功能映射

| 产品行为 | 触发条件 | 软件实现 |
| --- | --- | --- |
| 音量调大 | S1 短按 | `VOLUME_UP`，音量写入工厂配置区 |
| 进入配网 | S1 长按 3 秒 | `CONFIG_NETWORK`，红绿状态灯交替闪烁 |
| 语言/图像模型切换 | 唤醒状态下 S2 短按 | `APP_EVT_IR_MODE_SWITCH` |
| 开关机 | S2 长按 3 秒 | 深度休眠关机；GPIO12 长按唤醒 |
| 音量调小 | S3 短按 | `VOLUME_DOWN`，音量写入工厂配置区 |
| 恢复出厂设置 | S3 长按 3 秒 | 擦除工厂配置并重启 |
| 上电待机 | 系统启动完成 | 绿灯常亮 |
| 联网/服务连接 | 重连中/服务已连接 | 绿灯快闪/慢闪 |
| 对话状态 | 对话开始/停止 | 绿灯熄灭/慢闪 |
| 网络或服务异常 | 配网、RTC 或智能体连接失败 | 红灯快闪 |
| 低电量 | 电量低于阈值且未充电 | 红灯慢闪 30 秒 |
| 充电状态 | ETA3422 CHG/FULL 输出 | 由充电芯片直接驱动充电红绿灯 |
| NFC 读卡 | MFRC522 检测到卡片 | UART1 轮询并上报 UID |

## 总体启动顺序

```text
main()
  -> rtos_set_user_app_entry()         注册 app_freertos_startup
  -> bk_init()                         初始化厂商平台
  -> app_core_boot_init()              按固定顺序启动硬件和基础服务
       -> 电源 -> 工厂数据 -> LED -> 媒体服务
       -> 事件总线 -> NFC -> 音频 -> CPU1 -> LVGL
       -> 无线配网 -> 按键 -> 电池 -> 存储

SDK user_app_thread
  -> app_freertos_startup()            FreeRTOS 应用任务入口
     -> app_core_background_init()     启动运行时引擎
       -> CPU 策略 -> 音频 -> 显示/摄像头 -> AI 服务
```

## FreeRTOS 与 LVGL 启动关系

```text
BK7258 复位
  -> BK IDK entry_main()
     -> rtos_init()                    初始化 FreeRTOS 端口和内存
     -> start_app_main_thread()        在调度器中创建 main 任务
        -> main()
           -> app_core_boot_init()
              -> media_service_init()
              -> 启动 CPU1
              -> app_screen_startup()

SDK 创建的 user_app_thread
  -> app_freertos_startup()
     -> 启动音频、显示和 AI 运行时模块
```

产品代码不得再次调用 `vTaskStartScheduler()`，因为 BK IDK 已经启动了
FreeRTOS 调度器。LVGL 继续复用现有的
`projects/common_components/dual_screen_avi_play/lvgl_app.c` 和 `lv_vendor`，
避免产生两个刷新任务、重复申请帧缓冲或重复初始化 LCD。

## 开发规则

1. `Drivers/App_Core/App_Config/src/main.c` 只负责平台入口、产品启动和深度休眠。
2. 每项硬件行为只能归属于一个 `App_Core` 模块。
3. 厂商公共驱动保留在 `bk_idk`，公共产品组件保留在
   `projects/common_components`。
4. CPU 专属代码在所属模块内部使用 `CONFIG_SYS_CPU0/1/2` 隔离。
5. 云端密钥必须通过配网或受保护的工厂数据提供，禁止硬编码到应用源码。
