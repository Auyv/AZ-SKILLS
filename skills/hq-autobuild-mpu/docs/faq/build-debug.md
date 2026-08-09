# MPU 本地构建故障诊断经验

## 通用原则

1. 保存完整日志，找第一个产生非零返回码的原始错误。
2. `build build_images result: fail` 等通常只是上游失败的汇总，不是根因。
3. 修改后先做定向验证，再执行完整构建。
4. 工具等待被中断不等于容器进程结束；重跑前执行 `docker exec <container> ps -ef`。

## `build_<model>.sh: No such file`

### 症状

```text
build_ag35_cetda.sh: No such file or directory
```

### 原因与处理

批处理错误地把 `ag35_cetda` 作为第三参数传给 `build.sh`。部分 AG35 项目的第三参数为空时，`build.sh` 才会读取 `list.env`，设置 `G_BUILD_MODEL` 并进入通用的 `build_ag35_xxt.sh`。

先阅读目标 `build.sh` 的分支，再决定是否传第三参数；不要根据模型名猜测脚本文件名。

## `DOCKER_IMAGE_NAME is empty`

### 症状

```text
Error: DOCKER_IMAGE_NAME is empty
build no_docker_ver_file result: fail
```

### 原因与处理

本地容器缺少 CI 注入的镜像元数据。读取 `tools/docker_cmd/config.json` 的实际镜像，并在 `docker exec` 中传入：

```text
-e DOCKER_IMAGE_NAME=<image> -e DOCKER_LOCAL_DEBUG=1
```

不要沿用旧项目或旧版本镜像常量。

## 容器工作目录不存在

### 症状

```text
chdir to cwd (...) failed: no such file or directory
```

### 原因与处理

宿主机目录名与容器 `-w` 路径不一致，常见错误是混淆 `test-mpu2` 和 `test_mpu2`。从 `config.json` 的 volume 映射和脚本真实路径重新计算容器路径，并用容器内 `test -f` 验证入口。

## `make: No rule to make target 'uboot'`

### 诊断顺序

1. 在报错前输出 `pwd`、`ls -l Makefile` 和目标检查结果。
2. 检查 `pushd/popd` 是否配对；必要时在构建链前显式 `cd "${G_SDK_PATH}"`。
3. 搜索所有覆盖 SDK Makefile 的操作。已见模式：

```sh
CopyFileIfExist "${G_PROJ_BUILD_CONFIG_PATH}/Makefile" "${G_SDK_PATH}/"
```

4. 确认源 Makefile 是否真实存在；复制函数可能在源不存在时静默跳过。
5. 对共享 SDK 使用 `make -C "${G_SDK_PATH}" -f "${G_SDK_PATH}/Makefile" <target>` 可减少隐式目录风险。

## 空驱动列表导致 `pushd` 失败

### 症状

```text
pushd: .../platform/drv: No such file or directory
popd: directory stack empty
```

### 原因

`G_PROJ_LIST_DRV=""`，但安装和导出逻辑仍无条件进入 `${G_PROJ_DRV_PATH}`。

### 修复模式

- 列表为空：输出 skip 并成功返回。
- 列表非空且目录不存在：明确 `BuildFail no_driver_path`。
- 只在 `pushd` 成功后执行 `popd`，路径始终加双引号。

`depmod` 缺少 `modules.order/modules.builtin` 通常只是警告；应以随后第一个非零命令为准。

## `sh` 与 Bash 兼容

即使文件头是 `#!/bin/bash`，上游若用 `sh script.sh` 调用，实际解释器由 `/bin/sh` 决定。排查时执行：

```sh
readlink -f /bin/sh
```

`[[ ... ]]`、`${var//pattern/replacement}` 是 Bash 语法。若脚本已依赖这些语法，统一用 `bash script.sh`；若必须兼容 POSIX `sh`，应系统性替换全部 Bash 专用语法，而不是只改一处。

## 中文批处理注释被执行

UTF-8 `.bat` 使用 `::` 加中文时，某些 CMD 环境可能把残片识别成命令。自动构建入口使用纯 ASCII `rem` 注释，并移除 `pause`。
