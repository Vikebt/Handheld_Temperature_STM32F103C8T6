# 构建验证记录

| 项目 | 验证日期 | 工具链 | 结果 | 程序资源占用 |
| --- | --- | --- | --- | --- |
| Handheld Temperature Detector | 2026-08-30 | Keil MDK 5.26.2 / ARMCC 5.06 update 6 | 0 Error(s), 0 Warning(s) | Code 33088 B, RO-data 3056 B, RW-data 248 B, ZI-data 15008 B |

验证工程：`Project/MDK-ARM/sud.uvprojx`，目标：`Target 1`。

本次采用 Keil 命令行全量构建并完成链接与 HEX 生成。构建产物仅用于验证，已在提交前清理；`.gitignore` 会阻止其再次进入版本库。
