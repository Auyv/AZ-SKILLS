# 0010 Go TUI IME 门禁前自动化报告

## 当前结论

P0-P2 以及 P3 的自动化状态机已完成；核心配置、Catalog、CLI、Runner、任务注册和最小 TUI 原型通过自动验证。完整 TUI 受 GATE-IME-001 约束，尚未进入 P4，功能状态保持“开发中”。

## 实现摘要

- Go 1.26 module 与固定依赖：Bubble Tea v2.0.8、Bubbles v2.1.1、YAML v4.0.0-rc.6、x/text v0.40.0。
- YAML 严格配置、入口相对路径、启用过滤、重复 ID/alias 和编码校验。
- `check`、`list`、`run <alias>` 及退出码边界。
- BAT/CMD、PowerShell、Python、Shell、EXE argv 构造和 stdout/stderr 解码。
- TaskRegistry 重复运行控制、取消、状态事件与 16 MiB 有界日志。
- 斜杠候选、中文过滤、稳定排序、方向键、Tab、Enter、Esc 的消息驱动模型。
- 独立进程入口模型；未实现共享内存、单例锁或网络服务。

## 自动化证据

| 项目 | 命令 | 结果 |
|------|------|------|
| 全量测试 | `go test ./...` | 通过 |
| 竞态测试 | `go test -race ./...` | 通过 |
| 静态检查 | `go vet ./...` | 通过 |
| 总覆盖率 | `go test -coverprofile .ai_tmp\coverage.out ./...` | 81.4% |
| 500 条匹配 | `go test -bench '.' -benchmem ./internal/catalog` | 55,513 ns/op，远低于 50 ms |
| Release 构建 | `build_azKits.bat` | 通过，5,047,808 字节，低于 10 MiB |
| CLI 烟测 | `check`、`list`、`run demo-bat` | 通过 |

## 已知风险

1. `go.yaml.in/yaml/v4` 尚无正式版，当前固定 `v4.0.0-rc.6`；首个正式版发布后需重新执行全量测试。
2. 中文 IME 组合态必须进行 Windows Terminal 与 Console Host 实机验证。
3. 空闲工作集、10 秒 CPU 采样、双终端实例和离线验证留待完整 TUI 后执行。
