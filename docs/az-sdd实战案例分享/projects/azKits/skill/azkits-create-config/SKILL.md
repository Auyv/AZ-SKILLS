---
name: azkits-create-config
description: |
  当用户要为 azKits 新建、初始化或生成一个空白 YAML 配置文件时使用。
  创建符合 azKits version 1 约束的 UTF-8 配置骨架，并按需在同级生成一键启动 BAT，不覆盖已有文件。
version: 1.1.0
inputs:
  required:
    - name: output_path
      type: string
      description: 要创建的 .yaml 或 .yml 文件路径
      validation: 非空，扩展名必须是 .yaml 或 .yml
    - name: toolbox_name
      type: string
      description: 工具箱名称
      validation: 非空，最多 200 个字符
    - name: toolbox_description
      type: string
      description: 工具箱说明
      validation: 非空，最多 1000 个字符
  optional:
    - name: create_parent
      type: boolean
      description: 父目录不存在时是否创建
      default: false
    - name: create_launcher
      type: boolean
      description: 是否在 YAML 同级创建一键启动 BAT
      default: true
    - name: azkits_exe_path
      type: string
      description: azkits.exe 路径；未指定时按配置同级、AZKITS_EXE 环境变量、当前项目 dist、PATH 顺序探测
      default: auto
    - name: launcher_name
      type: string
      description: 启动器文件名，只允许同级 .bat 文件名
      default: "<配置文件名去扩展名>Run.bat"
outputs:
  - name: result
    type: json
    description: 包含 status、timestamp 和已创建文件信息的结果
    format: '{"status":"success","timestamp":"...","data":{"path":"..."}}'
  - name: config_file
    type: file
    description: azKits YAML 配置骨架
    path: "{output_path}"
  - name: launcher_file
    type: file
    description: 使用绝对 EXE 路径和同级 YAML 相对路径的一键启动脚本
    path: "{output_path.parent}/{launcher_name}"
errors:
  - code: ERR_REQUIRED_INPUT
    message: 缺少必需输入或输入为空
    handling: 返回缺失字段并请用户补充
  - code: ERR_INVALID_TYPE
    message: 输入类型不正确
    handling: 返回字段要求，不创建文件
  - code: ERR_INVALID_PATH
    message: 输出路径不是 .yaml 或 .yml
    handling: 提示改用受支持的扩展名
  - code: ERR_FILE_EXISTS
    message: 目标文件已存在
    handling: 不覆盖文件，请用户换路径或明确自行处理旧文件
  - code: ERR_PARENT_NOT_FOUND
    message: 父目录不存在
    handling: 请用户创建目录或明确启用 create_parent
  - code: ERR_PERMISSION_DENIED
    message: 没有写入权限
    handling: 保留原始文件，提示选择可写目录
  - code: ERR_CONFIG_NOT_FOUND
    message: 创建启动器时配置文件不存在
    handling: 先创建或修正 YAML 路径
  - code: ERR_AZKITS_NOT_FOUND
    message: 未找到 azkits.exe
    handling: 指定 EXE 路径或将 EXE 放到配置同级目录
  - code: ERR_INVALID_LAUNCHER_NAME
    message: 启动器名称不是同级 BAT 文件名
    handling: 只提供文件名，不允许目录或非 .bat 扩展名
  - code: ERR_LAUNCHER_EXISTS
    message: 启动器已存在
    handling: 不覆盖已有启动器，请用户确认后另行处理
---

# 创建 azKits 空配置

## 做什么

创建一个可继续添加工具的 azKits YAML 配置骨架，并在需要时创建同级一键启动 BAT。YAML 使用 UTF-8（无 BOM），包含 `version`、`toolbox` 和空的 `scripts` 列表。

## 何时使用

- 用户说“创建 azKits 配置”“初始化工具箱 YAML”“生成空配置”。
- 用户还没有入口 YAML，希望从合法的最小结构开始。
- 用户希望测试人员双击 BAT 即可打开指定 azKits 工具箱。

不要在用户只是要向已有配置添加工具时使用；此时使用 `azkits-add-tool`。

## 怎么做

1. 收集输出路径、工具箱名称和说明；不要猜测会改变业务含义的名称或说明。
2. 确认目标扩展名为 `.yaml` 或 `.yml`。
3. 确认目标不存在。不要隐式覆盖已有配置。
4. 运行：

   ```powershell
   python scripts/create_config.py --output <配置路径> --name <名称> --description <说明>
   ```

5. 父目录不存在且用户明确要求创建时，追加 `--create-parent`。
6. 默认同时创建启动器，运行：

   ```powershell
   python scripts/create_config.py --output <配置路径> --name <名称> --description <说明> --create-launcher --azkits-exe <azkits.exe路径>
   ```

   也可以在已有 YAML 同级单独创建：

   ```powershell
   python scripts/create_launcher.py --config <配置路径> --azkits-exe <azkits.exe路径> --launcher-name <名称.bat>
   ```

7. 未指定 EXE 时，依次探测配置同级 `azkits.exe`、`AZKITS_EXE` 环境变量、当前项目 `dist/azkits.exe` 和 PATH；不要把同名旧版 `azKits.py` 当成 Go 版 EXE。
8. 启动器必须使用 `%~dp0` 引用同级 YAML，避免双击时工作目录不同导致配置找不到；EXE 使用已验证的绝对路径。
9. 启动器应检查 EXE/YAML 是否存在，并在异常退出时 `pause` 保留错误信息。
10. 向用户报告 YAML、启动器的绝对路径和标准 JSON 结果。
11. 运行 `azkits.exe --config <配置路径> check` 验证配置；空配置的 `scripts: []` 是预期状态。

## 边界

- 不访问网络，不安装依赖。
- 不生成真实脚本，不向 `scripts` 添加占位工具。
- 不覆盖已有 YAML 或启动器；需要替换时先让用户明确处理目标文件。
- 启动器只负责启动 azKits，不内嵌或拼接任意工具命令。
- 未找到 `azkits.exe` 时不要退化调用 `azkits` 裸命令或旧版 `azKits.py`。
- 名称和说明分别超过 200、1000 个字符时返回输入错误。

## 文件结构

```text
azkits-create-config/
├── SKILL.md
├── MANUAL.md
├── scripts/
│   ├── create_config.py
│   └── create_launcher.py
└── evals/evals.json
```
