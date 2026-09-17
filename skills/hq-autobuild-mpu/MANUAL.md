# autobuild-mpu.bat 生成与使用指南

## 生成脚本

在 PowerShell 中执行：

```powershell
$SkillDir = "C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\skills\hq-autobuild-mpu"
powershell -ExecutionPolicy Bypass -File "$SkillDir\scripts\generate_autobuild_mpu.ps1" `
  -Workspace "C:\path\to\ci-repository"
```

默认输出：

```text
<ci-repository>\build\autobuild-mpu.bat
```

可选参数：

```powershell
-OutputPath "C:\path\autobuild-mpu.bat"
-BuildModel "ag35_cetda"
```

生成器会读取：

- 当前持续集成仓库或上级目录中的 `tools/docker_cmd/config.json`：容器名与容器挂载点
- `<ci-repository>/build/mpu/build.sh`：工程名与构建工作目录
- `<ci-repository>/config/*/list.env`：产品名与 MPU 构建模型

若存在多个容器、多个构建入口或多个产品配置，生成器会停止并列出候选，避免生成到错误项目。

## 执行构建

先启动 Docker Desktop，然后运行：

```powershell
& "<ci-repository>\build\autobuild-mpu.bat"
```

只有目标项目明确要求第三参数时，重新生成带模型覆盖的脚本：

```powershell
powershell -ExecutionPolicy Bypass -File "$SkillDir\scripts\generate_autobuild_mpu.ps1" `
  -Workspace "<ci-repository>" -BuildModel "<build-model>"
```

默认不要传模型参数，让项目 `build.sh` 按 `list.env` 选择正确的通用构建分支。

脚本会检查 Docker CLI、Docker Engine 和容器状态，然后执行容器内 `build.sh`。成功返回 `0`，失败透传非零返回码。

## 常见问题

### Docker Engine 未运行

启动 Docker Desktop 后重试。典型 Windows 错误包含：

```text
open //./pipe/dockerDesktopLinuxEngine: The system cannot find the file specified.
```

### 容器不存在

执行项目已有入口：

```powershell
& "<docker-workspace>\tools\docker_cmd\ctp_docker_run.bat"
```

### 无法唯一识别项目或产品

直接把 `-Workspace` 指向包含 `build/mpu/build.sh` 的持续集成仓库。旧式总工作区模式下，检查 `autobuild/test_mpu/` 是否混入多个构建树。

### 构建失败

以 `.bat` 输出的首个原始错误和返回码为准，不要只看最后的级联 `result: fail`。
常见的参数、容器路径、Docker 环境变量、工作目录、空驱动列表和 Shell 兼容问题见：
[docs/faq/build-debug.md](docs/faq/build-debug.md)。
