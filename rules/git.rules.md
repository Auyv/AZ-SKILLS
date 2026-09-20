# Git 操作规则

本规则适用于创建、切换、合并、提交、推送、重置或管理 Git 分支的任务。

## 授权与安全

- 未获明确同意不得执行 `git commit`、`git push`、强制推送或 `git reset --hard`。

## 分支命名

- AI 创建的个人任务分支采用语义格式 `<name>-<tag>-<date>@<from>`；文档中的 `tag[date]` 仅表示任务标签后追加日期，实际 Git 分支名不得使用方括号。
- `name` 固定为 `jpz_ai` 或 `jpz_codex`；`tag` 使用 lowerCamelCase（小驼峰）；`date` 使用 `YYMMDD`；`from` 为分叉来源分支。
- 示例：`jpz_ai-configDefaultPropMr2-260917@qzf_20260908`。
