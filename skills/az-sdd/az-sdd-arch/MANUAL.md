# az-sdd-arch 手动指南

## 输入

- 项目路径和技术目标；
- 可选关联功能编号；
- 动作：create、update、review 或 validate。

## 步骤

1. 确认项目已初始化；v3 找到 spec，v4 找到 change。
2. 判断是否真的需要独立 arch；简单实现细节留在 spec 的影响提示。
3. 记录备选方案、取舍理由和最终决策。
4. 描述模块、接口、数据、部署、硬件和外部依赖；v4 写入 change/design.md。
5. 写清启动、关闭、验证、回退和风险条件。
6. 检查 arch 没有改变 spec 的产品验收口径。
7. v3 更新 `ARCH.md` 和 `STATUS.md`；v4 不直接修改 baseline。

## 验证

- 每项技术决策有理由；
- 运行和验证条件可执行；
- 关联 spec 可追溯。

## 常见错误

- spec 不存在：`ERR_ARTIFACT_NOT_FOUND`。
- 与 spec 冲突：`ERR_ARTIFACT_CONFLICT`。
- 无法写入：`ERR_WRITE_PERMISSION`。
