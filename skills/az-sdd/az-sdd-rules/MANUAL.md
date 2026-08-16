# az-sdd-rules 手动指南

## 输入

- 规则问题或内部检查请求；
- 项目路径和可选功能编号；
- 动作：explain、validate、transition、analyze、change-validate 或 rebuild。

## 步骤

1. 读取 `states.yaml`、`flow.yaml` 和 `naming.yaml`。
2. 不从 SKILL 正文复制或猜测状态规则。
3. validate：检查目录、命名、状态和必需制品。
4. transition：仅对 v3 验证当前状态、目标状态和允许边。
5. analyze：检查规范、设计、任务、测试、反馈和状态的一致性。
6. change-validate：检查 v4 delta 哈希、任务、证据和验收门禁。
7. 按 `critical / warning / info` 输出问题。
8. critical 未解决时阻止进入 coder、completed 或 archive。
9. warning 只有在记录理由后才可接受。

## 验证

- 非法转换必须拒绝；
- YAML 解析失败不得回退到硬编码规则；
- dry-run 不修改项目文件。

## 常见错误

- 状态未知：`ERR_INVALID_STATE`。
- 转换非法：`ERR_TRANSITION_NOT_ALLOWED`。
- 配置错误：`ERR_RULE_CONFIG`。
- 制品冲突：`ERR_ARTIFACT_CONFLICT`。
