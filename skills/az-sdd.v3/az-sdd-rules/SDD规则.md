# SDD 规则总览

本文档是 az-sdd.v2 系统规则的索引文档，提供整体概览，详细规则内容链接到对应配置文件。

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
init → spec → arch(可选) → coding → test → track → feedback
```

| 阶段 | SKILL | 说明 |
|------|-------|------|
| init | az-sdd-init | 建立项目档案目录结构 |
| spec | az-sdd-spec | 编写功能规范，AI 提取技术约束 |
| arch | az-sdd-arch | 为复杂技术决策创建独立文档（可选） |
| coding | az-sdd-coding | 按照规范实现代码 |
| test | az-sdd-test | 执行测试 + 用户确认 + 状态更新 |
| track | az-sdd-track | 更新项目状态追踪 |
| feedback | az-sdd-feedback | 处理用户反馈 |

### 快速流程（小型功能/修复）

```
spec → coding → test
```

适用于单一功能点，1-2天能完成的小型功能或紧急修复。

### 迭代流程（优化改进）

```
feedback → spec → coding → test → track
```

基于用户反馈的优化改进。

---

## 三、命名规范

> 详细规则：[az-sdd-rules-config/naming.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/naming.yaml)

### 项目档案目录结构

```
{workspace}/
├── az-sdd/                              # 规范档案目录（仅文档）
│   ├── init.md          # 项目初始化信息（必填）
│   ├── track.md         # 工作状态追踪记录（必填）
│   ├── SPEC.md          # 功能规范索引目录（必填）
│   ├── ARCH.md          # 技术规范索引目录（必填）
│   ├── FEEDBACK.md      # 用户反馈索引目录（必填）
│   ├── spec/            # 功能规范文档目录 *.spec.md
│   ├── arch/            # 技术规范文档目录 *.arch.md
│   ├── test/            # 测试相关文件目录
│   │   ├── cases/       # 测试用例
│   │   ├── reports/     # 测试报告
│   │   └── manuals/     # 操作手册
│   └── feedback/        # 用户反馈文档目录 FB-*.feedback.md
│
├── code/                                # 代码实现文件（统一存放位置）
│   ├── main.py                          # 主入口
│   ├── models/                          # 数据模型
│   ├── cli/                             # CLI 命令
│   ├── services/                        # 业务逻辑
│   └── tests/                           # 自动化测试
```

**⚠️ 强制规定：代码文件必须统一放在 `code/` 目录下**
- 禁止将代码文件放入 `az-sdd/` 目录
- 保持 `az-sdd/` 仅包含规范文档和配置文件
- 所有语言的项目统一使用 `code/` 作为代码根目录

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

### az-sdd-coding

| 操作 | 初始状态 | 变更后状态 |
|------|---------|-----------|
| 开始开发 | 已批准 | 开发中 |
| 开发完成 | 开发中 | 测试中 |
| 测试通过 | 测试中 | 待验收 |

### az-sdd-test

| 操作 | 初始状态 | 变更后状态 |
|------|---------|-----------|
| 验收通过 | 待验收 | 已完成 |

---

## 五、状态同步更新规则

> 详细规则：[az-sdd-rules-config/states.yaml](AZ-SKILLS/skills/az-sdd.v3/az-sdd-rules/az-sdd-rules-config/states.yaml) 中的 `sync_rules` 配置

**核心原则**：每次执行完流程、功能状态发生变化时，必须同步更新所有相关文档，防止出现文档内容不一致。

### 5.1 必须同步的文件

| 文件 | 路径 | 更新内容 | 触发条件 |
|------|------|---------|---------|
| 规范文件本身 | `az-sdd/spec/{ID}-{NAME}.spec.md` | 状态字段 | 任何状态变更 |
| 功能索引目录 | `az-sdd/SPEC.md` | 功能列表中的状态列 | 任何状态变更 |
| 批量开发计划 | `az-sdd/coding.md` | 当前编码进度情况 | 批量开发模式下 |
| 技术索引目录 | `az-sdd/ARCH.md` | 技术规范状态列 | 涉及技术规范变更 |
| 反馈索引目录 | `az-sdd/FEEDBACK.md` | 反馈处理状态列 | 涉及反馈处理 |
| 状态追踪记录 | `az-sdd/track.md` | 变更历史记录 | 任何状态变更 |

### 5.2 各 SKILL 的同步时机

| SKILL | 状态变更 | 必须更新的文件 |
|-------|---------|---------------|
| az-sdd-spec | draft → reviewing → approved | SPEC.md, track.md |
| az-sdd-coding | approved → developing → testing → acceptance | SPEC.md, coding.md, track.md |
| az-sdd-test | acceptance → completed | SPEC.md, coding.md, track.md |
| az-sdd-track | any → any | SPEC.md, ARCH.md, FEEDBACK.md, track.md |

### 5.3 同步更新检查清单

- [ ] SPEC.md 中该功能的状态列是否与规范文件中的当前状态一致
- [ ] coding.md 中的当前编码进度是否反映最新执行状态
- [ ] ARCH.md 中的技术规范状态是否与对应功能规范状态一致
- [ ] FEEDBACK.md 中相关反馈的处理状态是否已更新
- [ ] track.md 中是否已记录本次状态变更（时间、原因、变更前后状态）

### 5.4 常见不一致问题

| 症状 | 原因 | 处理方法 |
|------|------|---------|
| SPEC.md 显示已发布，但规范文件仍是草稿 | 未同步更新 | 立即同步所有相关文件 |
| coding.md 显示已完成，但 SPEC.md 还是开发中 | 批量开发计划与索引目录不同步 | 更新 SPEC.md 和 coding.md 使其一致 |
| ARCH.md 状态超前于 SPEC.md | 技术规范先于功能规范完成 | 确保技术规范状态不超过对应功能规范状态 |

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

每次触发 az-sdd-coding 时，**必须**创建编码任务计划文件：

| 要求 | 说明 |
|------|------|
| 保存位置 | `az-sdd/coding/{TIMESTAMP}-plan.coding.md` |
| 命名规则 | `{时间戳}-plan.coding.md`，如 `20260426_143052-plan.coding.md` |
| 必须包含 | ① 任务目标 ② 执行计划 ③ 功能实现前趋图 ④ 编码进度 |
| 触发条件 | **每次**触发 az-sdd-coding 时（不仅是批量模式） |

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
