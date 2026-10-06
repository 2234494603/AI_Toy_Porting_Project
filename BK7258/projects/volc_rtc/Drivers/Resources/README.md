# 4. 接入 AI

本章只说明如何把 `Project_Chen` 开发板接入火山引擎“小琴”智能体。
内容按新手第一次操作的顺序编写。完成以后，开发板应当能够联网、加入 RTC
房间、把麦克风声音发送给智能体，并从扬声器播放智能体的语音回答。

> 当前工程使用火山引擎内置豆包模型，不使用千问。不要把百炼 API Key、
> 火山 AccessKey、SecretAccessKey 或 RTC AppKey 写进 `.c`、`.h` 文件，也不要
> 上传到 Git。

## 4.1 先理解接入链路

当前方案不是让 BK7258 直接保存云端密钥，而是使用笔记本作为安全的本地服务端：

```text
用户说话
  ↓
BK7258 开发板（麦克风、Opus、RTC）
  ↓  Project_Chen 2.4 GHz 热点
Windows 笔记本本地服务（192.168.137.1:8080）
  ↓  使用 AK/SK 签名调用 StartVoiceChat
火山引擎 AI 音视频互动智能体“小琴”
  ↓  ASR → 豆包大模型 → TTS
BK7258 开发板（扬声器播放回答）
```

各部分职责如下：

| 部分 | 作用 |
| --- | --- |
| 开发板 | 连接 Wi-Fi、采集麦克风、上传音频、播放云端音频 |
| 笔记本本地服务 | 保管云端密钥、生成 RTC Token、启动和停止智能体任务 |
| 火山 RTC | 传输实时音频并管理房间 |
| ASR | 把用户语音转换成文字 |
| 豆包大模型 | 理解问题并生成回答 |
| TTS | 把回答合成为“小琴”的声音 |

火山官方接口说明：

- [StartVoiceChat 官方文档](https://docs.volcengine.com/docs/real_time_communication/StartAIconversationStartVoiceChat?lang=zh&redirect=1)
- [RTC OpenAPI 调用和签名方法](https://docs.volcengine.com/docs/real_time_communication/HowtocallOpenAPI?lang=zh)
- [实时对话式 AI 方案说明](https://docs.volcengine.com/docs/ark/rtc-ai?lang=zh)

本项目本地服务调用的是新版 `StartVoiceChat 2025-06-01`。官方旧版接口页面也会
提示迁移到新版，不要照抄旧版 `2024-12-01` 请求结构覆盖当前模板。

## 4.2 准备火山引擎账号和服务

### 4.2.1 开通 AI 音视频互动服务

1. 打开[火山引擎控制台](https://console.volcengine.com/home)。
2. 登录并完成个人实名认证。
3. 在顶部搜索框输入“实时音视频”。
4. 进入“实时音视频”产品。
5. 如果页面要求开通服务，阅读协议后开通。
6. 在实时音视频概览中找到“AI 音视频互动方案”。
7. 按页面提示开通“AI 音视频互动”。
8. 暂时不要开启云端录制、转推直播等与语音助手无关的收费功能。

### 4.2.2 确认 AI 应用

本项目必须使用 **AI 音视频互动应用**，不能使用普通 RTC 应用。

1. 打开实时音视频控制台的“应用管理”。
2. 找到 AI 智能体通话所使用的应用。
3. 确认应用状态为“启用”。
4. 记录页面上的 `AppId`。
5. 点击主 `AppKey` 右侧的显示或复制按钮，自己妥善保存。
6. 不要把 AppKey 发到聊天、截图或提交到 Git。

当前工程使用的 AI 应用 AppId 已写在：

```text
Drivers/App_Core/App_Config/include/aidk_board_pins.h
```

对应宏为：

```c
#define CONFIG_RTC_APP_ID "你的 AI 应用 AppId"
```

如果服务端出现下面的错误：

```text
appid ... is not ai agent type
```

说明填入了普通 RTC 应用的 AppId 或 AppKey。回到 AI 音视频互动智能体页面，
找到“通话将使用应用”，改用该应用对应的 AppId 和 AppKey。

### 4.2.3 创建并配置“小琴”智能体

1. 打开[AI 音视频互动方案控制台](https://console.volcengine.com/conversational-ai/agentManage)。
2. 点击“创建智能体”。
3. 智能体名称填写“小琴”。
4. 接入模式选择“标准模型/Agent 接入”。
5. 大模型选择“方舟大模型”。
6. 方舟接入方式选择“使用内置的模型”。
7. 当前工程测试使用 `doubao-seed-2-0-lite-260215`；如果控制台仍提供该模型，
   选择它。若以后模型下线，应选择状态正常的火山方舟内置豆包模型，并同步更新
   本地服务模板。
8. 不要选择“第三方大模型/Agent”，本工程当前不使用千问。
9. 在 Prompt 中填写小琴的人设、回答长度和行为要求。
10. ASR 选择豆包流式语音识别模型。
11. 语音识别模式选择“流式输入流式输出”。
12. TTS 选择豆包语音合成模型。
13. 当前工程使用“俏皮女声”；也可以试听后选择其他女性音色。
14. 欢迎语可填写：`哟，我是小琴，你好呀？`
15. 点击右上角“保存”或“更新”。
16. 在网页右侧点击“开始通话”，先确认网页端能听懂问题并能播放回答。

网页端测试失败时，不要急着修改开发板。先确保智能体本身的 ASR、模型和 TTS
都能在网页中正常工作。

### 4.2.4 确认智能体使用的应用

1. 在智能体编辑页面右上方找到“通话将使用应用”。
2. 选择刚才确认过的 AI 音视频互动应用。
3. 再次点击“保存”或“更新”。
4. 记住这里选择的应用必须和开发板及本地服务中的 AppId 完全一致。

## 4.3 创建服务端 AccessKey

StartVoiceChat 是服务端 OpenAPI，必须使用签名请求，不能只靠 RTC AppKey 调用。

1. 在火山引擎控制台打开“访问控制”。
2. 建议创建一个专门给本项目使用的 IAM 子用户。
3. 给该子用户授予调用 RTC AI 对话接口所需的最小权限。
4. 为子用户创建 AccessKey。
5. 保存以下两项：

```text
AccessKey ID
SecretAccessKey
```

6. SecretAccessKey 通常只在创建时完整显示一次，请保存在安全位置。
7. 不要在本教程、源码、串口日志或 Git 仓库中记录完整密钥。

火山官方建议避免长期使用主账号 AccessKey，并遵循最小权限原则。请求必须携带
由 AK/SK 计算出的签名，开发板不适合直接保存这两个密钥，所以本项目由笔记本
服务端完成签名。

## 4.4 配置 Windows 笔记本网络

### 4.4.1 开启移动热点

1. 打开 Windows“设置”。
2. 进入“网络和 Internet”。
3. 打开“移动热点”。
4. “共享我的 Internet 连接”选择笔记本当前能上网的连接。
5. “共享到”选择 `WLAN`。
6. 点击热点“属性”中的“编辑”。
7. 网络频段选择 `2.4 GHz`。BK7258 不要使用 5 GHz 热点。
8. 热点名称和密码必须与下面文件中的宏保持一致：

```text
Drivers/App_Core/App_Config/include/aidk_board_pins.h
```

```c
#define AIDK_WIFI_SSID     "你的热点名称"
#define AIDK_WIFI_PASSWORD "你的热点密码"
```

9. 打开移动热点。
10. 保持笔记本自身能够访问互联网。

Windows 移动热点默认通常会让笔记本使用 `192.168.137.1`。本项目开发板访问的
本地服务地址在同一个头文件中配置：

```c
#define CONFIG_AGENT_SERVER_HOST "192.168.137.1:8080"
```

如果 Windows 实际地址不是 `192.168.137.1`，在命令提示符执行 `ipconfig`，找到
移动热点网卡的 IPv4 地址，然后同步修改 `CONFIG_AGENT_SERVER_HOST`。

### 4.4.2 放行 Windows 防火墙

第一次启动服务时，Windows 可能弹出防火墙提示：

1. 勾选“专用网络”。
2. 点击“允许访问”。
3. 不建议在公共网络中开放本地演示服务。

## 4.5 配置笔记本本地智能体服务

本地服务目录是：

```text
\\wsl.localhost\Ubuntu\home\lenovo\Project_Chen\projects\volc_rtc\Drivers\App_Core\AI_Service\local_agent_server
```

建议把以上路径粘贴到 Windows 文件资源管理器地址栏，然后在文件资源管理器中
双击脚本。不要先用 CMD `cd` 到 UNC 路径；CMD 对 UNC 当前目录支持不完整。

### 4.5.1 第一次配置

1. 双击 `configure_local_server.cmd`。
2. 按提示输入火山引擎 AccessKey ID。
3. 按提示输入 SecretAccessKey。
4. 按提示输入 AI 应用的 RTC AppKey。
5. 输入密钥时窗口不显示字符属于正常现象。
6. 如果脚本要求智能体代码示例，在“小琴”智能体卡片中点击“代码示例”。
7. 点击弹窗右下角“复制”。
8. 回到配置窗口，根据提示粘贴或按回车继续。
9. 看到“配置已安全保存”后关闭窗口。

配置会保存在：

```text
Drivers/App_Core/AI_Service/local_agent_server/private/
```

该目录已被 `.gitignore` 排除。不要手工取消忽略，也不要把其中的 JSON 发给别人。

### 4.5.2 只更换 RTC AppKey

如果 AppId 没变，只是重新生成或切换了 AppKey：

1. 双击 `set_ai_app_key.cmd`。
2. 输入当前 AI 应用的 AppKey。
3. 等待脚本提示保存成功。
4. 关闭旧的本地服务窗口。
5. 重新启动本地服务。

### 4.5.3 三个脚本分别做什么

| 脚本 | 使用时机 |
| --- | --- |
| `configure_local_server.cmd` | 第一次配置全部云端密钥和智能体参数 |
| `set_ai_app_key.cmd` | 只更新 AI 应用的 RTC AppKey |
| `start_local_server.cmd` | 每次使用开发板前启动本地服务 |

## 4.6 启动并检查本地服务

1. 确认 Windows 移动热点已开启。
2. 确认笔记本可以上网。
3. 双击 `start_local_server.cmd`。
4. 不要关闭弹出的黑色窗口。
5. 正常情况下窗口会显示：

```text
Project_Chen 本地智能体服务已启动
监听地址：0.0.0.0:8080
开发板访问：http://192.168.137.1:8080
```

6. 在笔记本浏览器打开：

```text
http://127.0.0.1:8080/health
```

7. 返回 JSON 且 `code` 为 `200`，表示服务程序已经启动。
8. 再让手机连接同一个 Windows 热点。
9. 用手机浏览器打开：

```text
http://192.168.137.1:8080/health
```

10. 手机也能看到 `code: 200`，表示热点、防火墙和服务端端口都正常。

浏览器额外请求 `/favicon.ico` 并返回 `404` 不影响功能，可以忽略。

## 4.7 检查开发板工程配置

### 4.7.1 检查功能开关

打开：

```text
config/bk7258/config
```

确认至少包含：

```text
CONFIG_BK_DEV_STARTUP_AGENT=y
CONFIG_VOLC_RTC_EN=y
```

### 4.7.2 检查 AppId 和服务端地址

打开：

```text
Drivers/App_Core/App_Config/include/aidk_board_pins.h
```

确认：

1. `CONFIG_RTC_APP_ID` 是 AI 音视频互动应用的 AppId。
2. `CONFIG_AGENT_SERVER_HOST` 是 Windows 热点网卡地址加端口 `8080`。
3. Wi-Fi 名称和密码与 Windows 移动热点完全一致。
4. AppId 可以写入固件，但 RTC AppKey、AK 和 SK 不能写入固件。

### 4.7.3 理解唤醒词和智能体名称的区别

当前固件中的离线唤醒词由 Wanson 离线识别库决定，当前可用的是“阿米诺”相关
唤醒词；“小琴”是云端智能体名称，两者不是同一个配置。

因此正确使用方式是：

```text
说“嗨阿米诺”或“你好阿米诺”唤醒开发板
开发板提示“我在，请说”
随后与云端“小琴”自然对话
```

只修改火山控制台中的智能体名称，不能把离线唤醒词改成“小琴”。如果必须直接
说“小琴”唤醒，需要向离线识别算法供应方获取包含“小琴”的新识别库，替换后
重新生成并烧录固件。

## 4.8 编译和烧录新版固件

只有修改了开发板源码、AppId、Wi-Fi、服务端地址或唤醒库时，才需要重新编译和
烧录。只修改云端人设、模型、音色或本地服务模板时，不需要重新编译开发板。

在 Ubuntu/WSL 终端执行：

```bash
cd ~/Project_Chen/projects/volc_rtc
make bk7258
```

构建日志保存在：

```text
~/Project_Chen/projects/volc_rtc/build/LOG/
```

构建成功后，按照烧录工具要求选择 `build` 中生成的 BK7258 固件并烧录。烧录时：

1. 笔记本本地服务可以保持运行。
2. 确认串口号选择正确。
3. 不要在烧录过程中断开 USB 或开发板电源。
4. 烧录完成后复位或重新上电。

> 本文只给出操作命令，不会自动执行编译或烧录。

## 4.9 第一次完整测试

严格按照下面顺序测试：

1. 打开 Windows 移动热点。
2. 确认笔记本可以上网。
3. 双击 `start_local_server.cmd`，保持窗口开启。
4. 用浏览器确认 `/health` 返回 `200`。
5. 开发板上电。
6. 观察串口，确认开发板连接到 Wi-Fi。
7. 观察本地服务窗口，确认开发板请求了 `/startvoicechat`。
8. 看到“智能体已启动”后，说“嗨阿米诺”。
9. 等开发板播完本地提示“我在，请说”。
10. 正常说一句完整问题，例如“今天星期几？”
11. 说完后停顿约一秒，等待 ASR 判定一句话结束。
12. 开发板应从扬声器播放“小琴”的回答。
13. 继续提问时不需要每句话都重复唤醒词。
14. 测试结束后说“拜拜阿米诺”，或断开设备并确认本地服务调用了
    `/stopvoicechat`。

火山官方说明中，默认自动判停会根据静音时间触发下一轮回答。相关原理参见
[对话判停与触发](https://docs.volcengine.com/docs/real_time_communication/Judgmentstopanddialoguetrigger?lang=zh)。

## 4.10 成功标准

下面各项全部满足，才表示 AI 接入完成：

- 笔记本访问 `http://127.0.0.1:8080/health` 返回 `200`。
- 同一热点中的手机可以访问 `http://192.168.137.1:8080/health`。
- 开发板成功连接 `Project_Chen` 热点。
- 本地服务收到 `/startvoicechat` 并显示“智能体已启动”。
- 唤醒后能听到本地提示音。
- 串口中的 `[VOICE_UPLINK]` 计数持续增加。
- 用户说完问题后，串口出现 `[VOICE_DOWNLINK]`。
- 扬声器能够播放完整的“小琴”语音回答。
- 退出对话时，本地服务收到 `/stopvoicechat`。

## 4.11 常见问题排查

### 4.11.1 `/health` 在电脑能打开，手机打不开

依次检查：

1. 手机是否连接 Windows 的 `Project_Chen` 热点。
2. 热点频段是否为 2.4 GHz。
3. Windows 防火墙是否允许 Python 在专用网络通信。
4. 服务窗口是否仍然开启。
5. 热点网卡地址是否仍是 `192.168.137.1`。

### 4.11.2 开发板提示服务连接失败

检查 `CONFIG_AGENT_SERVER_HOST`。开发板不能使用 `127.0.0.1`，因为开发板的
`127.0.0.1` 指向开发板自身，必须填写笔记本热点网卡地址。

### 4.11.3 服务端提示 `appid ... is not ai agent type`

使用了普通 RTC 应用。重新确认智能体页面的“通话将使用应用”，然后同步更新：

1. `CONFIG_RTC_APP_ID`。
2. 本地服务私密配置中的 `rtc_app_id`。
3. 与该 AppId 配套的 RTC AppKey。

### 4.11.4 能听到欢迎语，之后不回答

按顺序检查：

1. 是否等“我在，请说”完整播放后才开始说话。
2. 串口 `[VOICE_UPLINK]` 是否持续增长。
3. 麦克风是否真正采集到声音，而不是只有空白音频帧。
4. 本地提示音播放结束后，音频上传是否恢复。
5. 火山网页端的“小琴”是否可以正常回答同一个问题。
6. 串口是否出现 `[VOICE_DOWNLINK]`。
7. `write_errors` 是否增长；增长说明云端音频到达但写入播放缓冲失败。

### 4.11.5 StartVoiceChat 返回 HTTP 200，但没有声音

HTTP 200 只代表任务已下发，不代表智能体已经成功入房或正常工作。继续检查
本地服务日志、串口上下行计数和火山控制台中的任务状态。官方也建议通过
VoiceChat 事件监控任务状态。

### 4.11.6 修改了云端配置但开发板还是旧效果

1. 在火山页面点击“保存”或“更新”。
2. 关闭旧的本地服务窗口。
3. 重新运行 `start_local_server.cmd`。
4. 重启开发板，让它创建新的 RTC 会话。

旧会话不会自动切换到新的模型、人设或音色。

### 4.11.7 提示 `prompt tone file is not exist`

这是开发板本地提示音资源问题，不是云端大模型问题。检查音频资源是否已经放入：

```text
Drivers/App_Core/Audio/Resources/
```

并确认 `Drivers/App_Core/App_Config/CMakeLists.txt` 中嵌入的文件名完全一致。

## 4.12 安全和费用注意事项

1. 不要上传 `local_agent_server/private/`。
2. 不要把 AK、SK 或 RTC AppKey 写入开发板固件。
3. 不要在截图、聊天、日志或教程中展示完整密钥。
4. AccessKey 泄露后应立即禁用并重新生成。
5. 现场演示结束后关闭开发板会话和本地服务。
6. 用户退出房间后，应及时调用 StopVoiceChat，避免任务在等待期继续计费。
7. 网页中的云端录制、视觉理解、声音复刻等额外功能可能单独收费，不需要时
   不要开启。
8. 本地笔记本方案只省去云服务器费用，RTC、ASR、TTS 和大模型仍可能按用量计费。

更详细的本地服务说明见：

```text
Drivers/App_Core/AI_Service/local_agent_server/README_CN.md
```
