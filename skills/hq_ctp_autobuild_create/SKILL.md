---
name: hq_ctp_autobuild_create
description: |
  用于从零创建新的 CTP autobuild 持续集成仓库。触发词包括“创建新的持续集成仓库”、“新建 CI 仓”、“搭建 autobuild 集成仓”。适用于 MCU+MPU 双芯 CTP/TBOX/网关类项目，输出仓库骨架、repo manifests、config/<product>/ 产品配置、项目层文档和三阶段验证清单。
version: 0.1.0
inputs:
  required:
    - name: ci_repo_name
      type: string
      description: 新持续集成仓名称
      validation: 非空
    - name: product
      type: string
      description: 产品配置名，对应 config/<product>/、mcu_<product>.xml、mpu_<product>.xml
      validation: 非空
    - name: product_code
      type: string
      description: 产品编码，对应 G_PROJ_PRODUCT_CODE 和 NFCreator Images/<CODE>/
      validation: 非空
    - name: mcu_repo
      type: string
      description: MCU 业务仓库名或 manifest project name
      validation: 非空
    - name: mpu_platform_repo
      type: string
      description: MPU platform/app/lib/drv/tools 主仓或仓库组信息
      validation: 非空
    - name: nfcreator_repo
      type: string
      description: NFCreator 或等效出包工具仓
      validation: 非空
    - name: mpu_model
      type: string
      description: Jenkins 第三参和 G_PRODUCT_BUILD_MPU_MODEL 使用的 MPU 模组型号
      validation: 非空
    - name: mpu_family
      type: string
      description: build/mpu/build_<mpu_family>.sh 对应的平台脚本族
      validation: 非空
  optional:
    - name: template_source
      type: string
      description: 最接近的已有 autobuild 项目路径，用于复制 build/ 和 tools/ 骨架
      default: ""
    - name: mcu_chip
      type: string
      description: MCU 芯片型号
      default: ""
    - name: mcu_toolchain
      type: string
      description: MCU 工具链
      default: ""
    - name: docker_image
      type: string
      description: Step2 Linux 容器镜像
      default: ""
outputs:
  - name: repo_skeleton
    type: directory_plan
    path: "{ci_repo_name}/"
    description: 新 CI 仓最小目录骨架
  - name: manifests
    type: files
    path: "{ci_repo_name}/manifests/"
    description: mcu_<product>.xml、mpu_<product>.xml、mcu_master.xml
  - name: product_config
    type: directory
    path: "{ci_repo_name}/config/{product}/"
    description: 产品配置目录和 list.env 草案
  - name: project_docs
    type: files
    path: "{ci_repo_name}/doc/autobuild/{ci_repo_name}/"
    description: 项目层说明文档
  - name: validation_checklist
    type: markdown
    description: Step1/Step2/Step3/Jenkins 验证清单
errors:
  - code: ERR_MISSING_REQUIRED_INFO
    message: 创建新 CI 仓所需关键信息不完整
    handling: 停止生成最终脚本，先输出缺失字段清单和差异表草稿
  - code: ERR_UNKNOWN_MPU_FAMILY
    message: 无法确定 mpu_model 到 build_<mpu_family> 的映射
    handling: 不生成 Step2 分发逻辑，要求补充 mpu_family 或参考项目
  - code: ERR_NFCREATOR_PATH_UNCLEAR
    message: NFCreator 工具仓 path 不明确
    handling: 要求以 manifests/*.xml 中 project path 为准
  - code: ERR_TEMPLATE_SOURCE_INVALID
    message: 参考 autobuild 项目不存在或缺少 build/tools 骨架
    handling: 退化为最小空骨架并标 TODO
  - code: ERR_UNSAFE_SECRET_IN_REPO
    message: TEE/secure OTA 私钥等敏感材料被要求直接入仓
    handling: 阻断并建议 Jenkins 凭据或安全制品仓注入
---

# hq_ctp_autobuild_create

## 何时使用

用户要求创建新的持续集成仓库、新建 CI 仓、搭建 autobuild 集成仓时使用。目标是先跑通三阶段最小闭环，再补齐业务模块。

## 必读参考

优先读取目标工作区中的：

- `doc/autobuild/ctp_template/06-新建集成仓流程.md`
- `doc/autobuild/ctp_template/05-项目差异表模板.md`
- 最接近的项目层 `README.md`、`03-项目差异表.md`、`11-新增产品配置指南.md`

## 工作流

1. 收集建仓信息：CI 仓名、产品名、产品编码、MCU 仓、MPU 仓、MPU 型号、脚本族、NFCreator 仓、Docker 镜像、partition 来源、安全策略。
2. 缺信息时停止生成最终脚本，输出缺失字段清单和差异表草稿。
3. 创建或规划仓库骨架：`manifests/`、`build/mcu/`、`build/mpu/`、`config/<product>/`、`package/`、`tools/`、`doc/autobuild/<project>/`。
4. 生成三份 XML：`mcu_<product>.xml`、`mpu_<product>.xml`、`mcu_master.xml`。NFCreator 位置只以 XML 的 `project path` 为准。
5. 生成 `config/<product>/list.env` 最小字段：`G_PRODUCT_BUILD_MPU_MODEL`、`G_PROJ_PRODUCT_CODE`、`G_PROJ_HUBS_*`、`G_PROJ_HD_VER`、`G_PROJ_MPU_SOFT_VERSION`。
6. 生成项目层文档：至少 `README.md`、`03-项目差异表.md`、`10-常见问题与调试记录.md`，建议补齐 `01/02/04/05/06/07/09/11`。每份文档必须加入“时间属性”块，记录生成时间、仓库扫描基准时间、适用分支/revision、适用产品配置、时效性说明和复核周期。
7. 输出本地验证命令和判据。

## 最小骨架

```text
<ci_repo>/
├── manifests/
├── build/
│   ├── mcu/
│   ├── mpu/
│   └── pack_mcu_mpu.bat
├── config/<product>/
├── package/
├── tools/
└── doc/autobuild/<ci_repo>/
```

## 验证清单

Step1:

```bat
cd <ci_repo>\build\mcu
call build.bat <product>
call pack.bat <product> "smoke"
```

Step2:

```sh
cd <ci_repo>/build/mpu
sh build.sh <ci_repo_name> <product> <mpu_model>
```

Step3:

```bat
cd <ci_repo>\build
call pack_mcu_mpu.bat <product> "smoke"
```

Jenkins 接入必须等本地三阶段通过后再做，并确认 Windows bat 中 `PAUSE` 已被非交互跳过。

## 文档时间属性

新建 CI 仓时同步生成项目层文档，所有文档标题后必须包含：

```markdown
> 时间属性
> - 文档生成时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 仓库扫描基准时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 适用仓库/分支：<ci_repo>/<branch 或 revision>
> - 适用产品配置：<product>
> - 时效性说明：本文基于上述时间点的 manifests/config/build/tools 生成；仓库分支、Jenkins 参数、NFCreator、partition 或 list.env 变化后需要复核。
> - 建议复核周期：首次接 Jenkins 前、每次发版前、切换 SDK/工具链/安全策略后复核。
```

不要使用无时间锚点的“当前”“最新”描述；涉及 revision、SDK、Docker 镜像、工具链和 Jenkins 参数时，必须写明来源文件和采集时间。