---
name: az-experience-capture
description: |
  当用户说“沉淀经验”“记录到经验库”“把这个问题总结成经验”，或一个问题已经解决并验证通过时使用。读取当前问题上下文，先在经验仓库查重，再生成或更新标准 Markdown 经验；未经确认不 commit/push。
version: 1.1.0
inputs:
  required:
    - name: repo_path
      type: string
      description: 经验仓库本地路径。
      validation: 目录存在，包含 AGENTS.md 和 experiences/ 或 inbox/。
    - name: problem_summary
      type: string
      description: 本次问题的现象、根因、解决方法和验证摘要，可来自当前对话。
      validation: 非空，必须说明问题是否已经解决。
  optional:
    - name: evidence
      type: string
      description: 日志、退出码、产物或测试结果。
      default: ""
    - name: title
      type: string
      description: 经验标题；未提供时根据根因生成。
      default: 自动生成
    - name: category
      type: string
      description: experiences 下的分类目录。
      default: troubleshooting
    - name: allow_git_write
      type: boolean
      description: 是否已明确授权 commit、push 和创建合并请求。
      default: false
    - name: base_branch
      type: string
      description: 经验合并目标分支。
      default: 从 .experience/config.yaml 读取，缺省为 master
outputs:
  - name: result
    type: object
    description: 包含 status、timestamp、新建/合并决策、经验路径、工作分支和合并请求状态。
    format: JSON 风格结构
  - name: experience_file
    type: file
    description: 新建或更新的 Markdown 经验。
    path: "{repo_path}/experiences/<category>/<title>.md 或 {repo_path}/inbox/<title>.md"
errors:
  - code: ERR_REPO_NOT_FOUND
    message: 经验仓库不存在或结构不完整。
    handling: 停止写入并请求正确 repo_path。
  - code: ERR_EMPTY_CONTEXT
    message: 问题摘要为空或无法判断解决结果。
    handling: 请求补充现象、结论和验证信息。
  - code: ERR_UNRESOLVED
    message: 问题尚未解决。
    handling: 仅保存到 inbox，不创建 verified 经验。
  - code: ERR_PERMISSION_DENIED
    message: 目标目录无写入权限。
    handling: 保留草稿内容并报告失败路径。
  - code: ERR_DUPLICATE_REVIEW
    message: 存在高度相似经验，无法自动决定合并。
    handling: 展示差异并等待用户选择新建或合并。
  - code: ERR_GIT_NOT_AUTHORIZED
    message: 未获得 Git commit/push 授权。
    handling: 只完成工作区文件，不执行 Git 外部写入。
  - code: ERR_PROTECTED_BRANCH
    message: 当前位于 master/main，禁止直接新增或修改经验。
    handling: 从最新 master 创建 experience/<slug> 分支后重试。
  - code: ERR_INVALID_WORK_BRANCH
    message: 当前分支不符合 experience/<slug> 规范。
    handling: 切换到正确命名的经验工作分支。
  - code: ERR_MERGE_REQUEST_REQUIRED
    message: 经验尚未通过合并请求进入 master。
    handling: 返回分支和目标分支，等待合并请求完成；不得标记为可发布。
---

# 经验积累

## 做什么

把已经解决的问题转化为可检索、可验证的标准经验。先保存事实，再提炼根因；创建前必须查重。

## 何时使用

- 用户明确要求沉淀、记录或总结经验。
- 用户确认问题解决、构建通过或验证成功。
- 当前任务产生了反直觉根因或可复用排查方法。

问题未解决时只记录到 `inbox/`，不要伪造 verified 经验。

## 怎么做

1. 读取仓库 `AGENTS.md` 和 `.experience/config.yaml`。
2. 同步 `master`，创建 `experience/<slug>` 分支；运行 `scripts/check_capture_branch.py --repo <repo_path>`。在 `master/main` 或命名不合规的分支上停止写入。
3. 从当前对话提取：现象、环境、事实、推断、根因、修复、验证、回退和适用边界。
4. 用 `rg` 搜索错误码、日志关键字、工具名、根因和标签。
5. 读取最相似的候选经验：
   - 同一根因：更新已有经验并追加验证信息。
   - 相似但边界不同：展示差异，等待用户决定。
   - 没有命中：创建新经验。
6. 根据 `assets/experience-template.md` 生成草稿。
7. 已解决且有证据时写入 `experiences/<category>/`；否则写入 `inbox/`。
8. 运行 `scripts/validate_experience.py <file>`。
9. 返回文件路径、新建/合并理由、工作分支、目标分支和敏感信息提示。
10. 只有 `allow_git_write=true` 且用户明确同意时才能 commit、push 并创建目标为 `master` 的 Gitee 合并请求。
11. 合并前返回 `ERR_MERGE_REQUEST_REQUIRED` 或 `merge_request_status=pending`，明确该经验不可发布；合并后由发布技能独立校验 `origin/master`。

## 输出约束

返回结构必须包含：

```json
{
  "status": "success|partial|error",
  "timestamp": "ISO-8601",
  "data": {
    "decision": "created|merged|inbox|needs_review",
    "experience_file": "...",
    "similar_files": [],
    "validation": "...",
    "work_branch": "experience/<slug>",
    "base_branch": "master",
    "merge_request_status": "not_created|pending|merged"
  }
}
```

## 文件结构

```text
az-experience-capture/
├── SKILL.md
├── MANUAL.md
├── scripts/check_capture_branch.py
├── scripts/validate_experience.py
├── assets/experience-template.md
└── evals/evals.json
```
