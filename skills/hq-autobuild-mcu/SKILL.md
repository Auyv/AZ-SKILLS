---
name: hq-autobuild-mcu
description: |
  为本地调试 MCU 持续集成仓库自动创建 autobuild-mcu.bat。用户提到“创建 autobuild-mcu.bat”、
  “本地调试 MCU CI 仓库”、“给另一个 MCU 项目生成 autobuild 脚本”或提供含 build/mcu 的仓库路径时使用。
version: 1.0.0
inputs:
  required:
    - name: repository_root
      type: string
      description: MCU 持续集成仓库根目录，内部应包含项目子目录及其 build/mcu
      validation: 必须是存在的本地目录，长度不超过 4096 字符
    - name: product_name
      type: string
      description: 传给 build.bat/pack.bat 的产品配置名，例如 YLB04-A6
      validation: 非空，仅允许字母、数字、点、下划线和连字符
  optional:
    - name: project_name
      type: string
      description: repository_root 下的项目目录名；省略时自动探测唯一的 build/mcu
      default: 自动探测
    - name: build_model
      type: string
      description: 写入 G_BUILD_MODEL 的 MCU 型号，仅用于记录和日志
      default: mcu
    - name: mode
      type: string
      description: pack 仅打包，build-pack 先编译再打包
      default: pack
    - name: output_path
      type: string
      description: 输出批处理路径，必须位于 repository_root 根目录
      default: repository_root/autobuild-mcu.bat
    - name: force
      type: boolean
      description: 是否覆盖已存在的输出文件
      default: false
outputs:
  - name: result
    type: object
    description: 包含 status、timestamp，以及成功 data 或失败 error
    format: JSON
  - name: autobuild_script
    type: file
    description: 可在 Windows 主机运行的 MCU 本地调试入口
    path: "{output_path}"
errors:
  - code: ERR_INVALID_INPUT
    message: 输入为空、类型错误、格式错误或长度超限
    handling: 指出字段和合法格式，不写文件
  - code: ERR_REPOSITORY_NOT_FOUND
    message: 持续集成仓库根目录不存在
    handling: 返回绝对路径并要求修正
  - code: ERR_PROJECT_NOT_FOUND
    message: 未找到或无法唯一确定含 build/mcu 的项目目录
    handling: 列出候选项并要求明确 project_name
  - code: ERR_BUILD_ENTRY_NOT_FOUND
    message: 所选模式需要的 build.bat 或 pack.bat 不存在
    handling: 报告缺失入口，不生成脚本
  - code: ERR_OUTPUT_EXISTS
    message: 输出脚本已存在且未允许覆盖
    handling: 保留原文件；确认后再使用 --force
  - code: ERR_PERMISSION_DENIED
    message: 目标目录无写权限
    handling: 保留原始系统错误并建议更换目录或权限
---

# hq-autobuild-mcu

## 做什么

分析指定 MCU CI 仓库的目录结构，生成项目根目录下的 `autobuild-mcu.bat`，用于 Windows 本地调用 `<project>/build/mcu/build.bat` 和/或 `pack.bat`。

## 何时使用

- 用户要为另一个 MCU 工程复用本地 CI 调试入口。
- 用户提供一个仓库路径、产品配置名，希望创建 `autobuild-mcu.bat`。
- 已有脚本中的项目名、产品名或 MCU 型号不能直接复用。

不要用本技能执行编译、安装 Keil、修改 CI 仓库源码或覆盖现有脚本；这些动作需要用户另行授权。

## 怎么做

1. 读取项目的 `AGENTS.md` 和适用全局规则。
2. 检查 `repository_root`，寻找 `<project>/build/mcu/`。若候选不唯一，要求用户给出 `project_name`。
3. 检查 `pack.bat`；当模式为 `build-pack` 时同时检查 `build.bat`。
4. 运行本技能的生成器。默认 `mode=pack`，与参考脚本的本地打包行为一致。
5. 读取生成文件并检查：路径均有双引号、每个调用都透传 `%errorlevel%`、失败路径会 `popd`、脚本不含 `pause`，便于自动化执行。
6. 仅在用户明确要求时运行生成的脚本；用户提供脚本入口时优先直接运行它。

```powershell
python skills/hq-autobuild-mcu/scripts/create_autobuild_mcu.py `
  --repository-root "C:\work\test_mcu\demo" `
  --product-name "YLB04-A6" `
  --project-name "ac78428_asr1806" `
  --build-model "ac78428" `
  --mode pack
```

生成器把标准 JSON 写到 stdout。成功返回 `status=success`、时间戳和文件路径；失败返回 `status=error`、错误码与原始错误信息，并使用非零退出码。

## 文件结构

```text
hq-autobuild-mcu/
├── SKILL.md
├── MANUAL.md
├── scripts/create_autobuild_mcu.py
└── evals/evals.json
```

## 边界与恢复

- 不猜测多个候选项目；让用户明确选择。
- 不静默覆盖脚本；先读取现有文件，只有 `--force` 才覆盖。
- `--force` 覆盖前由生成器在同目录创建 `.bak` 备份。
- 输入过长、非法产品名、缺少入口脚本均在写文件前失败。
- 输出必须位于仓库根目录，否则 `%~dp0` 相对定位项目会失效。
- 脚本输出采用 UTF-8 BOM，兼顾 Windows 编辑器和中文注释。
