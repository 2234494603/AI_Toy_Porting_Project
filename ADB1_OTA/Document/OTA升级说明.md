# STM32F103 OTA 升级说明

## Flash 分区

| 区域 | 地址 | 大小 | 用途 |
|---|---:|---:|---|
| Bootloader | `0x08000000` | 16 KiB | 常驻升级程序，不参与 OTA 覆盖 |
| Application | `0x08004000` | 47 KiB | 可升级应用固件 |
| Metadata | `0x0800FC00` | 1 KiB | 升级状态、镜像大小、CRC32、版本号 |

芯片只有 64 KiB 内部 Flash，当前方案采用单应用槽。Bootloader 永久保留；升级中断后设备会停留在升级模式并等待模组从偏移 0 重新发送。由于没有第二份镜像存储区，本方案不宣称支持自动回滚。若必须回滚，需要增加外部 Flash 或更换更大容量芯片。

## Keil 工程

- `STM32F103_OTA_Bootloader.uvprojx`：地址 `0x08000000`，大小 `0x4000`。
- `STM32F103_AI.uvprojx`：地址 `0x08004000`，大小 `0xBC00`。

首次部署必须先烧录 Bootloader，再烧录 Application。以后 OTA 只传输 Application 生成的 bin 文件，不能把 Bootloader 合并进 OTA 包。

## UART OTA 协议

沿用现有串口协议：帧头 `DA 28`、协议版本 `01`、命令字、大端载荷长度、载荷、CRC16-CCITT。OTA 数据使用标准 CRC32（多项式 `0xEDB88320`，初值和结果异或均为 `0xFFFFFFFF`）。

| 命令 | 值 | 方向 | 载荷 |
|---|---:|---|---|
| PREPARE | `0x0D00` | 模组→MCU | size(4) + crc32(4) + version_code(4)，均为大端 |
| PREPARE_RESP | `0x0D01` | MCU→模组 | result(1) + max_image_size(4) |
| BOOT_READY | `0x0D02` | MCU→模组 | result(1) + next_offset(4) + max_chunk(2) + state(2) |
| DATA | `0x0D03` | 模组→MCU | offset(4) + data_len(2) + data，建议每包 240 字节 |
| DATA_ACK | `0x0D04` | MCU→模组 | result(1) + next_offset(4) |
| FINISH | `0x0D05` | 模组→MCU | 空 |
| FINISH_RESP | `0x0D06` | MCU→模组 | result(1) |
| STATUS | `0x0D07` | 双向预留 | 状态扩展 |

模组必须在收到每个 `DATA_ACK` 后再发送下一包；若 ACK 超时，重发相同 offset。Bootloader 重启后 `next_offset` 为 0，模组应从头发送完整镜像。

## 升级流程和设备反应

1. 应用收到 `PREPARE`，检查镜像不超过 47 KiB，写入 Metadata，并回复 `PREPARE_RESP`。
2. LED1、LED2 交替闪烁，约 500 ms 后进入 Bootloader。
3. Bootloader 每秒发送 `BOOT_READY`；接收数据时 LED1→LED2→LED3 跑马显示。
4. 首包到达后擦除 Application，按顺序写入数据。
5. `FINISH` 后检查长度、CRC32和向量表。成功则标记镜像有效并重启。
6. 新应用启动后，三盏灯快速闪烁；稳定运行 5 秒后把镜像标记为已确认，随后恢复业务 LED 状态。
7. 校验失败、掉电或传输中断时，Bootloader 不跳入损坏应用，继续跑马显示并发送 `BOOT_READY` 等待重新升级。

## 发布前检查

- Application 的 Code + RO-data + RW-data 必须小于 `0xBC00`（47 KiB）。
- Bootloader 必须小于 `0x4000`（16 KiB）。
- OTA 文件必须是 Application 工程生成的 `.bin`，并对该 bin 的全部字节计算 CRC32。
- 模组端必须实现 `0x0D00`～`0x0D06` 命令及 ACK/超时重传。
- 正式产品建议增加签名校验；CRC32只能发现传输损坏，不能防止恶意固件。
