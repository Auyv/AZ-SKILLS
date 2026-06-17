# Skill 每日进化汇报 - 2026-06-01

## 本次处理的反馈单

- [feedback_20260531_162637_az-sdd.v3-az-sdd-test.md](./feedback_20260531_162637_az-sdd.v3-az-sdd-test.md)
- [feedback_20260531_163248_az-sdd.v3-az-sdd-test.md](./feedback_20260531_163248_az-sdd.v3-az-sdd-test.md)
- [feedback_20260531_170401_az-sdd.v3-az-sdd-test.md](./feedback_20260531_170401_az-sdd.v3-az-sdd-test.md)

## 修改点

| Skill | 修改内容 | 引用反馈单 | 备份位置 |
|------|---------|------------|----------|
| `az-sdd.v3/az-sdd-test` | 明确验收异常如果与功能规范冲突，必须先提示用户；若用户要求立即修复且修复改变功能行为、验收标准或测试用例，需在同一轮同步更新对应功能规范和测试记录。该反馈正向、符合人工验收职责，采用最小规则补充。 | [feedback_20260531_162637_az-sdd.v3-az-sdd-test.md](./feedback_20260531_162637_az-sdd.v3-az-sdd-test.md) | `skill-feedback/backups/az-sdd.v3__az-sdd-test/20260601_010519` |
| `az-sdd.v3/az-sdd-test` | 新增“反复异常”处理规则：同一问题连续 3 次修复仍失败时，第 4 次先整理已尝试方案和阻塞点，用少量问题引导用户确认真实期望，并判断是否需要改规范或重构，避免测试单无限膨胀。该反馈正向、属于验收异常处理边界，修改保持为流程提示。 | [feedback_20260531_163248_az-sdd.v3-az-sdd-test.md](./feedback_20260531_163248_az-sdd.v3-az-sdd-test.md) | `skill-feedback/backups/az-sdd.v3__az-sdd-test/20260601_010519` |
| `az-sdd.v3/az-sdd-test` | 明确 Step 0 只做应用启动/关闭和进入人工验收的最小前置检查，不重复执行 `az-sdd-coding` 已完成的完整自动化测试；若代码未变且已有近期自测证据，应引用结果并执行最小检查。该反馈正向、符合减少流程重叠的目标。 | [feedback_20260531_170401_az-sdd.v3-az-sdd-test.md](./feedback_20260531_170401_az-sdd.v3-az-sdd-test.md) | `skill-feedback/backups/az-sdd.v3__az-sdd-test/20260601_010519` |

## 非修改点

| Skill | 未修改原因 | 引用反馈单 | 给用户的正确使用方式 |
|------|------------|------------|----------------------|
| - | 本次 3 条反馈均判断为正向且属于 `az-sdd-test` 的验收流程职责范围，均已用保守文字补充处理。 | - | - |

## 风险与后续观察

- 本次只调整流程说明，没有修改脚本生成的测试单模板；如果后续仍出现测试单持续膨胀，应再评估是否需要在模板或生成脚本中增加“反复异常摘要”字段。
- `az-sdd-coding` 与 `az-sdd-test` 的测试边界目前只在 test skill 中澄清；如果后续 coding 阶段仍重复引导完整验收前置测试，可再对 coding skill 做对应补充。
