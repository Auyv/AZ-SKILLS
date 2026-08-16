---
name: az-sdd-spec
version: 4.1.0
description: |
  创建、修改、澄清和审核 AZ-SDD 功能规范。仅在用户显式调用 /az-sdd-spec、
  az-sdd spec、sdd spec，或明确要求按 AZ-SDD/SDD 编写功能规范时触发。
  普通需求讨论和明确要求直接写代码时不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 规范意图
  optional:
    - name: project_path
      type: string
      default: 当前目录
    - name: feature_id
      type: string
      default: 自动分配
    - name: change_id
      type: string
      default: 自动分配
    - name: action
      type: string
      default: create
      validation: create、update、clarify、checklist、review 或 delta
    - name: dry_run
      type: boolean
      default: false
outputs:
  - name: result
    type: object
    description: 规范路径、审核状态、问题清单和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 需求为空、格式错误或范围无法识别
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_CONFLICT
    message: 功能编号或规范内容冲突
  - code: ERR_ARCH_REQUIRED
    message: 风险评估要求独立技术设计
  - code: ERR_WRITE_PERMISSION
    message: 无法写入规范或索引
  - code: ERR_CHANGE_EXISTS
    message: v4 change ID 已存在
  - code: ERR_BASELINE_CONFLICT
    message: delta 目标与当前 baseline 冲突
  - code: ERR_INVALID_RISK_FACTOR
    message: 风险因子不在 flow.yaml 定义中
---

# AZ-SDD-SPEC

## 做什么

把业务目标变成可审核、可开发、可测试的契约。v4 中先创建 change，再以 delta 表达相对 baseline 的行为变化。

## 何时使用

仅响应显式 AZ-SDD/SDD 规范意图。所有 change 都需要最小 proposal、delta 和验收条件；简单任务自动保持精简，风险只增加必要门禁。

## 怎么做

1. 运行 validate，确认项目结构有效。
2. action=clarify：
   - 找出影响范围、验收或风险的歧义。
   - 能从代码和项目档案获取的内容自行检查。
   - 最多一次询问 1-3 个关键问题。
3. v4 项目先确定 change ID、目标 baseline 文件和 ADDED/MODIFIED/REMOVED 操作，先 dry-run：

       python ../az-sdd-shared/scripts/az_sdd.py change-init CHG-0001 --project {project_path} --title "变更标题" --target specs/0001-name.spec.md --operation MODIFIED [--risk-factor FACTOR] --dry-run

4. 用户确认后以 `--apply` 创建聚合 change；v3 项目继续写 `spec/`。
5. 评估多模块、公共 API、数据迁移、新依赖、安全、硬件、部署、不可逆、测试不足、范围未知和体验变化风险。
6. 把风险因子传给 change-init；无风险时自动标记 fast，有风险时自动生成 expanded 门禁。
7. action=delta 时编辑 `changes/{change_id}/spec.delta.md`；保留机器标记和 base_sha256。
8. action=checklist：检查完整性、清晰度、可测试性和冲突。
9. design_required 时转 az-sdd-arch；简单任务不创建额外设计流程。
10. 写入后运行 analyze；critical 必须修复。v4 不直接改 baseline。

## 规范边界

- spec 说明“做什么、为什么、怎样验收”。
- arch 说明“怎样实现、怎样运行、怎样验证环境”。
- coder 不得用实现方便性改写验收口径。
- 验收标准使用稳定编号；人工用例使用 MT 编号。

## 按需读取

- 模板：references/spec.template.md
- 项目索引模板：references/SPEC.md.template
- 旧版批量示例和格式：references/workflow-details-v3.md
- 人工步骤：MANUAL.md
- v4 delta 格式：../az-sdd-shared/references/delta-format.md
