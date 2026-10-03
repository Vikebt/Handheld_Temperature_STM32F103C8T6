# 面试证据索引：手持体温检测仪

总讲义见 [模块化五项目面试讲义](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/tree/main/docs/interview-handbook)，本项目重点对应 [ARM/FreeRTOS](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/02-arm-freertos.md)、[P1 项目故事](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/06-project-stories.md#p1手持体温检测仪) 与 [P1 实验](https://github.com/Vikebt/EmbeddedLinux_IMX6ULL_ConditionMonitor/blob/main/docs/interview-handbook/09-experiments.md#2-p1融合条件与-flash-crc-边界)。固定证据标签为本仓库 `study-step-2-testable-fusion`；总讲义固定标签为 `study-step-7-detailed-handbook`。

本页只记录当前仓库中能够由代码或测试证明的内容。没有硬件在环记录的能力，不写成“已在实物验证”。

| 常见问题 | 本项目中的代码证据 | 可说明的工程取舍 |
|---|---|---|
| FreeRTOS 如何启动第一个任务？ | `Core/stm32f10x_it.c` 将 `SVC_Handler` 交给 `vPortSVCHandler` | Cortex-M 端口通过 SVC 恢复首任务上下文；空的 SVC 会让调度器无法正确启动 |
| 队列满怎么办？ | `Core/main.c::prvSendLatest` | 传感器链路选择丢弃最旧样本，保证生产者有界等待和数据新鲜度 |
| 互斥量和临界区如何选？ | I2C 使用 mutex；显示任务只在复制共享标量时进入临界区 | mutex 适合可能阻塞的外设总线；临界区必须短，不能包围 RTC 轮询 |
| 如何发现栈溢出和堆耗尽？ | `FreeRTOSConfig.h` 与两个 application hook | 栈溢出检查设为 level 2；创建 IPC 和任务后立即断言 |
| SysTick 为什么不能用于忙等延时？ | `BSP/delay_timer.c`、MDK 工程文件 | FreeRTOS 独占 SysTick；启动阶段和任务内的微秒延时改由 TIM2 1 MHz 自由运行计数器完成，避免改写 RTOS tick 寄存器 |
| Flash 参数为什么需要 CRC？ | `APP/FLASH_STORE/flash_store.c`、`tests/host/test_flash_store.c` | CRC 覆盖完整记录，计算时将 CRC 字段归零；magic/version/tail 防止误解释旧布局；校验成功前不发布到 RAM |
| Flash 擦除失败如何处理？ | `FlashStore_Init` 与模拟 Flash 页故障注入 | 初始化将擦除/写入失败向上返回，不再无条件报告成功；主机测试在擦除失败时检查返回值 |
| CRC 怎么证明不是“写了就算”？ | `tests/host/test_fusion_policy.c`、`test_flash_store.c` | 前者用 `123456789 -> 0xFEE8` 验证通信 CRC16；后者翻转模拟 Flash 记录字段验证存储 CRC32 的拒绝路径。两种 CRC 不应混为一谈 |
| 多源数据如何保证业务一致性？ | `FusionPolicy_IsMeasurementReady` | 温度、在场卡片及非零 UID 同时有效才形成业务记录，心跳不复用旧 UID |
| 敏感配置怎么处理？ | `FlashStore_SetDefaults` 与 ESP8266 连接接口 | 仓库不保存 SSID、密码和服务器地址；未配置时网络功能显式失败 |

## 验证边界

- Linux 主机 Debug/Release 均实际运行融合策略和模拟 Flash 页故障注入，两项各 2/2 通过；这不能证明片内 Flash 擦写、掉电恢复或 TIM2 实测时序。
- Keil IROM 链接区缩至 `0xFC00`，保留最后 1 KB 给配置页；尚需固件完整链接和 map 文件核对。
- Keil 工程已包含新增的 `fusion_policy.c`，但没有硬件和授权工具链时只能做工程结构与源码级验证。
- ADC/SPI 的初始化检查被标记为 `SKIP`，不再伪装成真实总线回环测试。
