# az-experience-publish 手工指南

1. 从来源经验复制并整理公开稿，禁止直接上传私有原文。
2. 删除内部项目、客户、账号、路径、IP、密钥、完整容器 ID 和受限材料。
3. 执行敏感信息扫描：

```powershell
python scripts/scan_sensitive.py publish/drafts/<文章>.md
```

4. 在经验仓库根目录 dry-run：

```powershell
lark-cli docs +create --as user --doc-format markdown --parent-token "<飞书文件夹URL>" --title "<标题>" --content @publish/drafts/<文章>.md --dry-run
```

5. 用户审核批准后，先校验经验已经合入远端 `master`：

```powershell
git fetch origin master
python scripts/check_publish_eligibility.py --repo . --experience experiences/<分类>/<经验>.md --base-ref origin/master
```

6. 只有校验成功，才移除 `--dry-run` 创建在线文档。
7. 只有退出码 0 且 JSON `ok == true` 才算成功。
8. 从最新 `master` 创建 `publish/<slug>` 分支，把文档 URL 回写到来源经验和 `publish/published.md`。
9. 经明确授权后推送该分支，并创建目标为 `master` 的合并请求。
