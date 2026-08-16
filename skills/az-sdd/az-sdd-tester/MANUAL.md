# az-sdd-tester 手动指南

## 输入

- 项目路径和功能编号；
- change ID 或功能编号；
- 动作：create、continue、summarize 或 close。

## 步骤

1. 确认 coder 已完成；v4 的 tasks 全部完成且 evidence 非占位。
2. 根据 arch 或项目文档确认启动、关闭、环境和硬件条件。
3. v3 从 spec 提取用例；v4 从 baseline+delta 提取变化后的用例。
4. 无风险时只记录关键验收；full_test_required 时记录完整测试范围。
5. 用户执行测试并记录实际结果、证据和异常。
6. 失败时判断是实现、规范、环境还是架构问题。
7. v3 生成汇总；v4 写入 change/acceptance.md。
8. manual_acceptance_required 时记录 `验收类型: 人工`；通过后执行 change-validate，失败不得 archive。

## 验证

- 测试单能追溯到 spec；
- 自动化证据不会被人工结论覆盖；
- 失败原因和下一步明确。

## 常见错误

- 缺少测试用例/证据：`ERR_ARTIFACT_NOT_FOUND`。
- 环境不可用：`ERR_TEST_PREREQUISITE`。
- 验收失败：`ERR_TEST_FAILED`。
