---
name: az-sdd-rules
version: 4.1.0
description: |
  为显式触发的 AZ-SDD 流程提供规则解释、状态校验、转换和跨制品一致性检查。
  仅在用户显式调用 /az-sdd-rules、az-sdd rules、sdd rules，
  或由已触发的 az-sdd-* 技能执行内部检查时使用。普通开发对话不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且为明确规则查询或已触发技能的内部检查
  optional:
    - name: project_path
      type: string
      default: 当前目录
    - name: feature_id
      type: string
      default: null
    - name: change_id
      type: string
      default: null
    - name: action
      type: string
      default: validate
      validation: explain、validate、transition、analyze、change-validate 或 rebuild
    - name: dry_run
      type: boolean
      default: true
outputs:
  - name: result
    type: object
    description: 规则结论、严重级别、证据、允许动作和下一步
errors:
  - code: ERR_INVALID_INPUT
    message: 规则动作、状态或功能编号无效
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 需要项目档案但尚未初始化
  - code: ERR_INVALID_STATE
    message: 当前状态不在规则定义中
  - code: ERR_TRANSITION_NOT_ALLOWED
    message: 请求的状态转换不合法
  - code: ERR_ARTIFACT_CONFLICT
    message: 规范、状态、任务或测试证据互相冲突
  - code: ERR_RULE_CONFIG
    message: 规则配置缺失或无法解析
  - code: ERR_BASELINE_CONFLICT
    message: change 创建后的 baseline 已发生变化
  - code: ERR_DELTA_INVALID
    message: v4 delta 格式或目标无效
  - code: ERR_INVALID_RISK_FACTOR
    message: change 风险因子不在 flow.yaml 中
  - code: ERR_RISK_GATE_REQUIRED
    message: 风险触发的条件门禁未满足
---

# AZ-SDD-RULES

## 做什么

读取 YAML 规则真相源，执行确定性的目录、命名、状态转换和跨制品一致性检查。

## 何时使用

- 用户显式查询 AZ-SDD 规则或纠偏原因。
- 已触发的其他 AZ-SDD 技能需要前置/完成检查。
- 不作为普通开发的常驻守卫。

## 规则真相源

- az-sdd-rules-config/states.yaml：状态、转换、当前态同步要求。
- az-sdd-rules-config/flow.yaml：单一流程、风险因子和条件门禁。
- az-sdd-rules-config/naming.yaml：目录、命名和编号。
- SKILL 正文不得复制另一套状态表。

## 怎么做

根据 action 执行：

    python ../az-sdd-shared/scripts/az_sdd.py validate {project_path}
    python ../az-sdd-shared/scripts/az_sdd.py analyze {project_path}
    python ../az-sdd-shared/scripts/az_sdd.py status {project_path}
    python ../az-sdd-shared/scripts/az_sdd.py next {project_path}
    python ../az-sdd-shared/scripts/az_sdd.py rebuild-indexes {project_path} --dry-run
    python ../az-sdd-shared/scripts/az_sdd.py transition ID FROM TO --reason "原因"
    python ../az-sdd-shared/scripts/az_sdd.py change-validate CHANGE_ID --project {project_path}

## 严重级别

- critical：阻止进入 coder、acceptance 或 completed。
- warning：可以继续，但必须显示并记录接受理由。
- info：优化建议，不阻塞。

## 核心门禁

1. 项目结构和命名有效。
2. coder 有已检查的 proposal 和 delta。
3. 风险触发的 design、完整测试、人工验收或回滚计划已经满足。
4. 状态转换存在于 states.yaml。
5. 任务可追溯到验收标准。
6. completed/released 有测试或验收证据。
7. 关闭反馈有验证证据或关闭原因。
8. dry-run 不修改项目。
9. v4 archive 前 proposal、delta、tasks、evidence、acceptance 完整，且 baseline 哈希未变化。

## 关键约束

- YAML 解析失败返回 ERR_RULE_CONFIG，不用硬编码规则绕过。
- 冲突时列出双方路径，不自动覆盖。
- 索引重建只写 AZ-SDD:AUTO 标记区块。
- 状态写入前自动备份目标文件。

## 按需读取

- 旧版完整规则解释与示例：references/workflow-details-v3.md
- 详细规则：SDD规则.md
- 公共错误码：../az-sdd-shared/references/error-codes.md
- 人工步骤：MANUAL.md
- v4 制品模型与 delta：../az-sdd-shared/references/v4-artifact-model.md、../az-sdd-shared/references/delta-format.md
