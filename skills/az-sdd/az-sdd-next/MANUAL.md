# az-sdd-next 手动指南

## 输入

- 项目路径；
- 可选功能编号；
- 动作：next、status 或 converge。

## 步骤

1. 检查 `az-sdd/` 是否存在；不存在时建议 init。
2. 读取 `STATUS.md`、`SPEC.md`、`FEEDBACK.md` 和必要的功能文件。
3. critical 冲突优先返回 rules。
4. v4 有开放 change 时返回其第一个未完成门禁。
5. 再处理未关闭反馈和阻塞项。
6. v3 检查状态机并推荐一个下一步技能。
7. converge 检查 delta、任务、证据、验收和 baseline 冲突。
8. 输出可直接使用的提示词，但不自动触发未获授权的流程。

## 验证

- 普通“下一步”不触发本技能；
- 推荐动作符合状态机；
- critical 冲突存在时不推荐 coder/completed。

## 常见错误

- 未初始化：`ERR_PROJECT_NOT_INITIALIZED`。
- 档案冲突：`ERR_ARTIFACT_CONFLICT`。
- 状态未知：`ERR_INVALID_STATE`。
