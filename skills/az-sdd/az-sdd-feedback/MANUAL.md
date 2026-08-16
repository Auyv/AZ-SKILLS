# az-sdd-feedback 手动指南

## 输入

- 明确纳入 AZ-SDD 闭环的反馈；
- 可选反馈编号；
- 动作：create、clarify、plan、process、verify、close 或 resume。

## 步骤

1. 创建或读取 `FB-{YEAR}-{SEQ}.feedback.md`。
2. 保存用户原始描述，不用总结替代原文。
3. 判断实现缺陷、验收异常、需求变更、架构问题或新功能。
4. 信息不足时最多询问 1-3 个关键问题。
5. 写入处理计划、验证方式和关闭条件。
6. v4 需求变化先创建/关联 change，并同步 change/feedback.md；再进入 spec、arch、coder 或 tester。
7. 验证通过后标记已解决；无法复现标记待复现；不处理时记录关闭原因。
8. 更新反馈当前态并向 TRACK 追加关键事件；反馈关闭不自动归档 change。

## 验证

- 未验证反馈不能关闭为已解决；
- 转需求反馈关联新/改 spec；
- 每个关闭结论有证据或原因。

## 常见错误

- 反馈内容无效：`ERR_INVALID_INPUT`。
- 关联档案缺失：`ERR_ARTIFACT_NOT_FOUND`。
- 验证失败：`ERR_TEST_FAILED`。
