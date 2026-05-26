# Skill 每日进化汇报 - 2026-05-26

## 本次处理的反馈单

- [feedback_20260526_110541_az-sdd.v3-az-sdd-init.md](./feedback_20260526_110541_az-sdd.v3-az-sdd-init.md)

## 修改点

| Skill | 修改内容 | 引用反馈单 | 备份位置 |
|------|---------|------------|----------|
| az-sdd.v3/az-sdd-init | 在执行步骤中增加项目根目录 `AGENTS.md` 检查；要求 `AGENTS.md` 作为项目说明和 AZ-SDD 使用约定的单一来源；已存在时只追加缺失的 AZ-SDD 约定，不复制原内容到 `az-sdd/init.md`。 | [feedback_20260526_110541_az-sdd.v3-az-sdd-init.md](./feedback_20260526_110541_az-sdd.v3-az-sdd-init.md) | `skill-feedback/backups/az-sdd.v3__az-sdd-init/20260526_111556` |
| az-sdd.v3/az-sdd-init | 将 `references/init.template.md` 与 `references/init.md.template` 调整为轻量档案入口：保留项目名、创建日期、项目类型、`AGENTS.md` 来源、技术栈摘要和档案入口，移除项目描述、定位、目标用户、里程碑等容易与 `AGENTS.md` 重复的长文本字段。 | [feedback_20260526_110541_az-sdd.v3-az-sdd-init.md](./feedback_20260526_110541_az-sdd.v3-az-sdd-init.md) | `skill-feedback/backups/az-sdd.v3__az-sdd-init/20260526_111556` |

## 非修改点

| Skill | 未修改原因 | 引用反馈单 | 给用户的正确使用方式 |
|------|------------|------------|----------------------|
| az-sdd.v3/az-sdd-init | 未采纳“无需单独创建 `az-sdd/init.md`”的完整方案。当前 AZ-SDD 规则守卫和测试流程仍把 `az-sdd/init.md` 视为必填档案入口，直接取消会扩大影响面，不符合“最小有用变更”原则。 | [feedback_20260526_110541_az-sdd.v3-az-sdd-init.md](./feedback_20260526_110541_az-sdd.v3-az-sdd-init.md) | 初始化后维护项目背景、定位、目标用户和仓库约定时，以根目录 `AGENTS.md` 为准；`az-sdd/init.md` 只作为 SDD 档案入口和索引。 |

## 风险与后续观察

- 其他 AZ-SDD 子技能中仍有少量“读取 `az-sdd/init.md` 获取项目信息”的描述。由于本次反馈只指向 init 技能，暂未跨技能调整；后续如果发现 spec/test 仍误读 `init.md` 为项目说明来源，应单独收集反馈后统一调整。
- 模板新增 `{AGENTS_PATH}` 占位符；执行技能时需要用项目根目录 `AGENTS.md` 路径填充。若执行器未做变量替换，应至少保留字面提示，不影响档案结构。
