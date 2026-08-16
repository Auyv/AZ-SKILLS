---
name: az-sdd-next
version: 4.1.0
description: |
  AZ-SDD 流程导航器。仅在用户显式调用 /az-sdd-next、az-sdd next、sdd next，
  或明确要求按 AZ-SDD/SDD 查看下一步、状态或收敛情况时触发。
  普通“下一步”“继续开发”“查看进度”不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 导航意图
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
      default: next
      validation: next、status 或 converge
    - name: dry_run
      type: boolean
      default: true
outputs:
  - name: result
    type: object
    description: 当前阶段、一致性问题、推荐动作和可直接使用的提示词
errors:
  - code: ERR_INVALID_INPUT
    message: 输入格式错误
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_NOT_FOUND
    message: 状态或目标功能档案不存在
  - code: ERR_ARTIFACT_CONFLICT
    message: 多个档案的状态互相冲突
  - code: ERR_INVALID_STATE
    message: 当前状态无法识别
  - code: ERR_CHANGE_NOT_READY
    message: 开放 change 未满足下一阶段门禁
---

# AZ-SDD-NEXT

## 做什么

读取项目当前态、未关闭反馈和一致性问题，只推荐一个最有价值的下一步。

## 何时使用

把它作为显式 AZ-SDD 主入口。普通开发对话不读取 az-sdd/。

## 怎么做

1. 运行：

       python ../az-sdd-shared/scripts/az_sdd.py next {project_path}

2. 未初始化时，返回 ERR_PROJECT_NOT_INITIALIZED，并给出 /az-sdd-init。
3. 有 critical 一致性问题时，优先推荐 az-sdd-rules analyze。
4. v4 有开放 change 时，读取 risk.gates，优先推荐第一个未完成门禁；fast 路径允许连续完成，不人为拆分步骤。
5. 有未闭环反馈时，优先推荐 az-sdd-feedback resume。
6. v3 按状态选择：
   - draft/reviewing → az-sdd-spec
   - approved/developing → az-sdd-coder
   - testing/acceptance → az-sdd-tester
   - completed/released → az-sdd-track
7. action=converge 时调用 analyze/change-validate，识别 delta、任务、证据、验收和 baseline 冲突。
8. 输出推荐理由和可直接调用的提示词，不自动执行下一技能。

## 关键约束

- 一次只推荐一个主动作。
- critical 未解决时不推荐编码或完成。
- 不修改项目文件。
- 不凭 TRACK.md 推断当前态；以 STATUS.md 和功能制品为准。

## 按需读取

- 人工步骤：MANUAL.md
- 旧版提示词库：references/workflow-details-v3.md
- 状态与流程：../az-sdd-rules/az-sdd-rules-config/
