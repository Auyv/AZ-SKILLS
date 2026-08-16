# mcu_upgrade_app

## 项目说明

`mcu_upgrade_app` 是面向嵌入式设备的固件升级应用，负责 MPU、MCU 及 NF 升级包的检测、校验、刷写、进度查询、错误处理和掉电恢复。项目采用 C 语言实现，使用 CMake 与交叉编译工具链构建，覆盖 AG35_ASR1806、AG35_CEVN、AR590、SIM7805 等硬件平台。

## 工作约定

- 修改 C 代码时遵循项目既有代码风格，并保持日志兼容 ASCII/GB2312 中文显示。
- 构建优先使用 `build/build.sh`（Linux）或 `build/ac8257_64bit/build.bat`（Windows）等已有入口。
- 涉及固件刷写、升级包或设备操作时，先确认目标平台、升级包和测试环境。

## AZ-SDD 约定

- 功能需求在 `az-sdd/spec/` 中维护；技术方案在 `az-sdd/arch/` 中维护。
- 当前状态以 `az-sdd/STATUS.md` 为准，关键历史仅追加到 `az-sdd/TRACK.md`。
- 在实施已批准功能前，先确认对应规范的验收条件与状态。
