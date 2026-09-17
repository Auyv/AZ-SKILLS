---
name: hq-autobuild-mpu
description: >
  为当前 CTP/车载持续集成仓库生成或优化 build/autobuild-mpu.bat，自动读取
  当前仓库或上级工作区的 tools/docker_cmd/config.json，生成通过 Docker 调用 build.sh 的 Windows
  自动构建入口；也可运行该入口验证 MPU 编译。用户提到“生成/修改 autobuild-mpu.bat”、
  “本地构建 MPU”、“验证 MPU CI”或“跑 MPU 构建”时使用。
version: 2.2.0

inputs:
  required:
    - name: workspace
      type: string
      description: 持续集成仓库根目录绝对路径；目录内应存在 build/mpu/build.sh 和 config/*/list.env。未明确给出时使用当前工作区。
      validation: 目录必须存在。
  optional:
    - name: operation
      type: string
      description: generate（仅生成/优化脚本）或 build（生成后执行构建）。
      default: generate
    - name: output_path
      type: string
      description: autobuild-mpu.bat 输出路径。
      default: <workspace>\build\autobuild-mpu.bat
    - name: build_model
      type: string
      description: 显式覆盖 MPU 构建模型；默认从产品 list.env 推导。
      default: null

outputs:
  - name: generated_script
    type: file
    description: 与当前项目配置匹配的 autobuild-mpu.bat。
    path: <output_path>
  - name: result
    type: string
    description: 生成或构建结果。
    format: >
      { "status": "success"|"fail", "timestamp": "ISO 8601",
      "data": { "script": "...", "container": "...", "image": "...", "project": "...", "product": "...", "detected_model": "...", "model_argument": null|"..." }
      | "error": { "code": "ERR_xxx", "message": "..." } }

errors:
  - code: ERR_WORKSPACE_NOT_FOUND
    message: 工作区不存在。
    handling: 输出目标绝对路径并停止。
  - code: ERR_CONFIG_MISSING
    message: 当前持续集成仓库及其上级目录中不存在 tools/docker_cmd/config.json，或配置无法解析。
    handling: 要求用户补充项目 Docker 配置，不猜测容器名。
  - code: ERR_BUILD_TREE_AMBIGUOUS
    message: 找不到 MPU build.sh，或发现多个候选且无法唯一确定。
    handling: 列出候选路径，请用户指定构建树。
  - code: ERR_PRODUCT_CONFIG_MISSING
    message: 无法从 config/<product>/list.env 推导产品或构建模型。
    handling: 允许用户显式指定 product/build_model。
  - code: ERR_DOCKER_NOT_FOUND
    message: Docker CLI 不存在或 Docker Engine 未运行。
    handling: 生成操作仍可完成；仅阻止实际构建并保留原始错误。
  - code: ERR_CONTAINER_NOT_FOUND
    message: 配置中的容器不存在。
    handling: 引导执行 tools/docker_cmd/ctp_docker_run.bat。
  - code: ERR_BUILD_FAILED
    message: build.sh 返回非 0。
    handling: 保留原始输出和返回码，按故障特征查阅 docs/faq/build-debug.md。
  - code: ERR_BUILD_WORKDIR
    message: 容器构建工作目录不存在或构建过程偏离 SDK 工作目录。
    handling: 对照宿主机目录名和容器挂载路径，记录 pwd 后使用明确的 cd/-w。
  - code: ERR_DRIVER_PATH
    message: 驱动列表为空或驱动目录缺失时仍执行 pushd。
    handling: 空列表直接跳过；非空列表且目录缺失时明确失败。
---

# 项目自适应 MPU 自动构建脚本

## 做什么

根据当前项目的真实配置生成 `autobuild-mpu.bat`，不要沿用技能内的固定项目名、容器名或路径。
生成的脚本负责检查 Docker、按容器名工作、必要时启动容器、调用项目 MPU `build.sh`，并透传返回码。

本技能有两种操作：

- `generate`：生成或优化脚本，做静态和非构建验证；不擅自触发耗时编译。
- `build`：生成后直接执行脚本并等待编译完成。

用户只说“修改/生成脚本”时使用 `generate`；只有明确要求编译、验证构建时才使用 `build`。

## 怎么做

### 1. 遵循项目规则并识别编码

读取工作区 `AGENTS.md` 及其引用规则。修改已有 `.bat` 前识别编码，并备份到项目 `.ai_tmp/bak/`。

### 2. 从项目中推导参数

按以下来源推导，禁止复制旧项目常量：

1. 从持续集成仓库开始向上查找最近的 `tools/docker_cmd/config.json`
   - `containers[0].name` → 容器名
   - `containers[0].volumes` → Windows 工作区对应的容器挂载点
2. 使用持续集成仓库内的 `build/mpu/build.sh`
   - 持续集成仓库目录名 → `G_PROJECT_NAME`
   - 持续集成仓库的上一级目录 → Docker 工作目录
   - 为兼容旧项目，若传入总工作区且存在 `autobuild/test_mpu/`，仍可从其中唯一的构建树推导持续集成仓库
3. 查找 `<project>/config/*/list.env`
   - 配置目录名 → `G_PRODUCT_NAME`
   - `G_PRODUCT_BUILD_MPU_MODEL` → 用于核验模型；是否作为第三参数传入必须服从项目 `build.sh` 的分支语义
4. 若候选不唯一，停止并列出候选，不自行选择。
5. 默认把脚本放入持续集成仓库的 `build/autobuild-mpu.bat`；不要再写入总工作区的 `autobuild/test_mpu/`。

优先运行生成器：

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_autobuild_mpu.ps1 `
  -Workspace "<ci-repository>"
```

需要覆盖模型时加 `-BuildModel "<model>"`；需要其他输出位置时加 `-OutputPath "<path>"`。

### 3. 生成脚本的强制要求

生成的 `.bat` 必须：

- 使用配置中的容器名，禁止硬编码容器 ID。
- 先检查 `where docker`，再用 `docker info` 区分 CLI 缺失与 Engine 未运行。
- 用 `docker inspect` 检查容器存在性和运行状态；停止时尝试 `docker start`。
- 使用 `docker exec -i -w <build-root> ... <container> bash <build.sh> <project> <product>`；只有目标项目明确要求第三参数时才传 `<model>`。
- AG35 多合一构建中，第三参数为空通常用于加载 `list.env` 后进入 `build_ag35_xxt.sh`；不要把 `ag35_cetda` 直接拼成 `build_ag35_cetda.sh`。
- 若下游依赖 `DOCKER_IMAGE_NAME`/`DOCKER_LOCAL_DEBUG`，从 `config.json` 提取镜像并通过 `docker exec -e` 传入。
- 保存并透传 `build.sh` 的 `%ERRORLEVEL%`。
- 自动化入口不添加 `pause`。
- 日志优先 ASCII，避免 Windows 控制台编码问题。

### 4. 验证

`generate` 模式至少验证：

1. 输出文件为有效 UTF-8 或保留项目原编码。
2. 宿主机 `build.sh`、产品 `list.env` 存在。
3. 生成内容中的容器名、挂载路径、项目、产品、模型与源配置一致。
4. 若 Docker 可用，再执行只读的 `docker inspect` 和容器内 `test -f`。
5. Docker 不可用时报告原始错误，但不把“未实际编译”描述为生成失败。

`build` 模式直接运行用户指定或刚生成的 `.bat`；用户已指定脚本入口时不得拆解替代。保存完整日志到 `<ci-repository>/.ai_tmp/`，并以脚本返回码判断成功失败。

### 5. 构建失败调试顺序

先定位首个导致非零返回的原始错误，不把后续级联的 `build ... result: fail` 当根因。按以下顺序排查：

1. 批处理参数、容器名、镜像名和环境变量。
2. 宿主机目录与容器 `-w` 路径是否逐字符一致（特别注意 `-` 与 `_`）。
3. `pwd`、`pushd/popd` 和 `cd` 是否让 `make` 位于正确 SDK 根目录。
4. 配置列表为空时是否仍进入对应目录；空列表应成功跳过。
5. 脚本实际由 `sh` 还是 `bash` 启动；`[[ ]]`、`${var//.../...}` 等属于 Bash 语法。
6. 执行被中断后先检查容器进程，避免两个构建同时修改共享 `/home/ql-ol-sdk`。

详细症状、证据和修复模式见 [docs/faq/build-debug.md](docs/faq/build-debug.md)。

## 边界

- 作用域止于 `build.sh` 执行完毕。
- 不修改 `build.sh` 下游的库、应用、内核、rootfs 或打包逻辑。
- 不因旧技能示例而假定 WIT803、sim7805、YLB04-A6、AG35 或固定端口。
- 不自动创建 Docker 容器；容器缺失时提示项目已有的 `ctp_docker_run.bat`。

## 配套文件

- [MANUAL.md](MANUAL.md)：手动生成与使用方法
- [scripts/generate_autobuild_mpu.ps1](scripts/generate_autobuild_mpu.ps1)：项目自适应生成器
- [scripts/check_env.ps1](scripts/check_env.ps1)：旧项目构建环境检查器，仅兼容参考
- [docs/build-chain.md](docs/build-chain.md)：通用调用链说明
- [docs/faq/build-debug.md](docs/faq/build-debug.md)：实战故障诊断经验
- [evals/evals.json](evals/evals.json)：技能评测用例
