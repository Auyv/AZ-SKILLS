# azKits 脚本工具箱

azKits 是面向 Windows 测试场景的离线终端脚本工具箱。Go 版本通过 YAML 集中配置真实脚本，提供 TUI 斜杠命令以及 `list`、`run <alias>`、`check` 非交互命令；运行时不访问网络或 AI 服务。

> 当前 Go TUI 处于中文 IME 实机门禁阶段。旧 Python/Tkinter/JSON GUI 原型已经清理；项目运行入口统一为 Go EXE、Go 源码和 YAML。

## 快速开始

开发环境需要 Go 1.26.x：

```powershell
go run ./cmd/azkits --config examples/toolbox.yaml check
go run ./cmd/azkits --config examples/toolbox.yaml list
go run ./cmd/azkits --config examples/toolbox.yaml run demo-bat
go run ./cmd/azkits --config examples/toolbox.yaml
```

也可运行：

- `build_azKits.bat`：执行测试并构建 `dist/azkits.exe`。
- `run_azKits_go.bat`：优先运行已构建 EXE；不存在时使用 `go run`。

目标 PC 只需要 `dist/azkits.exe`、YAML 入口及其真实脚本，不需要安装 Go。目标脚本自身所需的 PowerShell、Python、Shell 等解释器仍需可用。

## YAML 配置

```yaml
version: 1
toolbox:
  name: 设备测试工具箱
  description: 初始化与回归测试入口
scripts:
  - id: device-init
    alias: device-init
    name: 设备初始化
    description: 初始化设备并检查连接
    path: scripts/device_init.ps1
    type: powershell
    enabled: true
    args: ["COM3", "115200"]
    cwd: scripts
    env:
      DEVICE_MODE: test
    encoding: utf-8
    interactive: true
```

约束：

- 入口只接受 UTF-8 `.yaml`、`.yml`，允许 UTF-8 BOM。
- `version` 当前只接受 `1`；未知字段会导致校验失败。
- 启用脚本的 `id`、`alias`、`name`、`description`、`path` 必须非空。
- `id` 和 `alias` 在单个入口内唯一。
- 相对 `path`、`cwd` 以入口 YAML 所在目录为基准。
- `args` 只能是字符串数组，运行时不能追加 YAML 外的自由参数。
- `type` 支持 `auto`、`bat`、`cmd`、`powershell`、`python`、`shell`、`exe`。
- `encoding` 支持 `utf-8`、`utf-8-sig`、`gbk`、`gb18030`。
- `interactive` 为可选布尔值，默认 `true`，脚本会在前台接管当前终端。无需标准输入且希望保持 TUI 可操作的脚本，应显式设为 `false` 使用后台模式；azKits 不扫描脚本内容猜测模式。

## TUI 键盘操作

- 输入 `/`：显示当前入口的已启用脚本。
- 继续输入：按别名、名称、简述过滤。
- `↑` / `↓`：移动选择，不越界。
- `Tab`：只补全，不执行。
- `Enter`：执行明确选中的脚本。
- 命令成功启动后输入框会清空；使用 `↑` / `↓` 可浏览并重复执行本实例的命令历史。
- `Esc`：关闭候选，不执行。
- `Ctrl+P`：脚本结束并返回主界面后，导出当前工具箱实例最近一次执行日志。
- `Ctrl+C`：退出当前实例。

导出文档使用 UTF-8 编码，默认保存在当前用户本地数据目录的 `azKits\logs` 子目录。工具会优先使用 Notepad++ 打开；不可用或启动失败时回退到 Notepad。编辑器均无法启动时，文档仍会保留，并在主界面显示可手动打开的完整路径。

默认情况下脚本以交互模式运行：TUI 会暂停并把当前终端交给脚本，因此 `pause`、文本输入和按键等待可以正常工作；脚本退出后自动恢复 TUI，并保留输出副本和最终状态。显式配置 `interactive: false` 的脚本仍在后台执行并持续写入日志视口。`run <alias>` 执行交互脚本时也会连接调用方的 stdin/stdout/stderr。

每次启动都是独立 OS 进程，只拥有自身入口、任务状态和日志，不使用跨实例共享状态。

## 开发验证

```powershell
go test ./...
go test -race ./...
go test -bench=. -benchmem ./internal/catalog
go build -trimpath -ldflags="-s -w" -o dist/azkits.exe ./cmd/azkits
```

正式人工验收遵循 `az-sdd/test/manuals/` 中的测试手册。
