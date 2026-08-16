# 手动创建 azKits 空配置

在本技能目录执行：

```powershell
python scripts/create_config.py --output C:\work\my-toolbox.yaml --name "我的工具箱" --description "本地测试入口"
```

父目录尚不存在并且允许创建时：

```powershell
python scripts/create_config.py --output C:\work\new\my-toolbox.yaml --name "我的工具箱" --description "本地测试入口" --create-parent
```

成功后文件内容类似：

```yaml
version: 1
toolbox:
  name: 我的工具箱
  description: 本地测试入口
scripts: []
```

脚本不会覆盖已有文件。成功和失败都会向标准输出写入包含 `status`、`timestamp` 的 JSON，成功退出码为 0，失败退出码为 1。

## 同时创建一键启动 BAT

```powershell
python scripts/create_config.py `
  --output C:\work\my-toolbox.yaml `
  --name "我的工具箱" `
  --description "本地测试入口" `
  --create-launcher `
  --azkits-exe C:\tools\azkits.exe
```

默认创建 `C:\work\my-toolboxRun.bat`。自定义同级文件名时追加：

```powershell
--launcher-name "测试工具箱-azKitsRun.bat"
```

也可为已有 YAML 单独创建启动器：

```powershell
python scripts/create_launcher.py `
  --config C:\work\my-toolbox.yaml `
  --azkits-exe C:\tools\azkits.exe `
  --launcher-name "测试工具箱-azKitsRun.bat"
```

启动器使用 `%~dp0` 查找同级 YAML，并检查 EXE 和配置是否存在。异常退出时会保留窗口显示错误码。脚本不会覆盖同名 BAT。
