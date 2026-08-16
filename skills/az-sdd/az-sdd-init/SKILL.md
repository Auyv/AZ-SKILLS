---
name: az-sdd-init
version: 4.1.0
description: |
  初始化 AZ-SDD 项目档案。仅在用户显式调用 /az-sdd-init、az-sdd init、sdd init，
  或明确要求按 AZ-SDD/SDD 初始化项目档案时触发。
  普通“初始化项目”不触发；已有 az-sdd/ 时不覆盖。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 初始化意图
  optional:
    - name: project_path
      type: string
      default: 当前目录
    - name: action
      type: string
      default: init
      validation: init 或 migrate
    - name: dry_run
      type: boolean
      default: true
outputs:
  - name: result
    type: object
    description: status、timestamp、初始化路径、warnings、error 和 next_action
  - name: project_archive
    type: directory
    path: "{project_path}/az-sdd/"
errors:
  - code: ERR_INVALID_INPUT
    message: 缺少明确初始化意图或输入格式错误
  - code: ERR_ALREADY_INITIALIZED
    message: 项目已经存在 AZ-SDD 档案
  - code: ERR_ARTIFACT_CONFLICT
    message: 目标路径存在不兼容文件
  - code: ERR_TEMPLATE_MISSING
    message: 初始化模板缺失
  - code: ERR_WRITE_PERMISSION
    message: 目标目录不可写
  - code: ERR_V4_NOT_INITIALIZED
    message: 请求 v4 操作但项目尚未启用 v4
---

# AZ-SDD-INIT

## 做什么

为新项目创建 v4 档案，或把现有 v3 项目显式迁移为“baseline + changes + archive”；v3 原档案不删除、不移动。

## 何时使用

只响应显式 AZ-SDD/SDD 初始化意图。已经存在 az-sdd/、用户拒绝初始化或只是创建普通项目时停止。

## 怎么做

1. 解析 project_path，确认它是目标项目根目录。
2. action=init 且不存在 `az-sdd/` 时，先 dry-run：

       python ../az-sdd-shared/scripts/az_sdd.py init --project {project_path} --title "项目名" --dry-run

3. 用户确认后追加 `--apply`，创建 `PROJECT.md`、`STATUS.md`、`TRACK.md`、`baseline/specs/`、`changes/`、`archive/`、`feedback/`。
4. action=migrate 时先执行：

       python ../az-sdd-shared/scripts/az_sdd.py migrate --from v3 --to v4 --project {project_path} --dry-run

5. dry-run 无冲突且用户确认后改用 `--apply`。复制 v3 spec 到 baseline；v3 文件保持原位。
6. 初始化或迁移后运行 validate/status，确认格式为 v4、功能编号集合一致。
7. 输出结构化 result，next_action 指向 az-sdd-spec 创建第一个 change。

## 关键约束

- 不覆盖现有档案；v3 只能显式迁移。
- STATUS.md 是当前态；TRACK.md 是事件日志。
- 项目不保存用户可选模式；每个 change 由风险评估自动决定附加门禁。
- 创建多文件前先向用户列出清单。
- 写命令默认 dry-run，只有 `--apply` 才落盘。

## 按需读取

- 人工步骤：MANUAL.md
- 旧版完整流程：references/workflow-details-v3.md
- 模板：references/INIT.md.template、references/STATUS.md.template、references/TRACK.md.template
- 单一自适应流程：../az-sdd-shared/references/modes.md
- v4 制品模型：../az-sdd-shared/references/v4-artifact-model.md
