# Skill 每日进化工作单 - 2026-06-03

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

### feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md

- Skill: `hq_tester_skills/hq_adb_srtget`
- 创建时间: 2026-06-02T16:52:28
- 反馈单: [feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md](./feedback_20260602_165228_hq_tester_skills-hq_adb_srtget.md)
- 痛点: 经常不知道srt id是多少。所以我希望可以通过一张表@"C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\SRT常用属性.md"帮助用户通过自然语言来查询。
- 场景: 用户：帮我通过srt系统查iccid的值
AI：不知道iccid对应的srt id是多少，无法进行查询
- 期望: 用户：帮我通过srt系统查iccid的值
AI：不知道iccid对应的srt id是多少，我去查询下常用属性。iccid对应srt id 306， 我将执行 `srtget 306` 进行查询

处理判断:
- 是否正向反馈: 待判断
- 是否符合单一职责: 待判断
- 建议动作: 待判断
