# AZ-SDD 单一自适应流程

AZ-SDD v4.1 取消 lite、standard、strict 用户可选模式。所有 change 使用同一生命周期，用户只描述任务，不选择流程强度。

## 快速路径

没有风险因子时，Agent 可以连续完成 change、实现、验证和归档。proposal、delta、tasks、evidence、acceptance 仍然存在，但内容保持最小。

## 风险展开

出现多模块、公共 API、数据迁移、新依赖、安全、硬件、部署、不可逆、测试不足、范围未知或体验变化时，自动增加对应门禁：

- `design_required`
- `full_test_required`
- `manual_acceptance_required`
- `rollback_plan_required`
- `scope_clarification_required`

## 原则

- 风险只能增加门禁，不能取消最小闭环。
- 用户可以要求更严格审查，但不能要求绕过已触发门禁。
- 简单任务快速完成，不等于无规范或无验证。
- 规则真相源是 `az-sdd-rules-config/flow.yaml`。
