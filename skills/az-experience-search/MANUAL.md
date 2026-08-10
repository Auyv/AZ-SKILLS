# az-experience-search 手工指南

1. 从问题中提取错误码、地址、工具和平台关键词。
2. 在经验仓库执行：

```powershell
python scripts/search_experiences.py --repo C:\path\to\jpz-experience-kb --query "exit code 139 vsyscall" --top-k 5
```

3. 读取排名最高的经验，比较当前环境、症状和适用边界。
4. 如果用户指定参考文档，先读取该文档的标签、症状和根因，再追加查询。
5. 未找到结果时记录“未命中”，继续正常排查。
6. 问题解决后更新命中经验，或创建新经验。
