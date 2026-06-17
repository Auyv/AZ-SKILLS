---
name: az-mpu-local-build
description: >
  在 Windows 宿主机上通过 local_build_mpu.bat 触发 Docker 容器内 MPU 交叉编译，
  验证持续集成构建链路是否正常。上限至容器内 build.sh 执行完毕，不涉及下游工具链细节。
  触发场景：用户提到"本地构建MPU"、"测试MPU编译"、"验证MPU CI"、"跑一下构建"、
  "试下能不能编译通过"等。
version: 1.0.0

inputs:
  required:
    - name: action
      type: string
      description: 构建动作，"build"（默认）或 "clean"（清理构建）。
      validation: 取值必须为 build 或 clean，不区分大小写。
  optional:
    - name: workspace
      type: string
      description: 项目工作区根目录绝对路径。
      default: "C:\\HQ_Workspace\\wit803_001-open"

outputs:
  - name: build_status
    type: string
    description: 构建结果，success 或 fail。
    format: >
      标准结构 { "status": "success"|"fail", "timestamp": "ISO 8601",
      "data": { "package_updated": bool, "zip_name": "..." } | "error": { "code": "ERR_xxx", "message": "..." } }
  - name: log_file
    type: file
    description: 完整构建日志。
    path: ai_tmp/build_log_YYYYMMDD_HHmmss.txt

errors:
  - code: ERR_DOCKER_NOT_FOUND
    message: Docker 未安装或 Docker Desktop 未启动。
    handling: 提示用户安装或启动 Docker，确认后重试。
  - code: ERR_CONTAINER_NOT_FOUND
    message: 容器 sim7805-wit803_001 不存在，需通过 ctp_docker_run.bat 创建。
    handling: 引导用户执行 tools/docker_cmd/ctp_docker_run.bat。
  - code: ERR_CONTAINER_NOT_RUNNING
    message: 容器已存在但未运行，且启动失败。
    handling: 检查端口冲突或其他原因，必要时重建容器。
  - code: ERR_PORT_CONFLICT
    message: 容器端口 127.0.0.1:2023 被其他容器占用。
    handling: 列出占用端口的容器，建议停止后重试。
  - code: ERR_BUILD_SCRIPT_MISSING
    message: local_build_mpu.bat 或容器内 build.sh 不存在。
    handling: 确认 workspace 路径正确，代码仓库完整。
  - code: ERR_BUILD_FAILED
    message: 容器内构建执行失败（build.sh 返回非 0）。
    handling: 输出最后 80 行日志，引导用户排查编译错误。
---

# MPU 本地构建验证

## 做什么

通过 `tools\local_build_mpu\local_build_mpu.bat` 一键触发 Docker 容器中的 MPU 交叉编译，
快速验证当前代码库的持续集成构建链路是否正常。

**作用域边界**：
- ✅ 容器启动 → `docker exec` 调用 `k3_sim7805/build/mpu/build.sh` → 等待返回
- ✅ 检查 `k3_sim7805/package/package.zip` 是否更新
- ❌ 不深入 `build.sh` 下游的 `build_sim7805.sh` 及具体 lib/app 编译细节

## 何时使用

当用户在 sim7805 项目（`WIT803-001`）中需要执行以下操作之一时激活本技能：

- 验证本地代码能否正常通过 CI 构建
- 更新 SDK 后确认编译链路完整性
- CI 构建失败后在本地复现排查
- 交付前最终构建验证

**触发措辞示例**：
- "本地构建MPU"
- "测试 MPU 编译"
- "验证 MPU CI"
- "跑一下构建"
- "试下能不能编译通过"

## 怎么做

### Step 0：环境检查

使用 `scripts/check_env.ps1` 检查前置条件：

```powershell
powershell -File <skill_dir>/scripts/check_env.ps1 -Workspace "<workspace>"
```

检查项：
1. `docker ps` 可执行（Docker 已安装且守护进程运行）
2. 容器 `sim7805-wit803_001` 存在
3. 容器处于运行状态（若退出则尝试 `docker start`）
4. 若 `docker start` 失败且报端口冲突，检查 `127.0.0.1:2023` 占用情况
5. `tools/local_build_mpu/local_build_mpu.bat` 文件存在
6. 容器内 `k3_sim7805/build/mpu/build.sh` 存在

### Step 1：记录构建前状态

记录 `k3_sim7805/package/package.zip` 的 LastWriteTime 作为比较基准。

### Step 2：执行构建

执行构建脚本（PowerShell 调用 .bat）：

```powershell
# 普通构建
& "<workspace>\tools\local_build_mpu\local_build_mpu.bat"

# 清理构建
& "<workspace>\tools\local_build_mpu\local_build_mpu.bat" clean
```

关键点：
- 脚本会通过 `docker exec` 进入容器执行 `build.sh`
- 构建时间约 3-10 分钟，取决于代码量和机器性能
- 构建日志应重定向到 `ai_tmp/` 目录保存

### Step 3：验证结果

构建完成后验证：
1. `.bat` 脚本 `errorlevel` 为 0
2. `build.sh` 输出中包含 "build build_app result: success"
3. `k3_sim7805/package/package.zip` 的 LastWriteTime 已更新
4. `package.zip` 文件大小显著大于 0（通常 > 100MB）

### 构建链路速览

```
Windows 宿主机
  └─ local_build_mpu.bat
       └─ docker exec -i sim7805-wit803_001 bash -c "..."
            └─ autobuild/k3_sim7805/k3_sim7805/build/mpu/build.sh
                 ├─ 编译 lib/（公共库）
                 ├─ 编译 app/（应用程序）
                 ├─ 打包 kernel/rootfs/firmware/fota
                 └─ 生成 k3_sim7805/package/package.zip
```

### 关键文件

| 文件 | 作用 |
|------|------|
| `tools/local_build_mpu/local_build_mpu.bat` | Windows 端构建入口 |
| `tools/docker_cmd/config.json` | 容器名/镜像/挂载/端口配置 |
| `tools/docker_cmd/ctp_docker_run.bat` | 容器创建/启动入口 |
| `autobuild/k3_sim7805/k3_sim7805/build/mpu/build.sh` | 容器内构建入口脚本 |

## 常见排错

### 容器未运行

```powershell
# 尝试启动
docker start sim7805-wit803_001

# 若端口冲突，找到占用端口的容器
docker ps -a --format "table {{.Names}}\t{{.Ports}}"
netstat -ano | Select-String ":2023 "

# 停止冲突容器后重试
docker stop <冲突容器名>
docker start sim7805-wit803_001
```

### 容器不存在

```powershell
# 通过配置重建
& "<workspace>\tools\docker_cmd\ctp_docker_run.bat"
```

### 容器 ID 变更

当前版本 `local_build_mpu.bat` 已改为按容器名 `sim7805-wit803_001` 动态解析 ID：

```bat
for /f %%i in ('docker ps -a -q -f "name=^%DOCKER_CONTAINER_NAME%$"') do set "DOCKER_CONTAINER_ID=%%i"
```

无需再关心容器重建导致的 ID 变更。

### SDK 头文件缺失

若构建报 "fatal error: xxx.h: No such file or directory"，检查：
1. SDK 包是否完整：`autobuild/local/k3_sim7805/sdk_WIT803-001/sdk/sim7805/include/`
2. 确认 SDK 版本是否匹配（当前 `B02V06`）

## 技能文件结构

```
az-mpu-local-build/
├── SKILL.md                          # AI Agent 技能说明
├── MANUAL.md                         # 手动操作指南
├── scripts/
│   └── check_env.ps1               # 环境检查脚本
├── docs/
│   └── build-chain.md              # 完整构建链路说明
└── evals/
    └── evals.json                  # 测试用例
```

## 相关文档

- 手动操作指南：[MANUAL.md](MANUAL.md)
- 构建链路详解：[docs/build-chain.md](docs/build-chain.md)
