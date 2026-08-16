# azKits — Windows 离线终端脚本工具箱（案例实现）

azKits 是一个面向 Windows 10/11 amd64 的离线终端脚本工具箱：以单个 YAML 入口
集中配置多个真实脚本，通过 TUI 或非交互命令触发运行。离线可用，不依赖网络或
AI 服务。

技术栈：Go 1.26 · Bubble Tea v2（`charm.land/bubbletea/v2`）· Bubbles v2 ·
Lip Gloss v2 · YAML v4 严格解析（`go.yaml.in/yaml/v4`，`WithKnownFields`）。

## 目录结构

```
main.go                     CLI 入口（list / run / check / TUI）
console_windows.go          控制台代码页转换（GBK 控制台显示 UTF-8 不花屏）
internal/
  entry/                    YAML 入口加载与严格校验
  matcher/                  候选过滤、排序（收藏/最近使用/别名匹配）
  run/                      子进程构建、编码解码、前台执行
  store/                    收藏持久化（%AppData%\azkits\favorites.json）
  tui/                      Bubble Tea 应用（候选、历史、运行视图、IME 安全）
examples/
  demo.yaml                 示例入口
  scripts/                  演示脚本（GBK/UTF-8 批处理、Python、PowerShell）
```

## 构建与运行

```powershell
# 构建
go build -o azkits.exe .

# 交互式 TUI（需要终端，如 Windows Terminal / conhost）
.\azkits.exe examples\demo.yaml

# 非交互命令
.\azkits.exe examples\demo.yaml list          # 列出脚本（★ 表示收藏）
.\azkits.exe examples\demo.yaml run hello     # 运行脚本（按 alias）
.\azkits.exe examples\demo.yaml run gbkpy
.\azkits.exe examples\demo.yaml check         # 检查入口可加载性

# 收藏存储位置（跨终端实例一致，默认 %AppData%\azkits\favorites.json）
# 可用 --favorites-dir 覆盖（测试用）
.\azkits.exe --favorites-dir <dir> examples\demo.yaml list
```

## YAML 入口格式

```yaml
version: 1                 # 只接受 1，其他值校验失败
name: 演示工具箱
description: 描述
scripts:
  - id: hello              # 必填，单入口内唯一
    alias: hello           # 必填，单入口内唯一（忽略大小写）
    name: 你好脚本          # 启用脚本必填
    description: 输出问候   # 启用脚本必填
    path: scripts/hello.bat # 启用脚本必填；相对入口文件目录
    type: auto             # auto|bat|cmd|powershell|python|shell|exe
    encoding: gbk          # utf-8|utf-8-sig|gbk|gb18030（默认 utf-8）
    interactive: false     # 可选布尔，true 时挂接真实终端
    enabled: true          # 可选布尔，默认 true；false 为已禁用
    args:                  # 只能由字符串组成；运行时禁止追加自由参数
      - "--mode"
      - "fast"
```

严格校验：未知字段、非法 type/encoding、重复 id/alias、启用脚本必填字段为空、
args 含非字符串元素，均会阻塞加载（`check` 逐一报告）。

## 编码处理

- `encoding` 描述脚本标准输出/错误的字节编码；`gbk`/`gb18030` 运行时转 UTF-8，
  `utf-8-sig` 去掉 BOM。`run` 与 TUI 运行视图输出均经解码。
- 非交互命令输出到终端时，若控制台代码页为 936/54936，自动转码为控制台代码页，
  避免中文花屏；重定向到文件时保持 UTF-8。TUI 运行时自动切换到 UTF-8 代码页并
  在退出时恢复。

## TUI 键盘

| 按键 | 行为 |
|---|---|
| `/` | 显示候选列表（渐进过滤：完整别名 > 前缀 > 包含） |
| ↑ / ↓ | 有候选时移动选择；无候选时浏览命令历史 |
| Tab | 补全为选中项的别名，不执行 |
| Enter | 执行明确选中的脚本 |
| Esc | 关闭候选（不执行）；输入为空时退出 |
| Ctrl+F | 收藏 / 取消收藏选中脚本 |
| Ctrl+C | 运行中终止；输入时退出 |

候选列表排序：收藏置顶（收藏且最近使用的去重），其次按最近成功使用时间，
其余按匹配规则。每行显示收藏（★）/最近使用（↺）标识、别名、名称、简述与状态
（有效 / 文件缺失 / 已禁用）。

- 收藏：跨终端实例持久化到 `%AppData%\azkits\favorites.json`。
- 最近使用：仅当前 TUI 实例内存内，同一脚本只保留最近一次成功启动，重启清空。
- 降级：脚本被删除/重命名后，收藏与最近使用记录被过滤，不会导致启动失败；
  文件缺失的脚本在候选与运行入口均给出友好提示。

## 测试与验证

```powershell
# 全部单元测试
go test ./...

# 500 个启用脚本的候选更新性能（要求 ≤50ms，实测约 50µs）
go test ./internal/matcher/ -bench BenchmarkFilter500 -benchtime 1s

# 示例入口检查 / 列出 / 运行
.\azkits.exe examples\demo.yaml check
.\azkits.exe examples\demo.yaml list
.\azkits.exe examples\demo.yaml run hello      # GBK 批处理解码
.\azkits.exe examples\demo.yaml run utf8       # UTF-8 直通
.\azkits.exe examples\demo.yaml run args       # 固定参数传递
.\azkits.exe examples\demo.yaml run ps         # PowerShell
.\azkits.exe examples\demo.yaml run fail       # 退出码 3 透传（$LASTEXITCODE=3）
.\azkits.exe examples\demo.yaml run missing    # 文件缺失降级提示
.\azkits.exe examples\demo.yaml run off        # 已禁用提示
```

测试覆盖（66 个用例）：

- `internal/entry`：版本、未知字段、必填字段、id/alias 唯一、type/encoding
  白名单、args 严格字符串数组、auto 推断、禁用脚本、文件缺失降级、BOM、路径解析。
- `internal/matcher`：完整别名/前缀/包含排序、收藏置顶与去重、最近使用单条记录、
  收藏按最近使用排序、500 脚本性能 ≤50ms。
- `internal/store`：收藏增删改、跨实例持久化、损坏/旧版本文件降级。
- `internal/run`：cmd/python/powershell 命令构建、路径含空格、退出码、GBK/UTF-8/
  utf-8-sig 解码、参数传递、禁用/缺失脚本拒绝。
- `internal/tui`：IME 组词/选字/上屏不误执行、Tab 只补全、Esc 关闭、↑/↓ 选择、
  收藏切换、文件缺失不启动、失败不计入最近使用、历史浏览、入口级 Bubble Tea
  集成运行。
- `main`：CLI 参数、list 收藏标识、run 别名/退出码、check 退出码、GBK 控制台转码。
