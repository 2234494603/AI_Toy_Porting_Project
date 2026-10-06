# AI 玩具移植工程（AI Toy Porting Project）

本仓库记录 AI 玩具在嵌入式平台上的移植、集成和验证工作，核心目标是将 **BK7258 / Beken AIDK** 的音频、网络、显示、外设与云端 AI 能力整合为可构建、可烧录、可调试的工程，同时保留 STM32F103 的 OTA 双工程参考实现。

仓库以源码、必要的编译依赖、工程配置、脚本和文档为主；可再生成的构建目录、固件中间产物、重复 SDK 文档、云端密钥及私有运行配置均不纳入版本控制。

## 项目能力概览

| 能力 | 说明 |
| --- | --- |
| AI 语音交互 | 覆盖唤醒、语音采集、AEC/降噪、提示音、云端对话及状态播报链路。 |
| 火山 RTC | `volc_rtc` 工程提供火山 RTC 相关的实时音频交互与应用集成入口。 |
| 显示与 UI | 支持双屏显示、LVGL 界面、机器人表情与设备状态呈现。 |
| 无线连接 | 提供 Wi-Fi、BLE、蓝牙及配网相关组件和配置。 |
| 板级外设 | 涵盖 NFC、按键、LED、振动马达、传感器、充电与电源管理、摄像头等参考实现。 |
| OTA 参考 | `ADB1_OTA` 中包含 STM32F103 的 APP / Bootloader 双工程、Keil 与 CMake 配置及升级说明。 |

## 目录说明

```text
AI_Toy_Porting_Project/
├─ BK7258/                         # BK7258 主工程与 AIDK 移植内容
│  ├─ bk_idk/                      # AIDK / IDK 编译基础与必要 SDK 源码
│  ├─ components/                  # 可复用组件、第三方库与媒体能力
│  ├─ projects/                    # 应用工程集合
│  │  └─ volc_rtc/                 # 火山 RTC AI 玩具应用工程
│  ├─ tools/                       # 构建工具、Makefile 公共逻辑与脚本
│  ├─ dbuild.sh / dbuild.ps1       # Docker 构建辅助入口
│  └─ README_CN.md                 # BK7258 / AIDK 原始中文说明
├─ ADB1_OTA/                       # STM32F103 OTA APP + Bootloader 参考工程
│  ├─ Keil/                        # Keil APP 与 Bootloader 工程
│  ├─ Project/                     # 源码、CMSIS/HAL、CMake 配置
│  └─ Document/                    # OTA 说明、架构与程序校验文档
├─ .vscode/                        # 编辑器工作区配置
├─ .gitignore                      # 构建产物、密钥与私有配置的忽略规则
└─ README.md                       # 本说明
```

## 快速开始

### 1. 获取源码

```bash
git clone https://github.com/2234494603/AI_Toy_Porting_Project.git
cd AI_Toy_Porting_Project/BK7258
```

> 工程内已保留构建需要的 `bk_idk`、`components`、`projects` 与 `tools`。请保持它们与 `BK7258/Makefile` 的相对目录关系，不要单独移动。

### 2. 准备构建环境

BK7258 工程基于 Beken AIDK / Armino 体系。建议使用与项目匹配的 AIDK 工具链、Python 环境和编译依赖；Windows 下可使用 `dbuild.ps1`，Linux/macOS 下可使用 `dbuild.sh` 或原生 `make`。

官方资料：

- [BK7258 AIDK 快速开始](https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/get-started/index.html)
- [BK7258 火山 RTC 说明](https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/thirdparty/volc/index.html)
- [BekenIoT App 配网说明](https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#ai)

### 3. 编译 `volc_rtc`

在 `BK7258` 目录执行：

```bash
make bk7258 PROJECT=volc_rtc
```

构建系统会使用：

- `BK7258/tools/build_main.mk`：统一设置工程根目录与 `ARMINO_PATH`；
- `BK7258/bk_idk`：AIDK / IDK 基础源码与构建依赖；
- `BK7258/components`：音频、网络、RTC、媒体及第三方组件；
- `BK7258/projects/volc_rtc`：AI 玩具应用、板级配置和应用配置。

生成目录均为可再生成内容，默认不会提交到 Git。

### 4. 烧录与运行

1. 按 BK7258 硬件连接说明完成供电、串口或下载接口连接。
2. 使用 AIDK 配套烧录工具将对应固件写入设备。
3. 使用 BekenIoT App 完成网络配置。
4. 设备联网后，根据工程配置唤醒设备并开始 AI / RTC 交互。

具体烧录参数和操作以所使用板卡、芯片版本与 AIDK 官方文档为准。

## `volc_rtc` 工程重点

`BK7258/projects/volc_rtc` 是本仓库的主要 AI 玩具应用工程，包含：

- `Drivers/App_Core`：应用入口及音频、显示、NFC、电源、传感器、存储、无线等模块；
- `Drivers/Resources`：应用使用的资源与服务端辅助脚本；
- `config`：BK7258 / CP1 / CP2 等配置、分区表和 GPIO 配置；
- `CMakeLists.txt` 与 `pj_config.mk`：组件依赖和工程构建描述；
- `README_CN.md`：火山 RTC 工程的专项使用说明。

## STM32F103 OTA 参考工程

`ADB1_OTA` 提供与 BK7258 主工程独立的 STM32F103 OTA 参考实现：

- `Keil/APP`：应用侧工程；
- `Keil/Boot`：Bootloader 工程；
- `Project/Cmake`：GNU Arm / CMake 构建配置；
- `Document`：OTA 升级说明、软件架构、程序校验流程与代码索引。

该部分用于理解 APP 与 Bootloader 的分区、升级、校验和回退协作关系。请根据实际芯片型号、Flash 分区与下载方式调整工程配置后再烧录。

## 配置与凭据安全

仓库**不包含**真实云端凭据或私有运行配置。以下内容默认被 `.gitignore` 排除：

- 火山/AI 服务 `AccessKey`、`ApiKey` 与业务空间密钥文件；
- 本地 `private/` 服务配置、请求模板及运行期数据；
- 构建目录、CMake 中间文件、ELF/MAP/HEX/BIN 等产物；
- 重复 SDK 文档与临时记录。

如需配置云端服务，请在本地根据工程中的 `*.example.json`、配置脚本或部署说明创建私有文件；不要将真实密钥提交到仓库。

## 开发与提交约定

1. 修改功能时优先保持 `BK7258` 根目录、`bk_idk`、`components` 与 `projects` 的相对关系。
2. 不提交 `build`、日志、IDE 缓存、固件产物和可再生文件。
3. 不提交 AccessKey、ApiKey、Token、Wi-Fi 密码或私有服务配置。
4. 提交信息应使用中文描述变更目的、涉及模块和验证范围。
5. 涉及板级参数、分区表、下载方式或云端配置时，请同步更新对应项目文档。

## 相关文档

- [BK7258 AIDK 中文说明](BK7258/README_CN.md)
- [火山 RTC 工程说明](BK7258/projects/volc_rtc/README_CN.md)
- [STM32F103 OTA 文档](ADB1_OTA/Document/OTA升级说明.md)

## 许可证与第三方内容

本仓库包含 Beken AIDK / IDK、CMSIS/HAL、第三方组件及资源。使用、再发布或商业化前，请分别阅读各目录中的 `LICENSE`、`README` 和上游许可证要求。
