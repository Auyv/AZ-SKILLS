# AZ-SDD v3 — 日常开发用规范驱动 Skills

AZ-SDD 是一组面向 AI 编程助手的规范驱动开发（Spec-Driven Development）skills。目标不是让流程变重，而是让日常开发中的需求、实现、测试、验收和反馈有记录、可追踪、可复用。

核心收益：

- 减少 AI 编程中的需求漂移和重复解释。
- 让开发前有清晰 spec，开发后有测试和验收记录。
- 把用户反馈转成可追踪的 bug、改进或新需求。
- 适合个人开发者、小团队、教学项目和需要轻量过程治理的 AI 开发场景。

## 推荐入口

日常使用优先从 `/az-sdd-next` 进入。它会读取项目状态，推荐下一步，并显式加载规则检查。

直接调用某个 skill 也可以，但必须带有明确 AZ-SDD/SDD 意图，例如：

```text
/az-sdd-spec 为登录功能创建规范
/az-sdd-coding 按已批准 spec 实现 0001
/az-sdd-test 验收 0001
```

普通地说“写代码”“跑测试”“修 bug”不应自动进入 AZ-SDD 流程。

## 流程强度

AZ-SDD 支持三档使用方式，按任务风险选择，不强迫所有任务走完整流程。

| 模式 | 适用场景 | 建议流程 |
|------|----------|----------|
| lite | 小修小改、一次性脚本、低风险功能 | spec 简版 + coding checklist |
| standard | 日常功能开发，默认推荐 | spec → coding → test → track |
| strict | 多模块、长期维护、交付给用户的功能 | spec → arch → coding → test → acceptance → feedback |

## Skill 体系（9 个）

| Skill | 命令 | 用途 |
|-------|------|------|
| az-sdd-next | `/az-sdd-next` | 读取状态并推荐下一步，日常主入口 |
| az-sdd-init | `/az-sdd-init` | 初始化 `az-sdd/` 档案目录 |
| az-sdd-spec | `/az-sdd-spec` | 把需求转成可开发、可测试的功能规范 |
| az-sdd-arch | `/az-sdd-arch` | 为复杂技术决策创建独立架构规范 |
| az-sdd-coding | `/az-sdd-coding` | 按已批准规范实现代码，支持批量开发计划 |
| az-sdd-test | `/az-sdd-test` | 生成验收工单，组织人工验收和测试结果归档 |
| az-sdd-track | `/az-sdd-track` | 查看和更新功能状态、进度、历史 |
| az-sdd-feedback | `/az-sdd-feedback` | 将用户反馈分类为 bug、改进或新需求 |
| az-sdd-rules | `/az-sdd-rules` | 规则检查清单和状态机定义 |

## 规则守卫的现实用法

不同 AI 平台未必支持一个 skill 自动拦截另一个 skill。v3 的约定是：

1. `/az-sdd-next` 是推荐入口，它负责读取状态并提示应加载的规则。
2. 每个 `az-sdd-*` skill 在执行前都要做前置检查：是否存在 `az-sdd/`、目标 spec 状态是否允许、是否需要 arch、是否会造成状态跳跃。
3. `/az-sdd-rules` 提供规则解释和故障排查，不依赖平台级自动 hook。

## 典型日常流程

```text
1. /az-sdd-init
2. /az-sdd-spec 描述一个功能或一组功能
3. /az-sdd-coding 按已批准规范实现
4. /az-sdd-test 做验收和测试归档
5. /az-sdd-track 查看进度
6. /az-sdd-feedback 记录用户反馈并触发下一轮迭代
```

## 状态定义

```text
草稿 → 审核中 → 已批准 → 开发中 → 测试中 → 待验收 → 已完成 → 已发布
```

`blocked` 可用于记录阻塞状态。状态变更必须同步更新 `SPEC.md`、相关 spec 文件和 `track.md`。

## 项目档案结构

```text
az-sdd/
├── init.md
├── track.md
├── SPEC.md
├── ARCH.md
├── FEEDBACK.md
├── spec/
├── arch/
├── test/
│   ├── cases/
│   ├── reports/
│   └── manuals/
└── feedback/
```

## 安装

```powershell
python az-sdd-install.py --cline
python az-sdd-install.py --claude
python az-sdd-install.py --dest C:\path\to\skills
```

使用 `--dry-run` 预览安装目标，使用 `--force` 覆盖已存在 skill。

## 不适用场景

- 已有严格工程流程且所有状态都在 Jira、Linear、CI、QA 系统中自动管理的大团队。
- 安全、合规、审计要求极高但尚未接入企业权限和审计系统的项目。
- 用户只想快速试验一小段代码，不希望产生任何项目档案。
