# 构建验证记录

| 项目 | 验证日期 | 工具链 | 结果 | 程序资源占用 |
| --- | --- | --- | --- | --- |
| Handheld Temperature Detector | 2026-10-08 | Keil MDK / ARMCC 5.06 update 6 | 0 Error(s), 0 Warning(s) | Code 33704 B, RO-data 3124 B, RW-data 248 B, ZI-data 15008 B |

验证工程：`Project/MDK-ARM/sud.uvprojx`，目标：`Target 1`。

本次采用 Keil 命令行全量构建并完成链接与 HEX 生成。构建产物仅用于验证，已在提交前清理；`.gitignore` 会阻止其再次进入版本库。

链接 map 中 `LR_IROM1` 从 `0x08000000` 开始，使用 `0x90D4` 字节，上限为 `0xFC00`；配置页从 `0x0800FC00` 开始，二者没有重叠。
