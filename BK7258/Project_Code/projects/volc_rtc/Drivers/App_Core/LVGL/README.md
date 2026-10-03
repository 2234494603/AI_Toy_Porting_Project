# LVGL UI 说明

本目录只负责 UI，不直接负责屏幕硬件启停。屏幕通道由相邻的 `Screen`
模块控制，整体调用关系为：

```text
app_screen_startup()
  -> projects/common_components/dual_screen_avi_play/lvgl_app.c
     -> media_app_lvgl_open()
        -> CPU1: lv_vendor_init()
        -> lcd_display_open()
        -> lv_vendor_start()
```

CPU0 负责发起界面打开/关闭请求，CPU1 负责 LVGL 绘制和 LCD 刷新。这样可以避免
重复创建 LVGL 任务和重复申请显示缓冲区。

`CONFIG_APP_SCREEN_STARTUP` 默认启用，设备上电后打开双屏显示通道；关闭该
选项后，仍可由语音唤醒事件按需启动屏幕。

## 双屏机器人表情

`LVGL/src/robot_face_ui.c` 加载 Windows Designer 工程导出的青色机器人
眼睛，不再依赖 `/genie_eye.avi`。双屏端口提供一张 160 x 320 的虚拟画布：

当前固件直接读取本目录中的 Designer 工程：

`Drivers/App_Core/LVGL/Robot_Face_UI`

只要完整放入 `Robot_Face_UI/`，并确保其中存在
`beken_generated/neutral_init.c`，CMake 就会自动收集全部生成页面、回调和
`custom` 源码，不需要再填写 Windows 绝对路径。Designer 项目文件中的编辑器
路径字段不参与固件构建。

自动发现和 LVGL 8.3 兼容规则集中在 `robot_face_ui.cmake`。因此重新导出或
整体替换 `Robot_Face_UI/` 时，不要修改公共显示组件。

- `y = 0..159`：LCD device 0，默认作为左眼；
- `y = 160..319`：LCD device 1，默认作为右眼。

当前内置 14 组表情：`neutral`、`blink_high`、`happy`、`glee`、
`blink_low`、`sad`、`worried`、`focused`、`annoyed`、`surprised`、
`skeptical`、`sleepy`、`angry`、`scared`。

`volc_rtc` 启动后由 `LVGL/Emotion` 根据设备状态驱动表情，不再自动轮播：

- 唤醒并收音：`focused`；
- 等待云端回复：`skeptical`；
- 播放云端语音：`happy`，语音结束后自动回到 `focused` 继续倾听；
- 配网/理解不确定：`skeptical`；
- 联网成功、智能体上线、充电或升级成功：短暂 `glee`；
- 断网、智能体离线或启动失败：红色 `worried`；
- 低电量：黄色 `worried`；设备移除或低电关机：`sad`。

CPU0 只发送表情事件，真正的页面切换和颜色更新通过 multimedia mailbox
交给 CPU1 的 LVGL 线程完成，避免跨核直接操作 LVGL 对象。

需要固定表情时，在持有 `lv_vendor_disp_lock()` 的 LVGL 上下文中调用：

```c
robot_face_ui_stop_demo();
robot_face_ui_set_expression(ROBOT_FACE_HAPPY);
```

如果两块物理屏幕左右接反，可调用：

```c
robot_face_ui_set_swap_displays(true);
```
