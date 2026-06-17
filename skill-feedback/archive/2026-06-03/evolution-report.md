# Skill 每日进化汇报 - 2026-06-03

## 本次处理的反馈单

- [feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md](./feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md)

## 修改点

| Skill | 修改内容 | 引用反馈单 | 备份位置 |
|------|---------|------------|----------|
| `hq_tester_skills/hq_adb_srtget` | 增加“通过自然语言属性名称确定 SRT ID”的触发与执行规则：当用户只说 ICCID、IMEI、SIM卡状态、网络信号等属性名时，先查常用列表；未覆盖时读取 `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\SRT常用属性.md`，匹配后说明依据并执行 `adb shell srtget <id>`。同时明确查表仅服务于 `srtget` / `srtset` 的 ID 确定，不扩展为通用文档检索。 | [feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md](./feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md) | `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\skill-feedback\backups\hq_tester_skills__hq_adb_srtget\20260603_010534` |

## 非修改点

| Skill | 未修改原因 | 引用反馈单 | 给用户的正确使用方式 |
|------|------------|------------|----------------------|
| - | 本次没有判定为 no-change 或 defer 的反馈项。 | - | - |

## 风险与后续观察

- 外部表 `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\SRT常用属性.md` 位于技能目录之外；本次仅在技能中引用该既有知识表，没有复制或同步该文件。若该表移动或改名，后续需要同步更新技能说明。
- 自然语言匹配可能出现多个候选 ID；技能已要求列出候选并请用户确认，避免误查。
