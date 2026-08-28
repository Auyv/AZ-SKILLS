---
name: az-gitlab-merge-request
description: |
  当用户明确要求“将 <源分支> 合并到 <目标分支>”“创建从 A 到 B 的 GitLab 合并请求”时使用。
  核验分支与流水线、创建远端 GitLab 合并请求，但在点击最终提交按钮前必须取得用户当次确认；不得直接合并。
version: 1.0.0
inputs:
  required:
    - name: source_branch
      type: string
      description: GitLab 合并请求的源分支。
      validation: 非空、不得包含换行符，且远端分支存在。
    - name: target_branch
      type: string
      description: GitLab 合并请求的目标分支。
      validation: 非空、不得包含换行符，且远端分支存在且不同于 source_branch。
  optional:
    - name: repository
      type: string
      description: 本地仓库路径或 GitLab 项目 URL。
      default: 当前工作区 Git 仓库。
    - name: title
      type: string
      description: 合并请求标题。
      default: "feat: 合并 <source_branch> 到 <target_branch>"。
    - name: description
      type: string
      description: 合并请求说明；为空时依据提交和验证结果生成。
      default: ""。
    - name: delete_source_branch
      type: boolean
      description: 合并后是否删除源分支。
      default: false
outputs:
  - name: result
    type: object
    description: 包含 status、timestamp、项目、分支差异、流水线状态、确认状态和 MR URL。
    format: JSON 风格结构。
errors:
  - code: ERR_INVALID_BRANCH
    message: 源/目标分支为空、包含非法换行或二者相同。
    handling: 请求用户提供两个不同的有效分支名。
  - code: ERR_NOT_GIT_REPOSITORY
    message: 未找到可用 Git 仓库或 GitLab 远端。
    handling: 请求仓库路径或远端项目 URL。
  - code: ERR_SOURCE_BRANCH_NOT_FOUND
    message: 远端不存在源分支。
    handling: 停止创建并提示先推送或修正分支名。
  - code: ERR_TARGET_BRANCH_NOT_FOUND
    message: 远端不存在目标分支。
    handling: 停止创建并提示修正目标分支名。
  - code: ERR_GITLAB_AUTH
    message: GitLab CLI/API/浏览器均无法认证。
    handling: 提示用户完成 GitLab 登录或提供受限 token；不得输出凭据。
  - code: ERR_MR_ALREADY_EXISTS
    message: 同源分支、同目标分支的开放合并请求已存在。
    handling: 返回已有 MR URL，不重复创建。
  - code: ERR_USER_CONFIRMATION_REQUIRED
    message: 尚未取得本次最终提交确认。
    handling: 展示将提交的项目、分支、标题和说明，等待用户明确确认。
  - code: ERR_MR_CREATE_FAILED
    message: GitLab 拒绝创建合并请求或页面提交失败。
    handling: 保留原始错误，报告失败原因，不重试提交。
---

# GitLab 分支合并请求

## 做什么

把用户明确指定的源分支创建为目标分支的远端 GitLab 合并请求。先检查项目、分支、已有 MR、提交差异与可见流水线；生成标题和说明，最后等待用户确认再提交。此技能只创建 MR，绝不执行“合并”按钮。

## 何时使用

- 用户明确说“将 `xxx` 分支合并到 `yyy` 分支”。
- 用户要求“为 A 到 B 创建 GitLab 合并请求”。

不要在仅要求推送、切换分支、查看分支或直接合并代码时触发。

## 怎么做

1. 提取源分支和目标分支，运行 `scripts/validate_request.py` 校验；空值、相同值、包含换行符均返回 `ERR_INVALID_BRANCH`。
2. 在目标仓库确认 GitLab remote，使用 `git fetch` 与 `git ls-remote --heads` 检查两个远端分支。获取 `origin/<target>...origin/<source>` 的提交数和最近提交。
3. 使用已认证的 `glab`、GitLab API 或用户已登录的浏览器进入 MR 页面。若 CLI 不能用于自建 GitLab，允许改用用户已登录的浏览器；不要索取或输出密码、token。
4. 查询相同源/目标分支的开放 MR。存在时返回 URL 和 `ERR_MR_ALREADY_EXISTS`，不得创建重复项。
5. 填写标题和说明。默认保留源分支，除非用户明确要求合并后删除；不要自动指定评审人、标签或里程碑。
6. 在最终点击“提交合并请求 / Create merge request”之前，必须向用户清楚说明 GitLab 项目、源分支、目标分支、标题、是否保留源分支和说明摘要，并请求本次明确确认。
7. 仅在用户回复“提交”“确认提交”等明确指令后，执行最终提交。随后读取 MR 页面，返回 URL、编号、开放状态、提交数和流水线状态。

## 安全边界

- “创建 MR”是对第三方的外部提交；先前的泛化授权不能替代最终确认。
- 不点击合并、接受、删除分支或修改权限。
- 遇到登录页时让用户自行登录；登录后继续当前页面。
- MR 描述不得包含密码、token、私钥、设备日志原文或其他敏感信息。
- 未经用户另行授权，不执行 `git commit`、`git push`、强推或重置操作。

## 输出结构

```json
{
  "status": "ready|success|error",
  "timestamp": "ISO-8601",
  "data": {
    "project": "group/project",
    "source_branch": "feature/a",
    "target_branch": "master",
    "ahead_commits": 3,
    "pipeline_status": "passed|unknown",
    "confirmation_required": false,
    "merge_request_url": "https://gitlab.example/group/project/-/merge_requests/1"
  }
}
```

## 文件结构

```text
az-gitlab-merge-request/
├── SKILL.md
├── MANUAL.md
├── scripts/validate_request.py
└── evals/evals.json
```
