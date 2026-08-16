---
name: az-sdd-arch
version: 4.1.0
description: |
  为 AZ-SDD 复杂功能或项目创建技术规范。仅在用户显式调用 /az-sdd-arch、
  az-sdd arch、sdd arch，或明确要求按 AZ-SDD/SDD 创建技术规范时触发。
  普通架构讨论、接口设计和代码评审不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 技术规范意图
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
      default: create
      validation: create、update、review 或 validate
    - name: dry_run
      type: boolean
      default: false
outputs:
  - name: result
    type: object
    description: 技术规范路径、关联功能、风险和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 技术目标或范围不明确
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_NOT_FOUND
    message: 关联功能规范不存在
  - code: ERR_ARTIFACT_CONFLICT
    message: 技术规范与功能规范冲突
  - code: ERR_WRITE_PERMISSION
    message: 无法写入技术规范或索引
  - code: ERR_CHANGE_NOT_FOUND
    message: v4 change 不存在
---

# AZ-SDD-ARCH

## 做什么

记录技术选型、系统结构、接口、部署、硬件依赖、运行条件、风险和验证方式；v4 将设计聚合到对应 change。

## 何时使用

项目级约束，或风险评估触发 design_required 时使用。没有设计门禁的简单任务跳过本技能。

## 怎么做

1. validate 项目；v3 功能级 arch 必须找到关联 spec，v4 必须找到关联 change。
2. 判断是否真的需要独立 arch；不需要时把轻量实现影响留在 spec。
3. 收集现有代码、接口、依赖、部署和硬件约束。
4. 比较候选方案，记录取舍和最终决策。
5. v3 使用 references/arch.template.md 生成技术规范；v4 写入 `changes/{change_id}/design.md`。
6. 写清：
   - 模块和依赖；
   - API 和数据模型；
   - 启动、关闭和部署；
   - 硬件及外部系统；
   - 测试环境、回退和风险。
7. 检查 arch 不改变 spec 的功能和验收口径。
8. v3 更新 ARCH.md、STATUS.md；v4 不直接修改 baseline，只运行 analyze。
9. critical 冲突未解决时不进入 coder。

## 关键约束

- 技术规范必须关联验收来源。
- 新依赖必须检查许可证、离线能力和维护风险。
- 复杂图表和详细模板按需读取，不常驻 SKILL 上下文。

## 按需读取

- 模板：references/arch.template.md
- 索引模板：references/ARCH.md.template
- 完整检查清单：references/workflow-details-v3.md
- 人工步骤：MANUAL.md
- v4 制品模型：../az-sdd-shared/references/v4-artifact-model.md
