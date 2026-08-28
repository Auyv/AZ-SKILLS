# Go CLI 与 GUI 规则

> 仅在新建、修改或评审 Go 命令行工具，以及为其增加 GUI 时加载。

## CLI 与 GUI 决策

- 开发或迭代 Go 命令行工具前，主动询问用户是否需要同时提供 GUI；用户已有明确说明时无需重复询问。
- 用户确认需要 GUI 时，默认采用 Wails V2（稳定版）+ Vue 3 + TypeScript。
- 打包产物不得启动 HTTP 服务或监听本地 TCP 端口。

## 架构约定

- 业务逻辑保留在 Go 后端协程并复用 CLI 逻辑。
- 前端经 Wails 绑定调用导出函数。
- 长任务状态和日志使用 `runtime.EventsEmit` / `EventsOn` 推送，禁止前端轮询。

## 默认图标

- 新建 GUI 工具默认图标为 `C:\JPZ_Workspace\AZ_TOOLS\build_template\Wails GUI - 工程模版\appicon.png`，并置于 `build/appicon.png`。
- 若 `build/windows/icon.ico` 并非由该 PNG 生成，先删除该 ICO，再执行 `wails build` 重新生成。
