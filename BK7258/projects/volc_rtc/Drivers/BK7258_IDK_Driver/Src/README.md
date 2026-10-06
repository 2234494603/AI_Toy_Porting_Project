# HAL/LL 源码映射

由厂商维护的驱动实现位于：

- `bk_idk/middleware/driver`
- `bk_idk/middleware/soc/bk7258`
- `bk_idk/components/user_driver`

产品行为应当放在 `Drivers/App_Core`，不能写入这些厂商驱动源码。
