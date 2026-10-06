# 中文提示音资源

本目录只保存中文提示音的 WAV/PCM 源素材，不再嵌入 `volc_rtc` 固件。
实际运行文件已转换到相邻的 `prompt_tones/` 目录，格式为 16 kHz、单声道、
40 kbps CBR MP3，并由官方 VFS 提示音模块从板载 SD NAND 播放。

| 文件 | 中文播报 |
| --- | --- |
| `Im_here_sir_go_ahead` | 我在，请说。 |
| `Standing_by` | 再见。 |
| `Network_configuration_started` | 开始配网。 |
| `Network_configuration_succeeded` | 配网成功。 |
| `Network_configuration_failed` | 配网失败。 |
| `Reconnecting_to_network` | 正在联网。 |
| `Network_reconnected` | 网络已连接。 |
| `Network_reconnection_failed` | 联网失败。 |
| `Connection_lost` | 网络断开。 |
| `Assistant_online` | 助手上线。 |
| `Assistant_offline` | 助手离线。 |
| `Low_battery` | 电量不足。 |
| `Update_succeeded` | 更新成功。 |
| `Update_failed` | 更新失败。 |
| `Assistant_start_failed` | 助手启动失败。 |

运行时事件映射由 `projects/common_components/bk_app_event/app_event.c` 负责；
`Drivers/App_Core/Audio/src/app_audio.c` 只保留 AI 音频初始化和蓝牙通话抢占管理。
