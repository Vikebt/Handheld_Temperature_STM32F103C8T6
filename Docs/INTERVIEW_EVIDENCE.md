# 面试证据索引：手持体温检测仪

本页只记录当前仓库中能够由代码或测试证明的内容。没有硬件在环记录的能力，不写成“已在实物验证”。

| 常见问题 | 本项目中的代码证据 | 可说明的工程取舍 |
|---|---|---|
| FreeRTOS 如何启动第一个任务？ | `Core/stm32f10x_it.c` 将 `SVC_Handler` 交给 `vPortSVCHandler` | Cortex-M 端口通过 SVC 恢复首任务上下文；空的 SVC 会让调度器无法正确启动 |
| 队列满怎么办？ | `Core/main.c::prvSendLatest` | 传感器链路选择丢弃最旧样本，保证生产者有界等待和数据新鲜度 |
| 互斥量和临界区如何选？ | I2C 使用 mutex；显示任务只在复制共享标量时进入临界区 | mutex 适合可能阻塞的外设总线；临界区必须短，不能包围 RTC 轮询 |
| 如何发现栈溢出和堆耗尽？ | `FreeRTOSConfig.h` 与两个 application hook | 栈溢出检查设为 level 2；创建 IPC 和任务后立即断言 |
| Flash 参数为什么需要 CRC？ | `APP/FLASH_STORE/flash_store.c` | CRC 覆盖完整记录，计算时将 CRC 字段归零；magic/version/tail 防止误解释旧布局 |
| CRC 怎么证明不是“写了就算”？ | `tests/host/test_fusion_policy.c` | 使用标准检查向量 `123456789 -> 0xFEE8` 做主机单元测试 |
| 多源数据如何保证业务一致性？ | `FusionPolicy_IsMeasurementReady` | 温度、在场卡片及非零 UID 同时有效才形成业务记录，心跳不复用旧 UID |
| 敏感配置怎么处理？ | `FlashStore_SetDefaults` 与 ESP8266 连接接口 | 仓库不保存 SSID、密码和服务器地址；未配置时网络功能显式失败 |

## 验证边界

- 主机测试覆盖融合策略和 CRC；它不能证明 STM32 外设时序正确。
- Keil 工程已包含新增的 `fusion_policy.c`，但没有硬件和授权工具链时只能做工程结构与源码级验证。
- ADC/SPI 的初始化检查被标记为 `SKIP`，不再伪装成真实总线回环测试。
