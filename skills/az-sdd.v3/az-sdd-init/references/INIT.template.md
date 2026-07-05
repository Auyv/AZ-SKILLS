# {PROJECT_NAME}

## 项目基本信息

| 字段 | 内容 |
|------|------|
| 项目名称 | {PROJECT_NAME} |
| 创建日期 | {CURRENT_DATE} |
| 项目类型 | {PROJECT_TYPE} |
| 项目说明来源 | {AGENTS_PATH} |

## 项目说明

项目背景、定位、目标用户、仓库约定和 AZ-SDD 使用规则以项目根目录 `AGENTS.md` 为准。

本文件仅作为 AZ-SDD 档案入口，不重复维护 AGENTS.md 中的长文本说明。

## 技术栈摘要

{TECH_STACK}

## 档案入口

- 当前状态看板：`az-sdd/STATUS.md`
- 历史事件日志：`az-sdd/TRACK.md`
- 功能规范索引：`az-sdd/SPEC.md`
- 技术规范索引：`az-sdd/ARCH.md`
- 用户反馈索引：`az-sdd/FEEDBACK.md`

## 目录结构说明

```
az-sdd/
├── INIT.md          本文件，SDD 档案入口
├── STATUS.md        当前状态看板
├── TRACK.md         历史事件日志
├── SPEC.md          功能规范索引
├── ARCH.md          技术规范索引
├── FEEDBACK.md      用户反馈索引
├── spec/            功能规范文档 (*.spec.md)
├── arch/            技术规范文档 (*.arch.md)
├── test/            测试相关文件
│   ├── cases/       测试用例
│   ├── reports/     测试报告
│   └── manuals/     操作手册
├── reports/         对外汇报与分享报告
└── feedback/        用户反馈文档 (FB-*.feedback.md)
```

## 更新记录

| 日期 | 更新内容 | 更新人 |
|------|---------|--------|
| {CURRENT_DATE} | 初始化项目 | - |
