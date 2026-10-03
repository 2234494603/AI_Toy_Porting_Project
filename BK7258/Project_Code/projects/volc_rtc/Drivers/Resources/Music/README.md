# 音频资源

开机语音文本：

> Hello, sir. I'm J.A.R.V.I.S., your personal assistant.

`0_power_on_JARVIS.m4a`、`0_power_on_JARVIS.wav` 和
`power_on_16k_mono_16bit_en.pcm` 仅保留为开机语音的源素材，当前固件不再嵌入或播放它们。

运行时提示音位于 `prompt_tones/`：

- 采用 BK7258 AIDK 官方 `VFS + MP3` 播放链路。
- 文件存放在开发板的 SD NAND 根目录，不占用 CPU0 应用 FLASH。
- 原始中文 WAV/PCM 素材保留在 `chinese_prompts/`，方便后续重新生成。
