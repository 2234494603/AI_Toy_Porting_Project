# BK7258 IDK 驱动层

底层驱动继续保留在仓库根目录的 `bk_idk` 中，不复制到产品工程。复制 SDK
源码会造成两套驱动逐渐不一致，也会增加后续升级厂商 SDK 的风险。

STM32 风格目录与 BK7258 目录的对应关系如下：

| 职责 | BK7258 源码位置 |
| --- | --- |
| CMSIS / CPU 架构 | `bk_idk/components/cmsis`、`bk_idk/middleware/arch/cm33` |
| 公共驱动头文件 | `bk_idk/include/driver`、`bk_idk/include/soc` |
| HAL/LL 驱动实现 | `bk_idk/middleware/driver`、`bk_idk/middleware/soc/bk7258` |
| 板级配置 | `bk_idk/middleware/boards/bk7258*` |
| 启动和 RTOS | `bk_idk/components/bk_startup`、`bk_idk/components/bk_init` |

产品代码通过 `Drivers/App_Core` 调用 BK IDK 公共接口。产品状态机、云端密钥和
板级业务行为不能放进 `bk_idk`。
