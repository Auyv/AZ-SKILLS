---
name: hq_ctp_autobuild_doc
description: |
  生成或更新 CTP autobuild / 持续集成仓库说明文档。触发词包括“生成集成仓文档”、“更新 autobuild 文档”、“重写持续集成仓库说明文档”。基于目标仓库 manifests/config/build/tools 生成项目层 README、项目差异表、CI 总览、list.env 说明、MCU/MPU/Step3/FAQ 等文档。
version: 0.1.0
inputs:
  required:
    - name: target_repo
      type: directory
      description: 目标持续集成仓库根目录，必须可读
    - name: output_dir
      type: directory
      description: 项目层文档输出目录，通常为 doc/autobuild/<project>
  optional:
    - name: template_dir
      type: directory
      description: 平台模板目录
      default: C:/HQ_Workspace/YLB04_A6/doc/autobuild/ctp_template
    - name: project_name
      type: string
      description: 文档项目名；未提供时从仓库名、manifest、config 目录推断
      default: null
    - name: product_filter
      type: array
      description: 只生成指定产品配置的文档；未提供时扫描 config/*/list.env
      default: []
    - name: mode
      type: string
      description: generate 或 update
      default: update
    - name: backup
      type: boolean
      description: 更新前备份现有 output_dir
      default: true
outputs:
  - name: project_readme
    type: file
    path: "{output_dir}/README.md"
    description: 项目层入口说明
  - name: doc_set
    type: directory
    path: "{output_dir}/"
    description: 项目层 01-11 文档集合
  - name: variable_table
    type: file
    path: "{output_dir}/G_PROJ_变量总表(<product>).md"
    description: 从 list.env 汇总出的变量说明
  - name: backup_dir
    type: directory
    path: "{output_dir}.bak-<timestamp>"
    description: 原目录备份
  - name: time_metadata
    type: markdown
    description: 每份文档首部的时间属性块，包含生成时间、扫描基准时间、适用分支/revision、时效边界和复核建议
errors:
  - code: ERR_TARGET_REPO_NOT_FOUND
    message: 目标仓库不存在或不可读
    handling: 停止生成，要求用户提供正确 target_repo
  - code: ERR_TEMPLATE_MISSING
    message: 平台模板目录缺失
    handling: 停止生成或在用户允许下降级参考样例
  - code: ERR_NO_PRODUCT_CONFIG
    message: 未找到 config/*/list.env
    handling: 生成仓库级骨架文档并标待补齐项
  - code: ERR_MANIFEST_PARSE_FAILED
    message: manifests XML 无法解析
    handling: 保留解析错误并用文本摘要降级生成
  - code: ERR_ENCODING_SUSPECTED
    message: 检测到疑似乱码或混合编码
    handling: 停止复制乱码原文，重写对应段落
  - code: ERR_BACKUP_FAILED
    message: 备份现有文档失败
    handling: 停止写入，避免覆盖
---

# hq_ctp_autobuild_doc

## 何时使用

用户要求生成、更新、重写 autobuild / 持续集成仓库说明文档时使用。

## 扫描目标

从目标仓库读取：

- `manifests/*.xml`：project、path、revision、Step1/2/3 关系。
- `config/<product>/list.env`：产品编码、MPU 型号、HUBS 版本、模块清单、构建开关。
- `build/mcu/*.bat`、`build/mpu/**/*.sh`、`build/pack_mcu_mpu.bat`：入口、参数、产物。
- `tools/batch_nfcreator/`、`tools/secure_ota_pack/`、`tools/NFCreator*`：出包和安全工具。
- partition 相关文件：识别旧兼容目录和迁移方向。

## 输出文档

默认生成或更新：

```text
README.md
01-项目概览.md
02-CI流水线总览.md
03-项目差异表.md
04-环境与依赖.md
05-产品配置list.env说明.md
06-MCU构建与打包.md
07-MPU构建与打包.md
08-信息安全与开机自检.md
09-整包升级打包.md
10-常见问题与调试记录.md
11-新增产品配置指南.md
G_PROJ_变量总表(<product>).md
```

## 编写规则

- README 首屏说明集成仓、产品配置、MCU、MPU、MPU 脚本族、Step3 工具和平台模板链接。
- 每份生成或更新的文档必须在正文开头加入“时间属性”块，明确生成时间、扫描基准时间、适用仓库/分支/revision、适用产品配置、有效性边界和复核建议。
- `03-项目差异表.md` 覆盖项目基本信息、芯片平台、repo XML、产品配置、模块清单、MPU 映射、MCU 差异、Step 产物、安全差异、风险。
- `06/07/09` 写清实际入口脚本、运行目录、关键参数、输出目录。
- 项目层只写差异和实例化结果，公共规范链接 `../ctp_template`。
- 未从仓库事实确认的信息写“待确认”，不要编造。


## 时间属性与时效性

所有项目层文档都必须包含时间属性，避免后续维护者误把历史状态当成当前事实。建议放在标题后、正文前：

```markdown
> 时间属性
> - 文档生成时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 仓库扫描基准时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 适用仓库/分支：<repo>/<branch 或 revision>
> - 适用产品配置：<product list>
> - 时效性说明：本文基于上述时间点的 manifests/config/build/tools 生成；仓库分支、Jenkins 参数、NFCreator、partition 或 list.env 变化后需要复核。
> - 建议复核周期：新增产品、切分支、调整 Jenkins、升级 SDK/工具链、修改 secure OTA/partition 时立即复核；常规项目建议每次发版前复核。
```

写作要求：

- 不使用“当前”“最新”“现在”等无时间锚点表述，必须绑定具体日期、分支或 revision。
- 表格中涉及 revision、SDK、工具链、Docker 镜像、Jenkins 参数时，增加“采集时间/来源文件”列或在表下注明来源时间。
- 无法确认时间来源时写“待确认”，不要推断。
- changelog 必须记录本次更新时间、扫描范围、更新文件和未复核项。
## 备份与编码

- update 模式且输出目录存在时，先备份为 `.bak-YYYYMMDD-HHMMSS`。
- 乱码检查命中 `锛`、`鈥`、`銆`、`�` 时，不复制原文，改为 UTF-8 重写。
- 备份失败停止写入。

## partition 旧兼容说明

生成 `09-整包升级打包.md` 时必须说明：

- 当前 partition 来源。
- 是否仍使用 `tools/batch_nfcreator/partition_ini/<product>/` 旧兼容目录。
- 新模式应迁移到 NFCreator `Images/<CODE>/` 产品目录。
- 迁移前不要删除当前脚本仍检查的兼容目录。