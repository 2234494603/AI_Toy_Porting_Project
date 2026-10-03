# Screen

该目录只负责屏幕控制，不保存 UI 页面或表情资源：

- 上电启动两块 160 x 160 屏幕；
- 关闭屏幕显示通道；
- 查询屏幕是否已启动；
- 通过现有媒体服务协调 CPU0 与 CPU1 的双屏显示链路。

底层 LCD、帧缓冲、mailbox 和显示事件继续复用：

`projects/common_components/dual_screen_avi_play`

机器人表情、Designer 工程、生成页面以及 LVGL 8.3 兼容全部放在相邻的
`LVGL/` 模块中。`Screen` 不直接引用 Designer 文件。
