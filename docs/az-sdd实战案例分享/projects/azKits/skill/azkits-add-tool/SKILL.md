---
name: azkits-add-tool
description: |
  当用户要把 BAT、CMD、PowerShell、Python、Shell 或 EXE 工具描述添加到已有 azKits YAML 配置时使用。
  通过结构化字段添加单个脚本入口，并检查重复 id/alias 与 azKits 字段边界。
version: 1.0.0
inputs:
  required:
    - name: config_path
      type: string
      description: 已有 azKits .yaml 或 .yml 配置路径
      validation: 文件存在且为 UTF-8
    - name: id
      type: string
      description: 配置内唯一工具 ID
      validation: 非空，最多 128 个字符
    - name: alias
      type: string
      description: 配置内唯一运行别名
      validation: 非空，最多 128 个字符
    - name: name
      type: string
      description: 工具显示名称
      validation: 非空，最多 200 个字符
    - name: description
      type: string
      description: 工具用途说明
      validation: 非空，最多 1000 个字符
    - name: path
      type: string
      description: 真实脚本或 EXE 路径，相对路径以配置目录为基准
      validation: 非空
  optional:
    - name: type
      type: string
      description: auto、bat、cmd、powershell、python、shell 或 exe
      default: auto
    - name: enabled
      type: boolean
      description: 是否启用工具
      default: true
    - name: args
      type: array[string]
      description: 固定参数列表
      default: []
    - name: cwd
      type: string
      description: 工作目录，相对配置目录解析
      default: ""
    - name: env
      type: object[string,string]
      description: 固定环境变量
      default: {}
    - name: encoding
      type: string
      description: utf-8、utf-8-sig、gbk 或 gb18030
      default: utf-8
outputs:
  - name: result
    type: json
    description: 包含 status、timestamp 和新增工具信息的结果
    format: '{"status":"success","timestamp":"...","data":{"config":"...","id":"...","alias":"..."}}'
  - name: updated_config
    type: file
    description: 已追加工具描述的原配置文件
    path: "{config_path}"
errors:
  - code: ERR_REQUIRED_INPUT
    message: 缺少必需输入或字段为空
    handling: 返回缺失字段，不修改配置
  - code: ERR_INVALID_TYPE
    message: type、encoding、env 或参数格式非法
    handling: 返回允许值和正确格式
  - code: ERR_FILE_NOT_FOUND
    message: 配置文件不存在
    handling: 建议先使用 azkits-create-config
  - code: ERR_INVALID_CONFIG
    message: 文件不是受支持的 azKits YAML 结构或编码
    handling: 不修改文件，提示先修复并运行 azKits check
  - code: ERR_DUPLICATE_ID
    message: id 已存在
    handling: 不修改文件，请用户提供唯一 id
  - code: ERR_DUPLICATE_ALIAS
    message: alias 已存在（忽略大小写）
    handling: 不修改文件，请用户提供唯一 alias
  - code: ERR_PERMISSION_DENIED
    message: 没有修改配置的权限
    handling: 保留原文件并提示选择可写文件
---

# 向 azKits 配置添加工具

## 做什么

向已有 azKits YAML 的 `scripts` 列表追加一个完整、边界明确的工具描述。参数和环境变量均作为配置数据写入，不接受未经校验的自由系统命令。

## 何时使用

- 用户说“添加工具”“把脚本加入 azKits”“增加一个运行入口”。
- 用户提供一个已有配置和真实脚本信息，需要生成对应 YAML 描述。

如果用户还没有配置文件，先使用 `azkits-create-config`。

## 怎么做

1. 收集 `config_path`、`id`、`alias`、显示名称、用途说明和真实路径。
2. 询问或根据明确的文件扩展名选择 `type`；不确定时使用 `auto`。
3. 只把用户明确给出的参数、工作目录和环境变量写入配置。
4. 运行：

   ```powershell
   python scripts/add_tool.py --config <配置> --id <ID> --alias <别名> --name <名称> --description <说明> --path <脚本路径> --type <类型>
   ```

5. 固定参数使用重复的 `--arg`，环境变量使用重复的 `--env KEY=VALUE`。禁用工具时追加 `--disabled`。
6. 脚本会先检查配置头、`scripts` 字段和重复 `id/alias`，全部通过后才写入。
7. 写入后用项目入口执行 `azkits --config <配置> check`；若检查失败，报告原始错误并恢复同目录下脚本生成的 `.azkits-add-tool.<时间戳>.bak` 备份。

## 关键约束

- 配置必须是 UTF-8 `.yaml` 或 `.yml`，`version` 必须为 `1`。
- `id` 与 `alias` 在单个配置内唯一；alias 比较忽略大小写。
- `type` 仅支持 `auto`、`bat`、`cmd`、`powershell`、`python`、`shell`、`exe`。
- `encoding` 仅支持 `utf-8`、`utf-8-sig`、`gbk`、`gb18030`。
- 相对 `path` 和 `cwd` 以入口 YAML 所在目录为基准。
- 工具路径可以在添加时尚不存在，但最终交付前应运行 azKits `check` 验证。

## 文件结构

```text
azkits-add-tool/
├── SKILL.md
├── MANUAL.md
├── scripts/add_tool.py
└── evals/evals.json
```
