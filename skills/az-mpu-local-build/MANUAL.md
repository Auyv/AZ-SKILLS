# MPU 本地构建验证 - 手动操作指南

本文档面向**没有 AI Agent 辅助**的用户，提供完整的 MPU 本地构建验证步骤。

## 前置条件

- Windows 10/11 操作系统
- Docker Desktop 已安装并运行
- 项目仓库已克隆至 `C:\HQ_Workspace\wit803_001-open`
- Docker 容器 `sim7805-wit803_001` 已创建（通过 `ctp_docker_run.bat`）

## 步骤一：确保 Docker 容器运行

```bat
:: 检查容器状态
docker ps -a --filter "name=sim7805-wit803_001"

:: 如果状态为 Exited，尝试启动
docker start sim7805-wit803_001

:: 如果启动失败（端口冲突），检查 2023 端口
netstat -ano | findstr ":2023 "

:: 停止占用端口的容器（如 FOTON-G11）
docker stop <容器名>
docker start sim7805-wit803_001
```

若容器不存在，运行：
```bat
C:\HQ_Workspace\wit803_001-open\tools\docker_cmd\ctp_docker_run.bat
```

## 步骤二：执行构建

```bat
:: 普通 Release 构建
cd C:\HQ_Workspace\wit803_001-open
tools\local_build_mpu\local_build_mpu.bat

:: 清理构建（先删除 build_output 再编译）
tools\local_build_mpu\local_build_mpu.bat clean
```

构建过程大约 3-10 分钟。

## 步骤三：验证结果

构建成功后，观察以下输出：

1. 最后一行应为：
   ```
   "local_build_mpu.bat return errorlevel:0"
   ```

2. 构建日志中应包含：
   ```
   build build_app result: success
   build build_firmware result: success
   build fota result: success
   build zip_package result: success
   ```

3. 检查生成物是否更新：
   ```bat
   dir C:\HQ_Workspace\wit803_001-open\autobuild\k3_sim7805\k3_sim7805\package\package.zip
   ```

   文件修改时间应为最近几分钟，大小 > 100MB。

## 构建链路

```
local_build_mpu.bat (Windows)
  └─ docker exec → sim7805-wit803_001 容器
       └─ autobuild/k3_sim7805/k3_sim7805/build/mpu/build.sh
            ├─ 读取 config/WIT803-001/list.env
            ├─ 读取 sdk/sim7805.env 工具链环境
            ├─ 编译 lib/ 公共库
            ├─ 编译 app/ 应用程序
            ├─ 打包 firmware/fota
            └─ 生成 k3_sim7805/package/package.zip
```

## 常见问题

### Q: "container is not running"
A: 容器被退出。`docker start sim7805-wit803_001`。

### Q: "port is already allocated"
A: 端口 2023 被占用。停止占用端口 2023 的其他容器。

### Q: "No such container: xxx"
A: 容器 ID 过期（容器被重建），新版本脚本已按名称动态解析，不应再出现此问题。
若仍出现，检查 `local_build_mpu.bat` 第 29 行是否为 `for /f` 动态解析。

### Q: "fatal error: xxx.h: No such file or directory"
A: SDK 缺少头文件，检查：
```
autobuild\local\k3_sim7805\sdk_WIT803-001\sdk\sim7805\include\
```
确认 SDK 包完整且版本匹配（当前 B02V06）。

### Q: 构建时间过长
A: 正常 3-10 分钟。若超过 15 分钟无响应，可能：
- Docker 资源不足（设置中调整 CPU/内存限额）
- 磁盘 I/O 瓶颈（关闭其他占用磁盘的程序）

## 关键路径速查

| 用途 | 路径 |
|------|------|
| 构建入口 | `tools\local_build_mpu\local_build_mpu.bat` |
| 容器配置 | `tools\docker_cmd\config.json` |
| 容器启动 | `tools\docker_cmd\ctp_docker_run.bat` |
| 容器内构建 | `autobuild\k3_sim7805\k3_sim7805\build\mpu\build.sh` |
| 构建产物 | `autobuild\k3_sim7805\k3_sim7805\package\package.zip` |
| SDK 头文件 | `autobuild\local\k3_sim7805\sdk_WIT803-001\sdk\sim7805\include\` |
| 构建日志 | `ai_tmp\build_log_*.txt`（AI 模式下自动保存） |
