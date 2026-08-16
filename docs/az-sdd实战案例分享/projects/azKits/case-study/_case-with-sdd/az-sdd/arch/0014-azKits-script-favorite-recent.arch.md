# 技术规范：脚本收藏与最近使用

## 基本信息

| 字段 | 内容 |
|------|------|
| 规范编号 | 0014 |
| 父规范编号 | 0010 |
| 所属模块 | azKits / favorite |
| 关联功能 | 0014 |
| 创建日期 | 2026-08-12 |
| 状态 | 草稿 |

## 概述

本规范为已批准的 0014「脚本收藏与最近使用」提供实现契约。在既有 0010 统一 Go TUI 架构之上，新增收藏持久化存储与最近使用跟踪两个能力，并接入斜杠候选排序与 `list` 输出。

实现要点：

1. 收藏按用户偏好落盘，跨终端实例一致，脚本被删除或重命名时自动降级。
2. 最近使用仅当前实例内存态，重启后为空。
3. 候选过滤仍只由 `catalog` 完成；收藏/最近使用只影响候选的排序（置顶），不改变匹配规则和过滤范围。
4. 收藏切换通过新增斜杠命令 `/fav <alias>` 完成，不新增键盘事件，不与脚本别名过滤冲突。

## 关联功能与验收来源

| 来源 | 内容 |
|------|------|
| 功能规范 | 0014 - 脚本收藏与最近使用 |
| 功能目标 | 完全来自 spec，本 arch 不新增或改写产品行为 |
| 验收标准 | 继续使用 spec 的 [001]-[019] 与 MT001-MT013；本 arch 只补充实现边界、存储方案与验证入口 |

## 技术决策

### 决策 1：收藏持久化存储位置与格式

| 方案 | 说明 | 结论 |
|------|------|------|
| 入口 YAML 同目录文件 | 便于随工具箱复制携带 | 否：工具目录常为共享/只读/受版本控制，写入易失败 |
| 用户环境目录文件 | `%LOCALAPPDATA%\azKits\` 下持久化 | 是：用户偏好属于用户而非工具箱，写入权限可靠，符合 Windows 惯例 |

**最终决策**：

- 存储路径：`%LOCALAPPDATA%\azKits\favorites.json`。
- 文件格式：UTF-8 JSON，带 `version` 字段，便于未来格式迁移。

```json
{
  "version": 1,
  "favorites": {
    "C:\\tools\\device\\entry.yaml": ["device-init", "regression"]
  }
}
```

- 键为入口 YAML 的绝对路径，值为该入口下收藏的脚本 `id` 列表。
- 收藏以脚本 `id` 为标识而非 `alias`：`id` 是稳定标识，脚本改名后收藏仍跟随；脚本被删除后条目自动降级隐藏。

### 决策 2：异常与迁移方案

| 异常 | 处理方式 |
|------|----------|
| 文件不存在 | 视为空收藏，正常启动 |
| 文件损坏或无法解析 | 将原文件改名为 `favorites.json.bad-{时间戳}` 备份，按空收藏启动，TUI 一次性提示，不崩溃 |
| `version` 非 1 | 同上，备份后按空收藏启动，不猜测性迁移 |
| 写入失败 | 保留内存态继续本次实例使用，提示一次；下次变更时重试写入 |
| 脚本被删除/重命名 | 候选渲染时按当前有效清单过滤，失效 id 不显示、不参与运行 |
| 多实例并发写 | 原子写入（临时文件 + 重命名），最后写入生效；不引入锁服务 |

### 决策 3：最近使用

- 仅当前实例内存态，数据结构为带最近时间的有序 id 列表，同一脚本重复启动时移到最前。
- 更新时机与 0012「成功启动后清空输入行」相同：脚本启动成功那一刻记录。
- 上限 30 条，超出时淘汰最旧。
- 不落盘、不跨实例。

### 决策 4：候选排序接入

- 过滤仍由 `catalog.Match` 完成（完整 > 前缀 > 包含 > YAML 稳定序），不改动过滤规则。
- TUI 渲染时对匹配结果做展示级置顶排序，优先级：

```
1. 收藏且最近使用的脚本        （按最近使用时间倒序）
2. 仅收藏的脚本               （按 YAML 顺序）
3. 仅最近使用的脚本           （按最近使用时间倒序）
4. 其余匹配脚本               （catalog 原始顺序）
```

- 收藏且最近使用的脚本只出现一次，去重后计入第 1 组。
- 空查询时 catalog 返回全部启用脚本，置顶规则同样适用。
- 置顶排序只使用内存中的收藏集合与最近使用集合，不使用磁盘 I/O、网络或 goroutine，满足 500 条候选 50ms 预算。

### 决策 5：`/fav` 命令识别

- 输入行以 `/fav `（含空格）开头，或恰好为 `/fav` 时识别为收藏命令，不再作为脚本过滤。
- 其余以 `/` 开头的输入继续按既有脚本过滤规则处理，避免与别名以 `fav` 开头的脚本冲突。
- `/fav <alias>` 中的 `alias` 必须在当前入口启用脚本中唯一匹配，否则提示无效且不改变任何收藏状态。

## 架构设计

### 总体数据流

```text
                    ┌────────────────────────────┐
                    │  internal/favorite         │
                    │  FavoritesStore(落盘)      │
                    │  RecentTracker(内存)       │
                    └───────────┬────────────────┘
                                │ 收藏集合 / 最近使用集合
                                │
   TUI 输入 /fav <alias>        │
        │                       │
        ▼                       ▼
  terminal.AppModel ──► Catalog.Match(query) ──► 匹配候选
        │                       │
        │                       │ 追加置顶排序(收藏/最近使用)
        │                       ▼
        │                CandidateView(置顶规则)
        │                       │
        │                       │ Enter 启动成功
        │                       ▼
        │                ScriptService.Start ──► RecentTracker.Record(id)
        │                       │
        └── FavoritesStore.Toggle(entry, id) ──► 原子写盘
```

### 模块划分

新增模块 `internal/favorite`：

| 模块 | 职责 | 不允许承担的职责 |
|------|------|------------------|
| `favorite` | 收藏文件读写、原子写盘、Toggle/IsFavorite、损坏降级；最近使用内存跟踪 | 不读取终端、不运行脚本、不修改 YAML |
| `catalog` | 保持只读匹配与稳定排序（沿用 0010） | 不感知收藏/最近使用 |
| `terminal` | 候选置顶排序渲染、`/fav` 命令解析与反馈 | 不绕过 catalog 过滤，不重复第二套校验 |
| `cli` | `list` 输出收藏标识；`run <alias>` 不变 | 不直接写收藏文件业务逻辑之外的内容 |

### Go 数据模型

```go
type FavoritesFile struct {
    Version   int                 `json:"version"`
    Favorites map[string][]string `json:"favorites"` // 入口绝对路径 -> 脚本 id 列表
}

type FavoritesStore struct {
    mu   sync.Mutex
    path string
    data map[string]map[string]bool // 入口路径 -> id 集合
}

type RecentTracker struct {
    mu    sync.Mutex
    order []string
    limit int
}
```

接口（仅在进程内使用，无网络 API）：

```go
func LoadFavorites(path string) (*FavoritesStore, error)
func (s *FavoritesStore) IsFavorite(entryPath, id string) bool
func (s *FavoritesStore) Toggle(entryPath, id string) (bool, error)
func (s *FavoritesStore) Save() error

func NewRecentTracker(limit int) *RecentTracker
func (r *RecentTracker) Record(id string)
func (r *RecentTracker) List() []string
```

## 运行与验证条件

| 项目 | 内容 |
|------|------|
| 开发启动 | `go run ./cmd/azkits --config testdata/valid/toolbox.yaml` |
| 收藏文件位置 | `%LOCALAPPDATA%\azKits\favorites.json` |
| 自动验证入口 | `go test ./...`、`go test -race ./...` |
| 性能验证 | `go test -bench=. -benchmem ./internal/catalog/...`；0014 置顶排序纳入 catalog benchmark 或同量级 micro-benchmark |
| 构建验证 | `go build -trimpath -ldflags="-s -w" -o dist/azkits.exe ./cmd/azkits` |
| 人工验收前置 | 有效/无效 YAML、可收藏脚本、多实例、重启、删除/重命名脚本、500 条入口、离线环境 |

### 自动化测试范围

| 层级 | 必测内容 |
|------|----------|
| favorites 单元测试 | 加载/保存/原子写、损坏文件降级、version 非 1 降级、Toggle 可逆、按入口隔离、写入失败不崩溃 |
| recent 单元测试 | 记录顺序、去重上移、容量淘汰、实例隔离（构造独立实例） |
| terminal 排序测试 | 置顶优先级、去重、空查询与非空查询、失效 id 不显示 |
| CLI 测试 | `list` 收藏标识；`run <alias>` 对收藏/最近使用脚本正常 |
| 进程集成测试 | 两个进程同一入口收藏一致；重启后收藏保留、最近使用为空；并发写入不损坏文件 |

### 人工验收映射

| 规范用例 | 技术验证重点 |
|----------|--------------|
| 0014 MT001-MT013 | `/fav` 切换与反馈、持久化一致性、置顶去重、最近使用隔离、失效降级、空态、性能离线、中文 IME |

## 风险与决策

| 风险/决策 | 影响 | 处理方式 |
|----------|------|----------|
| 收藏以 `id` 标识，脚本改名后 `alias` 变了 | 收藏应跟随脚本而非别名 | `id` 为稳定主键；改名不改 id 时收藏保留 |
| 多实例并发写 favorites.json | 可能丢失最后一次写入 | 原子写 + 最后写入生效，容忍低频冲突，不引入锁服务 |
| 收藏文件损坏 | 启动失败风险 | 备份为 `.bad-{时间戳}` 后按空启动并提示一次 |
| `%LOCALAPPDATA%` 不可写 | 收藏无法落盘 | 保留内存态继续使用并提示，不阻断运行 |
| 置顶排序拖慢过滤 | 500 条预算 50ms | 只做内存排序，无磁盘/网络/goroutine；benchmark 验证 |
| `/fav` 与别名冲突 | 脚本别名以 fav 开头时误判命令 | 仅 `/fav ` 前缀或完全等于 `/fav` 时识别为命令，其余按脚本过滤 |

## 备注

- 旧 Python/JSON 原型与本功能无关，不纳入收藏迁移范围。
- 本 arch 不改变 0010 的模块边界；`catalog` 保持无收藏/最近使用感知的只读匹配。

## 审核记录

| 日期 | 审核人 | 审核结果 | 说明 |
|------|--------|----------|------|
| 2026-08-12 | 用户 | 提交审核 | 0014 技术规范进入审核 |
