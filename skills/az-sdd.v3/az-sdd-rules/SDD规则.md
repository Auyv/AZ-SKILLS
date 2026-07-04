# SDD 规则总览

本文档是 az-sdd.v3 系统规则的索引文档，提供整体概览，详细规则内容链接到对应配置文件。

> v3 触发边界：AZ-SDD 是显式命令型流程，不依赖平台级自动 hook，也不接管普通开发对话。
> 用户显式调用 `/az-sdd-next` 或任何 `az-sdd-*` skill 后，当前 skill 才加载本规则中的前置检查。

---

## 目录结构

```
az-sdd-rules/
├── 规则总览.md           # 本文件，规则索引
├── 01_流程定义.md        # 流程规则概述
├── 02_命名规范.md        # 命名规范概述
├── 03_状态定义.md        # 状态规则概述
└── az-sdd-rules-config/  # 配置文件（机器可读）
    ├── flow.yaml         # 流程定义
    ├── naming.yaml      # 命名规范
    └── states.yaml      # 状态定义
```

---

## 一、状态定义

> 详细规则：[az-sdd-rules-config/states.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/states.yaml)

共 9 个状态，组成完整生命周期：

```
草稿 → 审核中 → 已批准 → 开发中 → 测试中 → 待验收 → 已完成 → 已发布
                                          ↑
                                      （可回退）
```

| 状态 ID | 中文名称 | 说明 |
|---------|---------|------|
| draft | 草稿 | 规范文档已创建，待审核 |
| reviewing | 审核中 | 规范正在审核，等待确认 |
| approved | 已批准 | 规范审核通过，可以开始开发 |
| developing | 开发中 | 正在按照规范实现 |
| testing | 测试中 | 自动化测试执行中 |
| acceptance | 待验收 | 等待人工测试验收 |
| completed | 已完成 | 人工验收通过 |
| released | 已发布 | 功能已正式发布 |
| blocked | 阻塞 | 遇到阻塞问题，需要解决 |

---

## 二、流程定义

> 详细规则：[az-sdd-rules-config/flow.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/flow.yaml)

### 主流程（完整开发周期）

```
init → spec → arch(可选) → coder → tester → track → feedback
```

| 阶段 | SKILL | 说明 |
|------|-------|------|
| init | az-sdd-init | 建立项目档案目录结构 |
| spec | az-sdd-spec | 编写功能规范，AI 提取技术约束 |
| arch | az-sdd-arch | 为复杂技术决策创建独立文档（可选） |
| coder | az-sdd-coder | 按照规范实现代码 |
| tester | az-sdd-tester | 执行测试 + 用户确认 + 状态更新 |
| track | az-sdd-track | 更新项目状态追踪 |
| feedback | az-sdd-feedback | 用户显式调用后的自动闭环编排 |

### 快速流程（小型功能/修复）

```
spec → coder → tester
```

适用于单一功能点，1-2天能完成的小型功能或紧急修复。

### 迭代流程（优化改进）

```
feedback → spec → coder → tester → track
```

仅在用户显式调用 feedback 后执行自动闭环编排。

---

## 三、命名规范

> 详细规则：[az-sdd-rules-config/naming.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/naming.yaml)

### 项目档案目录结构

```
{workspace}/
├── az-sdd/                              # 规范档案目录（仅文档）
│   ├── INIT.md          # 项目初始化信息（必填）
│   ├── STATUS.md        # 当前状态看板（必填）
│   ├── TRACK.md         # 历史事件日志（必填）
│   ├── SPEC.md          # 功能规范索引目录（必填）
│   ├── ARCH.md          # 技术规范索引目录（必填）
│   ├── FEEDBACK.md      # 用户反馈索引目录（必填）
│   ├── spec/            # 功能规范文档目录 *.spec.md
│   ├── arch/            # 技术规范文档目录 *.arch.md
│   ├── test/            # 测试相关文件目录
│   │   ├── cases/       # 测试用例
│   │   ├── reports/     # 测试报告
│   │   └── manuals/     # 操作手册
│   ├── reports/         # 对外汇报与分享报告
│   └── feedback/        # 用户反馈文档目录 FB-*.feedback.md
│
├── code/                                # 新项目推荐代码目录（已有项目沿用原结构）
│   ├── main.py                          # 主入口
│   ├── models/                          # 数据模型
│   ├── cli/                             # CLI 命令
│   ├── services/                        # 业务逻辑
│   └── tests/                           # 自动化测试
```

**目录原则**
- 禁止将业务代码文件放入 `az-sdd/` 目录
- 保持 `az-sdd/` 仅包含规范文档、测试档案和配置文件
- 新项目推荐使用 `code/` 作为代码根目录；已有项目沿用原有语言/框架目录结构

### 编号分配规则

| 编号段 | 用途 |
|--------|------|
| 0001-0009 | 全局技术规范（技术栈、数据模型等） |
| 0010-0099 | 核心功能 |
| 0100+ | 普通功能 |

---

## 四、SKILL 与状态对应关系

### az-sdd-spec

| 操作 | 初始状态 | 变更后状态 |
|------|---------|-----------|
| 新建规范 | — | 草稿 |

### az-sdd-coder

| 操作 | 初始状态 | 变更后状态 |
|------|---------|-----------|
| 开始开发 | 已批准 | 开发中 |
| 开发完成 | 开发中 | 测试中 |
| 测试通过 | 测试中 | 待验收 |

### az-sdd-tester

| 操作 | 初始状态 | 变更后状态 |
|------|---------|-----------|
| 验收通过 | 待验收 | 已完成 |

---

## 五、状态看板与事件日志

> 详细规则：[az-sdd-rules-config/states.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/states.yaml) 中的 `sync_rules` 配置

**核心原则**：`STATUS.md` 是当前态入口，`TRACK.md` 是历史事件日志。用户要一眼看全局时看 `STATUS.md`；需要追溯变化时再看 `TRACK.md`。

### 5.1 文档职责

| 文件 | 职责 | 写入原则 |
|------|------|----------|
| `STATUS.md` | 当前阶段、整体进度、当前焦点、阻塞项、未闭环反馈、下一步 | 每次阶段推进后优先更新 |
| `SPEC.md` | 功能规范索引 | 只保留功能列表、状态、链接、简短备注 |
| `ARCH.md` | 技术规范索引 | 只保留架构文档列表和关联功能 |
| `FEEDBACK.md` | 反馈索引 | 只保留反馈列表、状态、下一步动作和链接 |
| `TRACK.md` | 历史事件日志 | 只追加关键状态变化、阻塞、关闭、发布记录 |
| `az-sdd/coding/*.coding.md`、`az-sdd/test/...`、`feedback/*.feedback.md` | 执行证据 | 不作为全局状态入口 |

### 5.2 各 SKILL 的写入时机

| SKILL | 状态变更 | 必须更新 | 追加事件 |
|-------|---------|----------|----------|
| az-sdd-spec | draft → reviewing → approved | STATUS.md, SPEC.md | TRACK.md（关键事件） |
| az-sdd-coder | approved → developing → testing → acceptance | STATUS.md, SPEC.md, coding 证据 | TRACK.md（关键事件） |
| az-sdd-tester | acceptance → completed | STATUS.md, SPEC.md, tester 证据 | TRACK.md（验收/关闭） |
| az-sdd-feedback | 反馈推进或关闭 | STATUS.md, FEEDBACK.md, feedback 单 | TRACK.md（关闭/挂起/高优先级） |
| az-sdd-track | 查看或汇报 | STATUS.md | TRACK.md（仅用户要求记录事件时） |

### 5.3 检查清单

- [ ] `STATUS.md` 第一屏是否能看到当前阶段、当前工作、阻塞项和下一步
- [ ] `STATUS.md` 当前焦点是否链接到 spec、coder、tester 或 feedback 证据
- [ ] `SPEC.md`、`ARCH.md`、`FEEDBACK.md` 是否仍是轻量索引
- [ ] `TRACK.md` 是否只记录关键事件，而不是重复维护状态总览

### 5.4 常见问题

| 症状 | 原因 | 处理方法 |
|------|------|---------|
| 用户无法一眼判断下一步 | STATUS.md 未更新或当前焦点不清晰 | 先更新 STATUS.md，再补证据链接 |
| 多个文件都在维护完整状态 | 索引或日志承担了状态看板职责 | 将当前态收敛到 STATUS.md |
| TRACK.md 越来越像日报或看板 | 把历史日志当状态入口使用 | 保留关键事件，当前态迁移到 STATUS.md |

---

## 六、测试用例格式约定

规范文件中测试用例格式：

```markdown
## 测试用例

| 用例编号 | 用例名称 | 测试类型 | 验证结果 |
|---------|---------|---------|---------|
| MT001 | 界面布局验证 | 人工 | 待验证 |
```

- **用例编号格式**：MTxxx（如 MT001）
- **必须写入规范文件**：test 阶段会从规范文件读取测试用例

### 编码任务文档

> 详细规则：[az-sdd-rules-config/flow.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/flow.yaml) 中的 `coding_task_doc` 配置

每次触发 az-sdd-coder 时，**必须**创建编码任务计划文件：

| 要求 | 说明 |
|------|------|
| 保存位置 | `az-sdd/coding/{TIMESTAMP}-plan.coding.md` |
| 命名规则 | `{时间戳}-plan.coding.md`，如 `20260426_143052-plan.coding.md` |
| 必须包含 | ① 任务目标 ② 执行计划 ③ 功能实现前趋图 ④ 编码进度 |
| 触发条件 | **每次**触发 az-sdd-coder 时（不仅是批量模式） |

**四要素**：
1. **任务目标** - 本次编码任务要达成的目标
2. **执行计划** - 分解的任务步骤和执行顺序
3. **功能实现前趋图** - 展示涉及功能的依赖关系（含已完成）
4. **编码进度** - 实时更新每个步骤的执行状态、耗时

**编码完成后的动作**：
1. 自动更新涉及的功能规范状态（开发中 → 测试中）
2. 提示用户可以开始 test 流程

**与旧版 batch_plan_doc 的区别**：
- 旧版：仅批量模式使用，保存在 `az-sdd/coding.md`
- 新版：**每次** coding 触发，文件名带时间戳

---

## 八、配置文件索引

| 配置文件 | 内容 |
|----------|------|
| [flow.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/flow.yaml) | 流程定义及跳转规则 |
| [naming.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/naming.yaml) | 命名规范及编号分配 |
| [states.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/states.yaml) | 状态定义及转换规则 |
