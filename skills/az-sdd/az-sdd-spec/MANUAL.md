# az-sdd-spec 手动指南

## 输入

- 明确的 AZ-SDD 规范意图；
- 用户需求；
- 可选功能编号、change ID 和风险线索；
- 动作：create、update、clarify、checklist 或 review。

## 步骤

1. 确认项目已初始化。
2. v4 确定 change ID、目标 baseline 和 ADDED/MODIFIED/REMOVED 操作。
3. 自动评估风险因子；不要询问用户选择 lite/standard/strict。
4. 先执行 `change-init ... --dry-run`，确认后以 `--apply` 创建 change。
5. 判断单功能或批量拆分；批量拆分先让用户确认边界。
6. clarify：找出影响验收的歧义，只询问关键问题。
7. v4 编辑 proposal.md 和 spec.delta.md，保留哈希与内容标记；v3 写原 spec。
8. checklist：检查目标、范围、非目标、验收标准、测试和依赖。
9. design_required 时转 arch；无风险时连续进入 coder。
10. 未经检查不进入 coder，且不直接修改 baseline。

## 验证

- 每条需求可被验收；
- 自动化测试与人工测试边界明确；
- 快速路径也包含目标、非目标、验收条件和影响范围。

## 常见错误

- 未初始化：`ERR_PROJECT_NOT_INITIALIZED`。
- 编号/内容冲突：`ERR_ARTIFACT_CONFLICT`。
- 无法写入：`ERR_WRITE_PERMISSION`。
