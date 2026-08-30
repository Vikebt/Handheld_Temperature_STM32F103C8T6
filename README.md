# Handheld Temperature Detector · STM32F103C8T6

一套面向快速筛查场景的手持式非接触体温检测终端。项目由本人主导作为团队竞赛作品完成，覆盖硬件方案、STM32 固件、FreeRTOS 多任务设计、外设驱动与整机联调；并非以教学案例为目标搭建的培训工程。

> **MCU**：STM32F103C8T6（Cortex-M3，72 MHz，64 KB Flash / 20 KB SRAM）  
> **项目记录**：2022 年大学生物理创新竞赛作品；STAR 资料记录为浙江省大学生物理创新竞赛一等奖。  
> **本人职责**：独立完成系统方案、模块选型、原理图/接线规划、驱动开发、RTOS 协同、通信协议与样机调试。

## 项目背景

为降低接触式测温的交叉感染风险，并解决人工登记效率低、数据难追溯的问题，我设计了该终端：以红外测温为核心，结合 RFID 身份识别、OLED 本地反馈和 WiFi 数据上报。实现重点不只是“读到温度”，而是在资源有限的 F103C8T6 上处理多外设并发、软件 I2C 总线竞争和异常恢复。

## 实现概览

```text
MLX90614 ─┐                         ┌─> OLED 本地显示
MFRC522 ─┼─> FreeRTOS 数据采集/融合 ├─> ESP8266 上传
ADC 电池 ─┘                         └─> Flash 配置 / RTC 时间戳
                  │
          Queue + EventGroup + Mutex
                  │
            IWDG / 任务心跳监控
```

| 任务 | 周期/触发 | 优先级 | 主要职责 |
| --- | --- | ---: | --- |
| Temperature | 500 ms | 3 | MLX90614 环境/目标温度采集 |
| RFID | 300 ms | 3 | MFRC522 卡片 UID 读取 |
| DataFusion | 事件驱动 | 3 | 融合身份与温度，形成上传数据 |
| Display | 200 ms | 2 | OLED 状态显示 |
| WiFi Upload | 事件驱动 | 2 | ESP8266 透传上报 |
| Battery | 2 s | 2 | ADC 电压采样与电量估算 |
| Watchdog | 1 s | 1 | 任务心跳巡检和 IWDG 喂狗 |

## 技术要点

- 使用 Queue 解耦温度、卡片与数据融合；以 EventGroup 同步“温度就绪 / 身份就绪 / 上传触发”。
- 为 MLX90614 等共享的软件 I2C 操作加入 Mutex，避免抢占破坏 Start–Send–Stop 时序。
- 集成 MLX90614（SMBus/PEC）、MFRC522（SPI）、ESP8266（USART）、OLED、ADC 电量监测、RTC 与 Flash 参数持久化。
- Flash 配置写入采用 C8T6 最后 1 KB 页（`0x0800FC00–0x0800FFFF`），并以 CRC-32 校验配置完整性。
- 异常检测覆盖温度突变、漂移、越界和卡死；看门狗任务通过心跳表避免业务任务静默失效。

## 硬件接口

| 模块 | 接口 / 引脚 |
| --- | --- |
| 调试串口 | USART1：PA9 / PA10 |
| ESP8266 | USART3：PB10 / PB11；RST：PC13；CH_PD：PB0 |
| MFRC522 | SPI2：PB12–PB15 |
| 电池采样 | ADC1_IN1：PA1 |
| OLED | 软件串行：PA5 / PA7 |

> 引脚定义以当前源码和 C8T6 封装为基线；迁移至实物前仍应按原理图复核供电、电平转换与外设地址。

## 工程结构

```text
APP/                 功能模块、传感器与业务任务
BSP/                 延时、串口、按键、LED 等板级支持
Core/                main、中断、FreeRTOSConfig
Libraries/           CMSIS 与 STM32 StdPeriph
FreeRTOS/            FreeRTOS 内核、include、Cortex-M3 移植层
Project/MDK-ARM/     Keil µVision 工程
Docs/                STAR 文档、架构审查和构建验证
```

## 构建与验证

1. 使用 Keil MDK 5 打开 `Project/MDK-ARM/sud.uvprojx`。
2. 选择 `Target 1`，执行 **Rebuild all target files**。

已在 Keil MDK 5.26.2 / ARMCC 5.06 update 6 下完成编译和链接验证；具体日期、资源占用和复核项见 [构建验证记录](Docs/BUILD_VERIFICATION.md)。仓库已忽略 `.axf`、`.hex`、`Obj/`、`Build/` 等生成物，适合直接初始化 Git 仓库。
