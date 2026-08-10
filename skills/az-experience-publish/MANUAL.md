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

5. 用户审核批准后，移除 `--dry-run` 创建在线文档。
6. 只有退出码 0 且 JSON `ok == true` 才算成功。
7. 把文档 URL 回写到来源经验和 `publish/published.md`。
