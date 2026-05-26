# Skill 每日进化工作单 - 2026-05-26

## 执行原则

1. 理性判断反馈是否为正向反馈，防止 skill 因用户个人意志偏离最初目的。
2. 坚持每个 skill 的单一职责原则，防止功能过度臃肿。
3. 每次修改建议保持最简原则，防止过度修改产生其他影响。

## 执行流程

1. 阅读本工作单和同目录下的反馈单。
2. 对每条反馈判断：是否正向、是否属于目标 skill 职责、是否需要修改。
3. 需要修改 skill 前，先运行 `python tools/skill_feedback/daily_evolution.py backup --skill <skill>`。
4. 修改后更新同目录 `evolution-report.md`，明确写入修改点和非修改点。

## 待处理反馈

### feedback_20260526_110541_az-sdd.v3-az-sdd-init.md

- Skill: `az-sdd.v3/az-sdd-init`
- 创建时间: 2026-05-26T11:05:41
- 反馈单: [feedback_20260526_110541_az-sdd.v3-az-sdd-init.md](./feedback_20260526_110541_az-sdd.v3-az-sdd-init.md)
- 痛点: 每次初始化后产生的init.md文件会和项目中AGENTS.md文件内容高度重合。这样很浪费token，也容易内容出现不统一。
- 场景: 项目A中AGENTS.md描述了基础功能是提交表单。执行 `/az-sdd-init` 后生成了内容基本一样的 `az-sdd/init.md`.后续用户在AGENTS.md中迭代了新的规范约束，但是执行 `/az-sdd-spec` 时没有携带新的规范约束要求。
- 期望: 执行 `/az-sdd-init` 后检查项目目录下是否存在AGENTS.md，如果没有则创建AGENTS.md，然后将之前的 `az-sdd/init.md` 内容整合到AGENTS.md文件中并描述az-sdd的规则。
无需单独创建 `az-sdd/init.md`

处理判断:
- 是否正向反馈: 待判断
- 是否符合单一职责: 待判断
- 建议动作: 待判断
