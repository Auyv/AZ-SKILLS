# 技术规范索引

详细技术规范存放于 `az-sdd/arch/`，文件名遵循 `{ID}-{MODULE}-{NAME}.arch.md`。

## 项目级技术约束

- 首个发布目标为 Windows 10/11 amd64 离线终端程序。
- 统一使用 Go 1.26.x；TUI 基于 Bubble Tea v2 与 Bubbles v2。
- YAML 使用 `go.yaml.in/yaml/v4` 严格解析；运行时不下载依赖。
- 每个 YAML 入口对应一个独立进程和终端实例。
- 真实脚本通过显式 argv 与独立子进程执行，禁止解释任意自由命令。

| 编号 | 父规范 | 技术主题 | 关联功能 | 状态 | 规范 | 备注 |
|------|----------|----------|----------|------|------|------|
| 0010 | - | Go TUI 脚本工具箱运行架构 | 0010、0011、0012 | 已批准 | [查看规范](arch/0010-azKits-go-tui-runtime.arch.md) | FB-2026-002 交互子进程终端移交方案已批准 |
| 0014 | 0010 | 脚本收藏与最近使用 | 0014 | 已批准 | [查看规范](arch/0014-azKits-script-favorite-recent.arch.md) | 收藏持久化落盘 `%LOCALAPPDATA%\azKits\favorites.json` |
