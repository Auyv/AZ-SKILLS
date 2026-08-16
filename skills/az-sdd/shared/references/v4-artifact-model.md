# AZ-SDD v4 制品模型

## 当前行为与变更分离

- `baseline/specs/`：已经生效、可被使用者观察到的当前行为。
- `changes/<CHANGE_ID>/`：尚未合入当前行为的一次变更工作区。
- `archive/<DATE>-<CHANGE_ID>/`：已经验收并合入 baseline 的不可变历史快照。
- `feedback/`：尚未归属具体 change 的跨变更反馈。

v3 的 `spec/`、`arch/`、`coding/`、`test/` 和 `feedback/` 在迁移后继续保留。读取器优先读取 v4 baseline，状态迁移仍可回看 v3 原档案。

## Change 目录

每个 change 必须包含：

| 文件 | 职责 |
|---|---|
| `proposal.md` | 说明原因、范围和非目标 |
| `spec.delta.md` | 声明 ADDED/MODIFIED/REMOVED 行为增量 |
| `design.md` | 记录复杂技术决策；简单变更说明无需独立设计 |
| `tasks.md` | 可执行任务及完成状态 |
| `evidence.md` | 自动化测试和实现证据 |
| `acceptance.md` | 人工验收结论 |
| `feedback.md` | 关联反馈摘要 |
| `.az-sdd-change.json` | 机器可读的 change 元数据和基线哈希 |

元数据中的 `risk.factors` 由任务影响自动识别，`risk.path` 为 `fast` 或 `expanded`。用户不选择模式；风险因子只负责启用 design、完整测试、人工验收、回滚或范围澄清门禁。

## 生命周期

```text
proposal → delta/design → tasks → implementation/evidence
         → acceptance → change-validate → archive → baseline
```

归档门禁要求 delta 无冲突、任务全部完成、证据非占位、验收结果为 `passed` 或 `通过`。归档写入前备份 baseline 和 change；写入失败时自动回滚。
