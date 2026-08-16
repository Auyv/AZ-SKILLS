# az-sdd-coder 手动指南

## 输入

- 项目路径；
- 已批准功能编号或批量范围；
- change ID 或已批准功能编号；
- 动作：plan、implement、continue、batch 或 refactor。

## 步骤

1. 验证项目；v4 校验 proposal、delta、design 与 baseline 哈希。
2. 对规范、设计、任务和测试要求执行一致性检查。
3. v3 创建 coding 计划；v4 更新 change/tasks.md。
4. 把任务映射到 spec 验收标准和具体文件。
5. 用户批准计划后再修改代码。
6. 优先写自动化测试，再实现最小代码。
7. 运行项目已有构建、测试和静态检查入口。
8. v4 把命令、结果、失败和遗留问题写入 change/evidence.md。
9. v3 测试通过后推进状态；v4 保持 change 开放并转 tester。

## 验证

- 未批准 spec 不编码；
- design_required 但设计未完成时不编码；
- 没有测试证据不进入人工验收。

## 常见错误

- spec 未批准：`ERR_SPEC_NOT_APPROVED`。
- arch 缺失：`ERR_ARCH_REQUIRED`。
- 测试失败：`ERR_TEST_FAILED`。
- 工具缺失：`ERR_EXTERNAL_TOOL_MISSING`。
