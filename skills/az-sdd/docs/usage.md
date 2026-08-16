# AZ-SDD v4 使用指南

## 1. 开始使用

在项目目录中显式调用：

```text
/az-sdd-next
```

未初始化时，导航器会建议执行 `/az-sdd-init`。v4 项目会读取 baseline、开放 change、未关闭反馈和门禁问题，只给出一个下一步。

## 2. 单一自适应流程

不需要选择模式。所有任务先创建 change，AZ-SDD 自动评估风险。

```text
无风险 → change → 实现/证据 → 关键验收 → archive
有风险 → change → 条件设计/测试/人工验收/回滚 → archive
```

简单任务可以连续完成多个阶段；复杂任务由风险门禁自动展开。

## 3. 常用入口

| 目标 | 命令示例 |
|---|---|
| 初始化项目档案 | `/az-sdd-init` |
| 迁移 v3 项目 | `/az-sdd-init migrate --from v3 --to v4` |
| 创建单个变更规范 | `/az-sdd-spec 为登录功能创建 CHG-0001` |
| 拆分一批功能 | `/az-sdd-spec 按 AZ-SDD 拆分以下需求：...` |
| 创建技术设计 | `/az-sdd-arch CHG-0001` |
| 开始实现 | `/az-sdd-coder CHG-0001` |
| 批量实现已批准功能 | `/az-sdd-coder 批量开发所有已批准规范` |
| 人工验收 | `/az-sdd-tester CHG-0001` |
| 查看状态 | `/az-sdd-track` |
| 生成汇报 | `/az-sdd-track 生成项目汇报 HTML` |
| 闭环处理反馈 | `/az-sdd-feedback 登录失败需要纳入闭环` |
| 查询流程规则 | `/az-sdd-rules 为什么现在不能开发 0001` |
| 校验归档门禁 | `/az-sdd-rules change-validate CHG-0001` |
| 归档合并 baseline | `/az-sdd-track archive CHG-0001` |

## 4. 规范审核

规范必须回答：

1. 用户要获得什么结果？
2. 哪些内容不在范围内？
3. 如何判断完成？
4. 哪些条件需要自动化测试？
5. 哪些条件只能人工验收？
6. 是否涉及架构、部署、硬件或外部依赖？
7. 是否涉及多模块、公共 API、数据、安全、不可逆或测试不足？

不清楚时先澄清；规范完整后再批准。

## 5. 开发与验证

- coder 只消费已通过前置检查的 change；v3 项目继续消费已批准 spec。
- v4 自动化测试证据写入 change/evidence.md。
- tester 不重复完整自动化测试，只复用证据并执行必要的启动检查和人工验收。
- 测试失败返回开发阶段，记录失败原因，不绕过验证。
- archive 必须有测试证据、用户验收和无冲突 delta。

## 6. v4 归档安全

- `change-init`、`migrate`、`archive` 默认 dry-run，只有 `--apply` 才写入。
- MODIFIED/REMOVED 记录 baseline SHA-256，变化后拒绝归档。
- archive 前校验任务、证据和验收。
- archive 写入前备份 baseline 和 change，失败自动回滚。

## 7. 反馈闭环

只有用户显式要求纳入 AZ-SDD 反馈闭环时才创建反馈单。

```text
记录 → 澄清 → 分类 → 计划 → 修复/变更 → 验证 → 关闭
```

无法复现时标记“待复现”；需求变化较大时转成新 spec；未验证通过时不得标记“已解决”。

## 8. 同步到 Agent

在 AZ-SKILLS 仓库根目录执行：

```powershell
python sync_skills.py --dry-run
python sync_skills.py --skill az-sdd
```

或仅同步到指定目标：

```powershell
python sync_skills.py --to codex
```

实际目标由 `manifest.yaml` 决定。同步后新开会话，确认九个 `az-sdd-*` 技能均已加载。

## 9. 故障排查

| 问题 | 处理 |
|---|---|
| 找不到 AZ-SDD 技能 | 检查 manifest、运行 dry-run、再执行实际同步 |
| 项目没有 `az-sdd/` | 显式调用 `/az-sdd-init` |
| 状态不允许开发 | 检查 spec 是否批准、arch 是否缺失、是否存在阻塞 |
| STATUS 与规范不一致 | 运行规则检查并重建索引，不手工猜测状态 |
| 普通开发意外进入 AZ-SDD | 停止流程；只有显式 AZ-SDD/SDD 意图才应触发 |
| archive 报 baseline 冲突 | 基于最新 baseline 重放 delta，不覆盖冲突 |
