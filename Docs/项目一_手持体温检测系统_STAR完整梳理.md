# 项目一：手持体温检测系统 — STAR 完整梳理

> **代码基线**：STM32F103C8T6 + STM32 StdPeriph + FreeRTOS  
> **最后校准**：2026-08-30；本文以当前仓库可编译源码为准。历史设计资料中的 `STM32F103ZET6`、512 KB Flash、USART2 和外部 24C02 表述不再适用于本工程。

## 一、项目基本信息

| 项目项 | 内容 |
| --- | --- |
| 项目名称 | 手持体温检测系统（轻量级 IoT 嵌入式终端） |
| 项目来源 | 个人竞赛项目；项目资料记录为 2022 年大学生物理创新竞赛作品 |
| 项目成果 | 项目资料记录为完成样机，并获浙江省大学生物理创新竞赛一等奖 |
| 本人角色 | 独立开发者：负责方案设计、模块选型、固件、RTOS 架构、外设联调和样机调试 |
| 主控 | STM32F103C8T6，Cortex-M3，72 MHz，64 KB Flash / 20 KB SRAM |
| 软件基础 | STM32 StdPeriph、FreeRTOS、Keil MDK / ARMCC |

> 竞赛成果为项目资料中的历史记录；本次代码审查实际验证的是仓库源码的编译、链接和资源占用，不将其等同于重新完成硬件性能复测。

## 二、Situation（背景）

面向社区、学校或办公楼的快速体温筛查，接触式测温和手工登记存在交叉接触、效率低、记录难追溯的问题。项目目标是把非接触测温、RFID 身份关联、屏幕提示和网络上报整合在资源受限的 STM32 平台上，并在多外设同时运行时保持数据链路可控。

## 三、Task（目标）

1. 周期性采集 MLX90614 红外温度，并读取 MFRC522 卡片 UID。
2. 将温度与身份数据通过任务间通信解耦，满足“数据就绪后触发处理”的软件流程。
3. 在 OLED 显示温度、电量及运行状态，通过 ESP8266 完成网络侧通信流程。
4. 支持 ADC 电池监测、RTC 时间戳、Flash 配置、异常检测和 IWDG/任务心跳监控。
5. 使工程可在 STM32F103C8T6 的 Flash 与 SRAM 限制内编译、链接。

> 测温精度、上传时延和长期稳定性会受光学结构、标定、网络与实物接线影响；当前源码仓库没有随附可复现的实验记录，因此不在本文声称具体实测数值。

## 四、Action（设计与实现）

### 4.1 硬件与接口基线

| 模块 | 当前源码接口 / 说明 |
| --- | --- |
| 红外测温 | MLX90614，软件 I2C/SMBus 流程 |
| RFID | MFRC522，SPI2（PB12–PB15） |
| WiFi | ESP8266，USART2（PA2 / PA3）；RST：PC13，CH_PD：PB0 |
| 调试串口 | USART1（PA9 / PA10） |
| OLED | 软件串行 GPIO（PA5 / PA7） |
| 电池监测 | ADC1_IN1（PA1） |
| 配置存储 | STM32 内部 Flash 最后 1 KB：`0x0800FC00–0x0800FFFF` |
| 时间与保护 | 内部 RTC（LSI 基线）与 IWDG |

### 4.2 FreeRTOS 任务架构

| 任务 | 周期或触发方式 | 优先级 | 职责 |
| --- | --- | ---: | --- |
| Temperature | 500 ms | 3 | 读取 MLX90614 温度数据 |
| RFID | 300 ms | 3 | 读取 MFRC522 卡片信息 |
| DataFusion | 事件/队列驱动 | 3 | 融合温度与身份信息，组织后续处理 |
| Display | 200 ms | 2 | OLED 状态刷新 |
| WiFi Upload | 事件驱动 | 2 | ESP8266 上传流程 |
| Battery | 2 s | 2 | ADC 采样、电压和电量估算 |
| Watchdog | 1 s | 1 | 任务心跳巡检与 IWDG 服务 |

工程创建了软件 I2C Mutex、系统 EventGroup、温度队列（4 项）和 RFID 队列（2 项）。设计重点是用 Mutex 保护完整软件 I2C 事务，使用 Queue 与 EventGroup 降低任务之间的直接耦合。

### 4.3 数据与可靠性设计

- 温度异常检测包含突变、漂移、越界与卡死四类规则。
- Flash 配置在写入前擦除目标页，配置结构带魔数、版本、尾标记和 CRC-32；CRC 已改为无查表的标准位运算实现，避免截断查表带来的错误校验风险。
- 看门狗模块使用 IWDG（LSI 40 kHz、预分频 64、重装载 2500，约 4 秒超时）和任务心跳表进行分层监控。
- RTC 使用 LSI 时钟基线，适合作相对时间戳；若需要准确绝对时间，应在实物联网后加入校时策略。

### 4.4 统一工程布局

```text
APP/                 传感器、通信、显示、存储与业务模块
BSP/                 延时、串口、按键、LED 等板级支持
Core/                main、中断、FreeRTOSConfig
Libraries/           CMSIS 与 STM32 StdPeriph
FreeRTOS/            内核、include 与 Cortex-M3 移植层
Project/MDK-ARM/     Keil µVision 工程
Docs/                STAR、审查与构建验证记录
```

## 五、Result（交付与验证）

- 工程已按传统 STM32 StdPeriph + FreeRTOS 布局整理，Keil 工程路径为 `Project/MDK-ARM/sud.uvprojx`。
- 使用 Keil MDK 5.26.2 / ARMCC 5.06 update 6 全量编译并链接：**0 Error(s), 0 Warning(s)**。
- 最近一次验证资源占用：Code 33088 B，RO-data 3056 B，RW-data 248 B，ZI-data 15008 B；在 F103C8T6 的 64 KB Flash / 20 KB SRAM 约束内。
- 构建产物已清理，`.gitignore` 已覆盖 Keil 生成目录与目标文件。

详细记录见 [BUILD_VERIFICATION.md](BUILD_VERIFICATION.md)。

## 六、面试陈述建议

可以聚焦“在小容量 F103 上，以 FreeRTOS 将测温、刷卡、显示、上传和电源管理拆分；通过 Queue、EventGroup 和 I2C Mutex 解决并发协同；同时补上 Flash 完整性校验与看门狗恢复链路”。不要把未随仓库提供实验数据支撑的精度、时延、连续运行时长作为已复验指标。

## 七、后续可优化项

1. 在真实硬件上复核电源、电平转换、I2C 地址与所有引脚，再完成温度和电池曲线标定。
2. 为 ESP8266 增加可复现的网络协议测试与断线重连压力测试。
3. 增加单元测试/串口日志用例，记录 Flash 擦写、CRC 异常和看门狗恢复场景。
