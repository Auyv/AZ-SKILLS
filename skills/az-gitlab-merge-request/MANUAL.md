# az-gitlab-merge-request 手工指南

1. 在仓库中确认远端和分支：

```powershell
git remote -v
git fetch origin
git ls-remote --heads origin <源分支> <目标分支>
git rev-list --left-right --count origin/<目标分支>...origin/<源分支>
```

2. 登录目标 GitLab，打开项目的“新建合并请求”，选择源分支和目标分支。
3. 检查是否已有相同源/目标的开放 MR；有则复用，避免重复创建。
4. 填写标题和说明，默认不要选择“合并后删除源分支”。
5. 向发起人展示项目、分支、标题与说明，等待其明确回复“提交”。
6. 收到确认后才点击“提交合并请求”。不要点击“合并”。
7. 返回 MR 编号、URL、开放状态、提交数及流水线状态。
