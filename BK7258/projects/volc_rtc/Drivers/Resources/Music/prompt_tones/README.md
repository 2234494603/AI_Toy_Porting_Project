# 中文 MP3 提示音

本目录中的 15 个文件由 `chinese_prompts/*.wav` 转换而来，供 BK7258 AIDK
官方 `VFS + MP3` 提示音链路使用。

编码参数：

- MPEG Layer III
- 单声道
- 16 kHz
- 40 kbps CBR
- 不含 ID3v1/ID3v2 标签

文件名保留 SDK 默认的 `_en` 后缀，是为了直接匹配
`projects/common_components/bk_app_event/app_event.c` 中的固定资源路径；文件内容仍是
Project_Chen 的中文提示音。

## 放入开发板

1. 用 Type-C 数据线连接 AIDK 开发板和 Windows 电脑。
2. 如果资源盘没有出现，在设备管理器中刷新 USB 设备。
3. 第一次使用时，在 Windows“磁盘管理”中把板载 SD NAND 建成 FAT32 卷。
4. 把本目录内的 15 个 `.mp3` 文件直接复制到 SD NAND 根目录，不要再套一层文件夹。
5. 安全弹出资源盘，再重启开发板。

固件只保存文件路径和 MP3 解码代码，提示音数据不再进入 CPU0 的应用 FLASH。
