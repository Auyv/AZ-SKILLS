# 技术规范：Go TUI 脚本工具箱运行架构

## 基本信息

| 字段 | 内容 |
|------|------|
| 规范编号 | 0010 |
| 父规范编号 | 无 |
| 所属模块 | azKits / runtime |
| 关联功能 | 0010、0011、0012 |
| 创建日期 | 2026-08-11 |
| 状态 | 已批准 |

## 概述

本规范为已批准的 0010、0011、0012 提供统一实现契约。实现使用 Go 构建 Windows 离线 TUI 脚本工具箱，以单个控制台 EXE 提供交互式 TUI 和非交互命令；每次启动形成一个独立进程和实例上下文，不使用图形窗口、浏览器运行时、网络服务或 AI 服务。

该功能涉及配置解析、终端输入、中文 IME、稳定过滤、子进程生命周期、日志流、多实例隔离和跨脚本类型分派，属于复杂功能，必须依据独立技术规范开发。

## 关联功能与验收来源

| 来源 | 内容 |
|------|------|
| 功能规范 | 0010 - TUI 脚本触发与运行 |
| 功能规范 | 0011 - 多入口终端实例 |
| 功能规范 | 0012 - TUI 斜杠命令交互 |
| 功能目标 | 完全来自上述已批准 spec，本 arch 不新增或改写产品行为 |
| 验收标准 | 继续使用三份 spec 的验收编号；本 arch 只补充实现边界、运行条件与自动验证入口 |

## 实现约束摘要

| 要素 | 技术约束 |
|------|----------|
| 运行平台 | Windows 10/11，amd64 为首个发布目标 |
| 产品形态 | 仅控制台 TUI；同时提供 `list`、`run <alias>`、`check` |
| 配置入口 | UTF-8 YAML，路径基准为入口 YAML 所在目录 |
| 实例模型 | 一个 azKits 进程对应一个入口和一个终端实例 |
| 执行边界 | 只能执行当前有效配置内的脚本，不解释任意用户命令字符串 |
| 支持目标 | BAT、CMD、PowerShell、Python、Shell、EXE |
| 离线要求 | 运行期间无网络访问；依赖只在构建阶段解析并固化到 `go.sum` |
| 性能要求 | 500 个启用脚本时，单次候选更新不超过 50 ms |
| 交互脚本 | YAML 显式标记；运行时释放当前终端给子进程，结束后恢复 TUI |

## 技术选型

### 选型对比

| 层级 | 备选 | 结论 | 原因 |
|------|------|------|------|
| 开发语言 | Python、Go、Rust、C++ | Go 1.26 | 单 EXE、低运行资源、跨脚本进程管理直接，开发和维护成本低于 Rust/C++ |
| TUI 基础 | 自研终端引擎、tcell、Bubble Tea | Bubble Tea v2 | 提供 Windows 终端适配、事件循环、渲染与键盘事件；避免重写终端底层 |
| TUI 组件 | 全部自研、Bubbles v2 | Bubbles v2 + 业务组件 | 复用 textinput/list/viewport/key；匹配排序和执行状态机由 azKits 实现 |
| CLI 解析 | Cobra、标准库 | 标准库 `flag` + 显式子命令分派 | 子命令少，减少依赖和体积 |
| YAML | 自研子集、旧 go-yaml v3、YAML 官方维护分支 | `go.yaml.in/yaml/v4` | 官方 YAML 组织持续维护，支持 YAML 1.2 主体能力，避免维护自研解析器 |
| 进程执行 | Shell 字符串、`os/exec` argv | 标准库 `os/exec` | 保持参数边界，不拼接未经校验的任意命令字符串 |
| 中文编码 | 仅 UTF-8、自研转换、`x/text` | UTF-8 + `golang.org/x/text` 按配置解码 | 兼容常见 UTF-8、GBK/GB18030 脚本输出 |
| 实例隔离 | 单进程多入口、每入口独立进程 | 每入口独立进程 | 与“多终端实例”自然一致，故障、状态和日志边界清晰 |

### 最终依赖与许可证

| 技术 | 用途 | 许可证 | 开源/可商用 | 替代方案 |
|------|------|--------|-------------|----------|
| Go toolchain / standard library | 编译、CLI、进程、并发、测试 | BSD-3-Clause | 是 / 是 | Rust 标准库 |
| `charm.land/bubbletea/v2` | TUI 事件循环、终端控制、渲染 | MIT | 是 / 是 | tcell、Ratatui |
| `charm.land/bubbles/v2` | 输入、候选列表、日志视口、按键绑定 | MIT | 是 / 是 | 自研组件 |
| `go.yaml.in/yaml/v4` | YAML 解析 | MIT + Apache-2.0 | 是 / 是 | 自研受限 YAML、JSON |
| `golang.org/x/text` | GBK/GB18030 等输出解码 | BSD-3-Clause | 是 / 是 | 仅允许 UTF-8 |

依赖版本在编码阶段以稳定发布版本写入 `go.mod` 和 `go.sum`，禁止使用未固定提交或运行时下载依赖。Bubble Tea 与 Bubbles 使用 v2 主版本；YAML 使用官方建议的新项目 v4 路径。

## 架构设计

### 总体数据流

```text
命令行参数 / 终端入口输入
          │
          ▼
     CLI Router
          │
          ▼
 YAML Loader ──► Validator ──► ToolboxConfig / ScriptConfig
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
             非交互命令                    TUI AppModel
          list/run/check              输入/候选/状态/日志视口
                    │                           │
                    └─────────────┬─────────────┘
                                  ▼
                           ScriptService
                     Catalog + TaskRegistry
                                  │
                                  ▼
                         Platform Runner
                 BAT/CMD/PS/Python/SH/EXE
                                  │
                                  ▼
                    TaskEvent(stdout/stderr/state)
```

### 目录与模块划分

```text
cmd/azkits/
└── main.go                    进程入口、退出码、依赖组装

internal/
├── cli/                       参数与 list/run/check/tui 路由
├── config/                    YAML 读取、严格校验、路径归一化
├── catalog/                   有效脚本清单、索引、匹配与排序
├── task/                      任务注册、重复运行控制、事件模型
├── runner/                    跨脚本类型命令构造与子进程生命周期
├── terminal/                  TUI AppModel、输入、候选和日志视口
└── textcodec/                 UTF-8、GBK/GB18030 输出解码

testdata/                      有效/无效 YAML 与辅助测试脚本
examples/                      可交付示例入口和脚本
```

| 模块 | 职责 | 不允许承担的职责 |
|------|------|------------------|
| `cli` | 解析固定子命令并选择执行路径 | 不直接解析 YAML、不直接创建系统进程 |
| `config` | 读取 YAML、字段校验、路径归一化、生成不可变配置值 | 不运行脚本、不维护界面状态 |
| `catalog` | 建立别名索引、提供稳定过滤结果 | 不读取终端、不运行脚本 |
| `task` | 维护当前实例任务状态、拒绝重复启动、发布事件 | 不拼装平台命令 |
| `runner` | 按脚本类型构造 argv、创建和终止子进程、采集输出 | 不接受 YAML 外自由命令 |
| `terminal` | 将 Bubble Tea 消息映射为产品交互并渲染状态 | 不绕过 ScriptService 直接启动进程 |
| `textcodec` | 按配置解码子进程字节流 | 不决定脚本业务状态 |

## 配置模型与兼容策略

### YAML 根模型

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

### Go 数据模型

```go
type ToolboxConfig struct {
    Version int
    Source  string
    Toolbox ToolboxMeta
    Scripts []ScriptConfig
}

type ScriptConfig struct {
    ID          string
    Alias       string
    Name        string
    Description string
    Path        string
    Type        ScriptType
    Enabled     bool
    Args        []string
    CWD         string
    Env         map[string]string
    Encoding    string
    Interactive bool
}
```

### 校验规则

1. 入口只接受 `.yaml`、`.yml`，文件按 UTF-8（允许 BOM）读取。
2. `version` 当前只接受 `1`；未知版本明确失败，不做猜测性降级。
3. 顶层未知字段和脚本未知字段视为配置错误，避免拼写错误静默生效。
4. `toolbox.name`、`toolbox.description`、`scripts` 必须存在。
5. 启用脚本的 `id`、`alias`、`name`、`description`、`path` 必须非空；`id` 和 `alias` 在单入口内唯一。
6. `type` 只允许 `auto`、`bat`、`cmd`、`powershell`、`python`、`shell`、`exe`。
7. 相对 `path` 和 `cwd` 以入口 YAML 所在目录为基准，归一化后保留绝对路径供运行层使用。
8. `args` 始终是字符串数组；`env` 只接受字符串键值；任何用户运行时输入都不能追加到 argv。
9. `encoding` 首版允许 `utf-8`、`utf-8-sig`、`gbk`、`gb18030`；未知编码在 `check` 阶段失败。
10. `interactive` 为可选布尔值，默认 `true`；显式配置 `false` 时使用后台模式，不得扫描脚本内容或根据扩展名猜测交互模式。

当前 Python/JSON 文件属于旧原型，不作为新产品运行时兼容入口。编码阶段应把示例迁移为 YAML；在 Go 版本达到功能等价并通过测试前保留旧原型文件用于行为对照，不直接覆盖或删除。

## 服务接口

```go
type ConfigLoader interface {
    Load(path string) (ToolboxConfig, error)
}

type Catalog interface {
    Enabled() []ScriptConfig
    FindAlias(alias string) (ScriptConfig, bool)
    Match(query string) []Candidate
}

type ScriptService interface {
    Start(ctx context.Context, id string) error
    Stop(id string) error
    Reload(path string) error
    Snapshot() InstanceSnapshot
    Events() <-chan TaskEvent
}

type Runner interface {
    Start(ctx context.Context, cfg ScriptConfig, sink chan<- TaskEvent) (ProcessHandle, error)
}
```

接口只在进程内使用，不提供网络 API、RPC 或跨实例共享服务。

## 候选匹配与交互状态

### 匹配算法

1. 对用户已经确认上屏的查询文本去掉开头 `/`，英文使用 Unicode 小写比较，中文原样比较。
2. 对 `alias`、`name`、`description` 分别计算匹配等级。
3. 完整别名匹配等级最高，其次是任一字段前缀匹配，再其次是包含匹配。
4. 同一等级使用 YAML 原始序号稳定排序。
5. 过滤过程只访问当前实例的只读切片；500 条数据不使用 goroutine、网络或磁盘 I/O。

### TUI 状态机

```text
Browsing ──输入 /──► Filtering ──Tab──► Completed
    ▲                    │                 │
    │                    ├─↑/↓─► Filtering│
    │                    ├─Esc─────────────┘
    │                    └─无匹配──► NoMatch
    │                                      │
    └──────────── Esc / 执行结果 ◄─────────┘

Completed / Filtering ──Enter 且目标明确──► Starting ─► Running
```

- `Tab` 只更新输入值和当前选择，不产生 `Start` 命令。
- `Enter` 只有在完整唯一匹配或界面明确高亮候选时才调用 `ScriptService.Start`。
- `Esc` 关闭候选，不清除运行任务和日志。
- 候选列表与非交互命令共用 `Catalog`，禁止在 TUI 内复制第二套过滤或校验规则。

### 中文 IME 门禁

Bubble Tea/Bubbles 支持 Unicode，但 Windows 中文 IME 组合态必须在正式编码前用最小原型验证。原型覆盖 Windows Terminal、传统 Console Host、微软拼音的组词、翻页、选字、上屏、`Tab`、`Enter` 和退格。

实现必须满足：

1. 只用已经确认上屏的 Unicode 文本刷新候选。
2. 同一输入批次如果既包含文本提交又包含 `Enter`，该 `Enter` 只完成上屏，不执行脚本。
3. 执行必须来自上屏完成后的独立 `Enter` 事件。
4. 若 Bubble Tea 默认输入事件无法可靠区分，使用 Windows 控制台输入适配层生成专用 `TextCommitted` 与 `ExecuteRequested` 消息；不得删除或降低 spec 的 IME 验收项。

在上述原型通过前，不开始完整 TUI 页面开发。

## 子进程执行与重复运行控制

### 执行模式分流

| 模式 | 配置 | 终端与 I/O | TUI 行为 |
|------|------|------------|----------|
| 后台模式 | `interactive: false` | stdin 不连接；stdout/stderr 通过管道解码并进入日志视口 | TUI 持续运行，可浏览其他任务 |
| 交互模式 | `interactive: true` 或省略 | 当前终端 stdin/stdout/stderr 交给子进程，同时保留可回写的输出记录 | TUI 暂停；子进程退出后恢复 |

- TUI 交互模式使用 Bubble Tea v2 `Exec`/`ExecProcess` 语义释放并恢复终端，不允许后台 goroutine 与 TUI 同时读取 `os.Stdin`。
- Runner 复用同一命令构造逻辑，但将“后台管道执行”和“前台交互命令”拆为明确入口。
- TaskService 负责交互任务的 Starting/Running/Succeeded/Failed 状态、重复启动检查和结束回写；Terminal 不绕过服务直接决定任务状态。
- 交互脚本执行期间不能从当前 TUI 发起其他操作；其他已运行后台任务可继续执行，其事件在恢复后处理。
- 非交互 `run <alias>` 对交互脚本使用调用方提供的 stdin/stdout/stderr 并同步等待退出。
- 交互输出应在前台实时可见；TUI 恢复后至少保留最终状态、退出码和可查看的输出副本。

### 命令构造

| 类型 | argv 规则 |
|------|-----------|
| BAT/CMD | `cmd.exe /d /s /c <script-path> <fixed-args...>`，路径和参数作为边界明确的 argv 输入 |
| PowerShell | 优先 `pwsh.exe`，回退 `powershell.exe`；使用 `-NoLogo -NoProfile -File <path>` |
| Python | 使用可配置或 PATH 中的 Python 解释器，`-u <path>` |
| Shell | 使用 PATH 中的 `bash.exe` 或 `sh.exe`；不存在时明确失败 |
| EXE | 直接执行绝对路径和固定参数 |
| auto | 仅根据已校验扩展名映射到上述类型；未知扩展名按 EXE 尝试前必须通过文件存在校验 |

所有命令设置 `shell=false` 等价边界；除 BAT/CMD 必需的 `cmd.exe` 外，不把自由文本交给命令解释器。

### 任务注册与事件

```go
type TaskState string
const (
    Ready TaskState = "ready"
    Starting TaskState = "starting"
    Running TaskState = "running"
    Stopping TaskState = "stopping"
    Succeeded TaskState = "succeeded"
    Failed TaskState = "failed"
    Stopped TaskState = "stopped"
)

type TaskEvent struct {
    ScriptID string
    Kind     EventKind
    Stream   StreamKind
    Data     []byte
    PID      int
    ExitCode int
    Err      error
}
```

- `TaskRegistry` 以 `ScriptConfig.ID` 为键，`Starting`、`Running`、`Stopping` 均拒绝第二次启动。
- 注册和状态转换在单个互斥区内完成，避免 TUI 与非交互入口并发竞态。
- stdout/stderr 分别读取并标记来源，最终在实例日志中按事件到达顺序展示。
- 不使用 `bufio.Scanner` 的默认行长限制；采用有界块读取和流式字符解码。
- 日志视口使用有界缓冲，默认每实例最多保留 16 MiB；截断旧内容时显示一次明确提示，避免长时间脚本无限占用内存。
- Windows 进程使用独立进程组；停止先请求正常终止，超时后强制结束。具体停止能力取决于目标脚本类型，并向用户显示结果。

## 多实例隔离

1. 每次运行 `azkits.exe` 创建一个独立 OS 进程、一个 `ToolboxConfig`、一个 `TaskRegistry` 和一个日志缓冲。
2. 不实现单例锁、命名管道、共享内存或跨实例任务查找。
3. 两个实例可以加载同名集合；终端标题和页头同时显示集合名称与入口绝对路径。
4. 重载只替换当前实例的配置快照。存在运行任务时，不改变这些任务已经捕获的启动配置；新触发使用新快照。
5. 当前实例退出时只处理自身任务，不枚举或终止其他 azKits 进程。

## CLI 与退出码

```text
azkits.exe --config <entry.yaml>              # 进入 TUI
azkits.exe --config <entry.yaml> list
azkits.exe --config <entry.yaml> run <alias>
azkits.exe --config <entry.yaml> check
```

未提供 `--config` 且将进入 TUI 时，在当前终端提示用户输入 YAML 路径；空输入或取消键正常退出，不扫描目录、不打开图形选择器。

| 退出码 | 含义 |
|--------|------|
| 0 | 命令成功；`run` 的子脚本退出码为 0 |
| 2 | 参数、入口读取或配置校验错误 |
| 3 | 别名不存在、重复或脚本不可运行 |
| 4 | azKits 内部启动/日志/终止错误 |
| 5 | 用户取消入口选择 |
| 其他非零 | `run` 子脚本的非零退出码；与保留退出码冲突时输出原退出码并返回 4 |

`list` 和 `check` 的标准输出可供人阅读；错误写入 stderr。首版不承诺 JSON 输出协议。

## 构建与部署

### 构建约束

```powershell
$env:CGO_ENABLED='0'
go test ./...
go build -trimpath -ldflags="-s -w" -o dist/azkits.exe ./cmd/azkits
```

- TUI 必须绑定控制台，禁止使用 `-H windowsgui`。
- 发布物由 `azkits.exe`、入口 YAML 和真实脚本组成；目标 PC 不安装 Go、Python GUI、WebView 或 AI 运行时。
- Go 模块缓存只用于构建；发布后的程序不得下载依赖。
- 首个正式发布目标为 `windows/amd64`；其他平台另行验证后才能宣称支持。

### 技术质量预算

| 指标 | 目标 | 验证方式 |
|------|------|----------|
| Release EXE | 不超过 10 MiB（不含符号文件） | 构建后读取文件大小 |
| 单空闲实例工作集 | 不超过 20 MiB | Windows PowerShell `Get-Process` 稳定采样 |
| 空闲 CPU | 10 秒采样平均不超过 1% 单核 | Windows 性能计数器或进程采样 |
| 500 条候选更新 | 每次不超过 50 ms | Go benchmark + 人工连续输入 |
| 离线运行 | 无网络连接尝试 | 断网人工测试及运行时审计 |

体积和内存属于技术预算。如果依赖版本导致超限，先通过组件裁剪和构建分析优化；不得通过删除 spec 功能规避指标。

## 运行与验证条件

| 项目 | 内容 |
|------|------|
| 开发环境 | Windows 10/11、Go 1.26.x、可用 Windows Terminal 与传统 Console Host |
| 开发启动 | `go run ./cmd/azkits --config testdata/valid/toolbox.yaml` |
| Release 启动 | `dist\azkits.exe --config examples\toolbox.yaml` |
| 正常关闭 | TUI 中退出；存在任务时按产品提示处理当前实例任务 |
| 强制关闭验证 | `Ctrl+C`，程序恢复终端模式并只清理当前实例资源 |
| 自动验证入口 | `go test ./...` |
| 竞态验证 | `go test -race ./...` |
| 性能验证 | `go test -bench=. -benchmem ./internal/catalog/...` |
| 构建验证 | `go build -trimpath -ldflags="-s -w" -o dist/azkits.exe ./cmd/azkits` |
| 人工验收前置 | 有效/无效 YAML、中文路径、UTF-8/GBK 输出脚本、500 条脚本入口、两个独立终端 |

### 自动化测试范围

| 层级 | 必测内容 |
|------|----------|
| 配置单元测试 | 未知字段、版本、必填项、重复 ID/alias、中文路径、相对路径、编码 |
| Catalog 单元测试 | 完整/前缀/包含等级、稳定顺序、禁用过滤、500 条性能 |
| TUI Model 测试 | `/`、输入、上下键、Tab、Enter、Esc、无匹配、状态同步；使用消息驱动测试，不依赖真实终端 |
| Runner 单元测试 | 每类脚本的 argv 构造、路径和参数边界、交互模式分流，不执行任意自由命令 |
| 进程集成测试 | 辅助子进程的 stdout/stderr、stdin 回显、非零退出、长行、停止、重复启动、中文解码 |
| CLI 集成测试 | list/run/check 的 stdout、stderr 和退出码一致性 |
| 实例隔离测试 | 两个进程加载不同入口，状态、日志、重载和退出互不影响 |

### 人工验收映射

| 规范用例 | 技术验证重点 |
|----------|--------------|
| 0010 MT001-MT009 | TUI-only、清单边界、中文日志、CLI 一致性、重载、离线与交互脚本终端移交 |
| 0011 MT001-MT006 | 入口路径、终端选择、双进程隔离、中文相对路径和离线 |
| 0012 MT001-MT009 | 斜杠候选、稳定过滤、键盘状态机、IME 门禁、50 ms 性能 |

## 风险与决策

| 风险/决策 | 影响 | 处理方式 |
|----------|------|----------|
| Windows 中文 IME 事件可能被终端/TUI 框架合并 | 可能在上屏时误触执行 | 编码前原型门禁；必要时实现 Windows 输入适配层，不降低验收标准 |
| Windows Terminal 与传统 Console Host 能力不同 | 光标、颜色、按键事件表现可能不同 | 只依赖基础 Unicode/按键能力，双终端人工验证，渐进增强必须有回退 |
| BAT/CMD 必须经过 `cmd.exe` | 存在命令解释器边界风险 | 只传入配置中已校验的路径和固定参数，禁止运行时自由参数和拼接字符串 |
| PowerShell/Python/Shell 解释器可能缺失 | 目标脚本无法启动 | 启动前探测并明确报错；`check` 报告阻止运行的问题 |
| 子进程产生无限日志 | 内存持续增长 | 有界日志缓冲、分块读取、截断提示 |
| TUI 与交互子进程争抢 stdin | `pause`、`set /p` 等等待失效或按键被 TUI 消费 | 只允许显式交互模式，并用 Bubble Tea 正式 Exec 机制释放/恢复终端 |
| 交互脚本输出编码和日志副本不一致 | 前台可见内容与恢复后的日志可能不同 | 前台写入终端的同时按配置编码采集副本，自动化覆盖中文输出 |
| Bubble Tea/Bubbles 主版本升级 | API 或输入行为变化 | `go.mod` 固定版本；升级必须重新执行 TUI、IME 和性能测试 |
| YAML 1.1/1.2 隐式类型差异 | 配置值解释不一致 | 结构体强类型、版本字段、未知字段拒绝、示例只使用 YAML 1.2 明确值 |
| 旧 Python/JSON 原型与新 Go/YAML 并存 | 用户误用入口 | 用户已批准清理旧 `azKits.py`、`azKits.json` 和旧启动脚本；Go/YAML 为唯一产品入口 |

## 参考来源

- Bubble Tea v2：<https://github.com/charmbracelet/bubbletea>
- Bubbles v2：<https://github.com/charmbracelet/bubbles>
- YAML 官方 Go 实现：<https://github.com/yaml/go-yaml>
- Go：<https://go.dev/>

## 审核记录

| 日期 | 审核人 | 结果 | 说明 |
|------|--------|------|------|
| 2026-08-11 | Codex | 草稿 | 根据用户选择 Go 开发生成统一技术规范，等待用户审核 |
| 2026-08-11 | 用户 | 审核中 | 提交 0010 统一技术规范审核 |
| 2026-08-11 | 用户 | 已批准 | 批准 Go TUI 统一技术规范，作为 0010、0011、0012 的开发约束 |
| 2026-08-11 | 用户 | 需求变更 | FB-2026-002：增加显式交互脚本模式与 Bubble Tea 终端移交方案，回退草稿待审 |
| 2026-08-11 | 用户 | 默认值确认 | 交互模式默认启用；只有显式 `interactive: false` 才使用后台模式 |
| 2026-08-11 | 用户 | 已批准 | 批准 FB-2026-002 终端移交、I/O 分流和状态恢复方案 |
