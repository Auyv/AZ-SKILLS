# 操作手册：azKits 0010-0014（含脚本收藏与最近使用）

## 构建与启动

```powershell
$env:CGO_ENABLED='0'
go build -trimpath -ldflags="-s -w" -o dist/azkits.exe ./cmd/azkits

dist\azkits.exe --config examples\toolbox.yaml          # 进入 TUI
dist\azkits.exe --config examples\toolbox.yaml list     # 非交互：列出脚本
dist\azkits.exe --config examples\toolbox.yaml check    # 非交互：校验入口
dist\azkits.exe --config examples\toolbox.yaml run demo-bat  # 非交互：运行脚本
```

## TUI 键盘操作

| 按键 | 行为 |
|------|------|
| `/` | 显示脚本候选 |
| 继续输入 | 实时过滤（收藏/最近使用置顶） |
| `↑` / `↓` | 移动选择；空输入时浏览命令历史 |
| `Tab` | 补全当前候选，不执行 |
| `Enter` | 执行完整匹配或明确选中的候选 |
| `Esc` | 关闭候选，不执行 |
| `Ctrl+P` | 导出本次执行日志（Notepad++ 优先，Notepad 回退） |
| `Ctrl+C` | 退出 azKits |

## 0014 收藏与最近使用

| 操作 | 说明 |
|------|------|
| `/fav <alias>` | 切换收藏状态（可反复收藏/取消） |
| 收藏标识 | 候选行左侧显示 `★`，置顶排序 |
| 最近使用标识 | 候选行右侧显示 `↺`，按最近成功启动排序 |
| 持久化 | 收藏保存于 `%LOCALAPPDATA%\azKits\favorites.json`，跨实例一致 |
| 最近使用 | 仅当前实例内存态，重启后清空 |

候选排序优先级：收藏且最近使用 > 仅收藏 > 仅最近使用 > 普通匹配。

## 收藏文件格式

```json
{
  "version": 1,
  "favorites": {
    "C:\\tools\\device\\entry.yaml": ["device-init", "regression"]
  }
}
```

损坏或 version 非 1 时自动备份为 `favorites.json.bad-{时间戳}` 并按空收藏继续，不阻断启动。

## 日志导出

- `Ctrl+P` 导出当前实例最近一次已结束的执行到 `%LOCALAPPDATA%\azKits\logs\`。
- 文档为 UTF-8 文本文档，含集合名、脚本、起止时间、结果、退出码与输出。
- Notepad++ 可用时优先；不可用时自动回退 Notepad。

## 人工验收提示

中文 IME 组词/选字/上屏期间 Enter 只完成上屏不执行脚本；如需验证，请在中文输入法下输入 `/` 后组词并连续按 Enter。
