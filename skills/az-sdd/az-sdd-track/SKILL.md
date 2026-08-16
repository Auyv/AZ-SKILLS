---
name: az-sdd-track
version: 4.1.0
description: |
  查看、校验和更新 AZ-SDD 项目状态，并生成项目汇报。
  仅在用户显式调用 /az-sdd-track、az-sdd track、sdd track，
  或明确要求按 AZ-SDD/SDD 管理状态时触发。普通进度查询不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 状态或汇报意图
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
      default: status
      validation: status、feature、change、archive、rebuild、report 或 history
    - name: dry_run
      type: boolean
      default: true
outputs:
  - name: result
    type: object
    description: 状态摘要、冲突、报告路径和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 查询参数或动作无效
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_NOT_FOUND
    message: 状态、索引或目标功能不存在
  - code: ERR_ARTIFACT_CONFLICT
    message: 状态看板和制品索引不一致
  - code: ERR_WRITE_PERMISSION
    message: 无法重建状态或写入报告
  - code: ERR_CHANGE_NOT_READY
    message: change 未通过归档门禁
  - code: ERR_ARCHIVE_FAILED
    message: 归档写入失败并已自动回滚
---

# AZ-SDD-TRACK

## 做什么

以 STATUS.md 为当前态入口，校验并汇总功能、反馈、阻塞和关键历史，按需生成面向读者的 HTML 报告。

## 何时使用

只响应显式 AZ-SDD/SDD 状态管理或汇报意图。可以在任意 AZ-SDD 阶段使用。

## 怎么做

1. action=status：

       python ../az-sdd-shared/scripts/az_sdd.py status {project_path}

2. action=feature：v3 读取目标链路；v4 读取 baseline 和关联 archive。
3. action=change：汇总 proposal、delta、design、tasks、evidence、acceptance 和 feedback。
4. action=archive 先 dry-run：

       python ../az-sdd-shared/scripts/az_sdd.py archive CHANGE_ID --project {project_path} --dry-run

   仅在 change-validate 通过且用户确认后改用 `--apply`。
5. action=rebuild：先 dry-run，再经用户允许执行：

       python ../az-sdd-shared/scripts/az_sdd.py rebuild-indexes {project_path} --dry-run

6. 发现冲突时返回 ERR_ARTIFACT_CONFLICT，不直接覆盖人工内容。
7. 重建脚本只维护 AZ-SDD:AUTO 标记区块。
8. action=report：
   - 读取 INIT、STATUS、SPEC、FEEDBACK、TRACK。
   - 输出项目目标、总体进度、当前风险、未闭环反馈、下一步和证据链接。
   - 使用 references/generate-project-report-html.py。
9. TRACK 只追加关键事件，不承担状态总览。

## 报告要求

- 浏览器可直接打开并可打印。
- 一分钟内能理解结论、风险和下一步。
- 不要求读者理解 AZ-SDD 内部流程。
- 不堆叠完整规范正文。

## 按需读取

- 完整视图和报告字段：references/workflow-details-v3.md
- 报告脚本：references/generate-project-report-html.py
- 人工步骤：MANUAL.md
