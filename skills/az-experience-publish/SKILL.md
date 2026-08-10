---
name: az-experience-publish
description: |
  当用户要求“发布成博客”“生成并发布飞书文章”“分享这条经验”时使用。读取标准经验，生成公开稿，扫描敏感信息，用户确认后通过 lark-cli 在配置的飞书文件夹创建在线文档，并回写 URL；禁止未经批准自动发布。
version: 1.0.0
inputs:
  required:
    - name: repo_path
      type: string
      description: 经验仓库本地路径。
      validation: 目录存在且包含 .experience/config.yaml。
    - name: experience_path
      type: string
      description: 要分享的经验文件，相对 repo_path 或绝对路径。
      validation: UTF-8 Markdown 文件存在。
  optional:
    - name: draft_path
      type: string
      description: 已审核公开稿；为空时根据经验生成。
      default: ""
    - name: folder_url
      type: string
      description: 飞书目标文件夹 URL/token。
      default: 从 .experience/config.yaml 读取
    - name: approved
      type: boolean
      description: 用户是否已逐篇批准公开稿。
      default: false
outputs:
  - name: result
    type: object
    description: 包含 status、timestamp、公开稿路径、飞书文档 token/URL 和回写状态。
    format: JSON 风格结构
  - name: publication_draft
    type: file
    description: 脱敏后的公开 Markdown。
    path: "{repo_path}/publish/drafts/<title>-飞书稿.md"
errors:
  - code: ERR_REPO_NOT_FOUND
    message: 经验仓库不存在或缺少配置。
    handling: 请求正确 repo_path。
  - code: ERR_EXPERIENCE_NOT_FOUND
    message: 来源经验不存在。
    handling: 停止并报告路径。
  - code: ERR_SENSITIVE_CONTENT
    message: 公开稿仍包含高风险敏感信息。
    handling: 展示命中项并禁止发布，等待人工处理。
  - code: ERR_NOT_APPROVED
    message: 用户尚未批准当前公开稿。
    handling: 只生成预览和 dry-run，不创建飞书文档。
  - code: ERR_LARK_AUTH
    message: lark-cli 用户身份或权限不可用。
    handling: 按 lark-shared 最小权限授权流程处理。
  - code: ERR_LARK_CREATE
    message: 飞书在线文档创建失败。
    handling: 保留本地稿和原始错误，不回写虚假 URL。
  - code: ERR_WRITEBACK
    message: 飞书创建成功但本地 URL 回写失败。
    handling: 返回远端 URL 和 partial 状态，提示手工回写。
---

# 经验分享与飞书发布

## 做什么

把私有经验改写为通用公开文章，经敏感信息扫描和人工批准后，使用 `lark-cli docs +create` 在目标飞书文件夹创建真正的在线文档。

## 何时使用

- 用户要求把某条经验发布成博客或飞书文章。
- 用户指定经验文件并明确要求分享。
- 维护任务推荐发布候选后，用户选择其中一条。

## 怎么做

1. 读取仓库 `AGENTS.md`、`.experience/config.yaml` 和来源经验。
2. 生成或读取 `publish/drafts/<title>-飞书稿.md`。
3. 删除 YAML frontmatter 后形成飞书正文，确保公开稿不依赖内部上下文。
4. 运行：

```powershell
python scripts/scan_sensitive.py <draft.md>
```

5. 命中高风险信息时返回 `ERR_SENSITIVE_CONTENT`，逐项展示并等待处理。
6. 展示最终公开稿差异。`approved=false` 时只执行 dry-run：

```powershell
lark-cli docs +create --as user --doc-format markdown --parent-token "<folder-url>" --title "<title>" --content @publish/drafts/<file>.md --dry-run
```

7. 用户批准后，在经验仓库根目录执行同一命令并移除 `--dry-run`。路径必须相对 cwd。
8. 以退出码 0 且 JSON `ok == true` 判断成功。
9. 把文档 URL 回写到来源经验 `published` 字段和 `publish/published.md`。
10. 未经 Git 授权不 commit/push。

## 安全边界

- 默认 `--as user`，因为目标是用户云空间。
- 不输出 access token、app secret 或设备码。
- 无法判断是否敏感时停止发布。
- 不把原始私有经验直接上传；只上传已审核公开稿。
- 飞书创建成功前不写入虚假 URL。

## 输出结构

```json
{
  "status": "success|partial|error",
  "timestamp": "ISO-8601",
  "data": {
    "draft": "...md",
    "lark_url": "https://...",
    "writeback": true
  }
}
```

## 文件结构

```text
az-experience-publish/
├── SKILL.md
├── MANUAL.md
├── scripts/scan_sensitive.py
├── assets/publication-template.md
└── evals/evals.json
```
