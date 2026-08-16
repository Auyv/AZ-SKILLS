# 手动向 azKits 添加工具

基本示例：

```powershell
python scripts/add_tool.py `
  --config C:\work\toolbox.yaml `
  --id device-init `
  --alias device-init `
  --name "设备初始化" `
  --description "初始化设备并检查连接" `
  --path scripts/device_init.ps1 `
  --type powershell
```

带固定参数、环境变量和工作目录：

```powershell
python scripts/add_tool.py `
  --config C:\work\toolbox.yaml `
  --id device-init `
  --alias device-init `
  --name "设备初始化" `
  --description "初始化设备并检查连接" `
  --path scripts/device_init.ps1 `
  --type powershell `
  --arg COM3 --arg 115200 `
  --cwd scripts `
  --env DEVICE_MODE=test `
  --encoding utf-8
```

脚本修改前会在配置同目录创建 `<配置名>.azkits-add-tool.<时间戳>.bak`。确认新配置通过 `azkits --config <配置> check` 后，可以手动删除该备份；失败时用备份恢复。

成功和失败均输出包含 `status`、`timestamp` 的 JSON。脚本拒绝重复 `id/alias`，不会解释或执行被添加的工具命令。
