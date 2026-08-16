# az-sdd-track 手动指南

## 输入

- 项目路径；
- 可选功能编号；
- 动作：status、feature、change、archive、rebuild、report 或 history。

## 步骤

1. 读取 `STATUS.md` 作为当前态入口。
2. v3 对照索引和功能文件；v4 汇总 baseline、开放 changes 和 archive。
3. 发现冲突时先报告，不直接覆盖。
4. rebuild 时根据制品重建索引和状态摘要。
5. archive 时先运行 `archive CHANGE_ID --dry-run`，门禁通过且确认后才用 `--apply`。
6. 归档结果必须包含 baseline 操作、archive 路径和备份路径。
7. report 时生成面向读者的汇报；TRACK 只追加关键事件。

## 验证

- 状态数量与索引一致；
- 未关闭反馈和阻塞项可见；
- 报告能在浏览器打开并打印。

## 常见错误

- 未初始化：`ERR_PROJECT_NOT_INITIALIZED`。
- 制品冲突：`ERR_ARTIFACT_CONFLICT`。
- 报告不可写：`ERR_WRITE_PERMISSION`。
