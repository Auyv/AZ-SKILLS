# 0010 交互脚本终端移交自动化测试报告

## 测试信息

| 字段 | 内容 |
|------|------|
| 关联反馈 | FB-2026-002 |
| 测试时间 | 2026-08-11 22:45:13 |
| 测试范围 | Config、Runner、TaskService、TUI、CLI、普通后台回归 |
| 结果 | 通过；自动化、正式构建及 MT007-MT009 人工验收全部通过 |

## 覆盖行为

- 省略 `interactive` 默认进入交互模式；显式 `false` 进入后台模式。
- 交互子进程连接 stdin/stdout/stderr，实时输出同时保留日志副本。
- Bubble Tea Exec 消息驱动的终端移交、TUI 恢复和任务状态收敛。
- CLI `run <alias>` 透传调用方 I/O。
- 普通后台任务、重复启动和配置边界保持不变。

## 验证证据

| 命令 | 结果 |
|------|------|
| `go test ./...` | 通过 |
| `go test ./... -cover` | 通过；terminal 85.9%，runner 79.7%，task 81.5%，CLI 74.0%，config 83.5% |
| `go test -race ./...` | 通过 |
| `go vet ./...` | 通过 |
| `go run ./cmd/azkits --config examples/toolbox.yaml check` | 通过；4 个启用脚本 |
| 替代路径 `go build` | 通过；`.ai_tmp/verification/FB-2026-002/azkits.exe`，5,076,992 字节 |
| `build_azKits.bat` | 通过；生成 `dist/azkits.exe`，5,076,992 字节 |
| `dist/azkits.exe --config examples/toolbox.yaml check` | 通过；4 个启用脚本 |

默认值变更后再次执行全量测试、竞态检测、静态检查、正式构建和配置检查，结果均通过。

## 人工验收结果

- MT007：通过；示例脚本的文本输入、两次按键等待和 TUI 恢复正常。
- MT008：通过；CLI 交互 I/O 与普通后台模式回归正常。
- MT009：通过；真实 `cfun.bat` 三次 `pause` 分别等待按键，ADB 流程和 TUI 恢复正常。
