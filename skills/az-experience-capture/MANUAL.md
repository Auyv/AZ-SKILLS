# az-experience-capture 手工指南

1. 确认问题已经解决，并保存退出码、产物或测试结果。
2. 在经验仓库执行关键词查重：

```powershell
rg -n "错误码|日志关键字|根因" experiences inbox
```

3. 若根因相同，更新已有经验；否则复制 `assets/experience-template.md`。
4. 未解决的问题放入 `inbox/`；已验证经验放入 `experiences/<分类>/`。
5. 填写现象、根因、排查思路、解决方法、验证与回退、适用边界。
6. 执行校验：

```powershell
python scripts/validate_experience.py <经验文件>
```

7. 检查敏感信息。未经明确确认，不执行 commit/push。
