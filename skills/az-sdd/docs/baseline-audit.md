# AZ-SDD v3.2 优化基线审计

## 审计范围

- 目录：`skills/az-sdd/`
- 技能数量：9
- 规则配置：`flow.yaml`、`states.yaml`、`naming.yaml`
- 审计目标：记录优化前规模、交付完整性和主要风险，供 v3.2 回归比较。

## 技能规模

| Skill | 优化前行数 | v3.2 目标上限 |
|---|---:|---:|
| az-sdd-coder | 1277 | 400 |
| az-sdd-tester | 622 | 350 |
| az-sdd-spec | 531 | 350 |
| az-sdd-rules | 529 | 250 |
| az-sdd-next | 482 | 250 |
| az-sdd-arch | 300 | 300 |
| az-sdd-feedback | 219 | 300 |
| az-sdd-track | 152 | 300 |
| az-sdd-init | 136 | 300 |

## 交付缺口

优化前九个 SKILL 均存在以下缺口：

- front matter 未声明 inputs；
- front matter 未声明 outputs；
- front matter 未声明 errors；
- 技能目录没有 `MANUAL.md`；
- 技能目录没有 `evals/evals.json`。

README 还引用了不存在的 `USAGE.md`、`COMPETITION_FORM.md`、`PROJECT_INTRO.md` 和 `az-sdd-install.py`。

## 规则风险

1. 状态和流程规则同时存在于 YAML 和多个 SKILL，存在漂移风险。
2. `flow.yaml` 允许“跳过 spec 直接 coding”，而 coder 前置条件要求已批准 spec。
3. 状态和索引主要由 Agent 手工同步，缺少确定性校验器。
4. 跨 spec、arch、任务、测试和代码的一致性检查未工具化。

## v3.2 验收指标

- README 内部链接 100% 有效；
- 9/9 SKILL 具备 inputs、outputs、errors；
- 9/9 技能具备 MANUAL 和至少 5 个 eval；
- 所有核心 SKILL 达到目标行数；
- 非法状态转换能被脚本拒绝；
- v3 项目档案继续可读；
- 普通开发请求不触发 AZ-SDD；
- dry-run 不修改项目文件。
