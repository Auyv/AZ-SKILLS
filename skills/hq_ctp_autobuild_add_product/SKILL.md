---
name: hq_ctp_autobuild_add_product
description: |
  用于给已有 CTP autobuild / 持续集成仓库添加产品配置。触发词包括“已有集成仓新增产品”、“添加产品配置”、“新增 config 产品目录”。指导复制 config、创建 mcu/mpu XML、准备 NFCreator Images/<CODE>、处理旧 partition 兼容目录、更新文档并验证 Step1/2/3。
version: 0.1.0
inputs:
  required:
    - name: target_repo
      type: directory
      description: 已有持续集成仓根目录
    - name: base_product
      type: string
      description: 作为复制基线的已有产品配置名
    - name: new_product
      type: string
      description: 新产品配置名，对应 config/<new_product>/
    - name: new_product_code
      type: string
      description: 新产品编码，对应 G_PROJ_PRODUCT_CODE 和 NFCreator Images/<CODE>/
  optional:
    - name: mpu_model
      type: string
      description: 新产品 MPU 型号；默认沿用 base_product
      default: ""
    - name: mcu_revision
      type: string
      description: 新产品 MCU 仓 revision
      default: ""
    - name: mpu_revisions
      type: object
      description: 新产品 MPU app/lib/drv/platform 仓 revision 差异
      default: {}
    - name: use_legacy_partition_dir
      type: boolean
      description: 是否继续补齐 tools/batch_nfcreator/partition_ini/<new_product>/ 旧兼容目录
      default: true
    - name: dry_run
      type: boolean
      description: 只输出计划和补丁建议，不实际修改文件
      default: false
    - name: run_validation
      type: string
      description: 验证范围，可选 none、inspect、step1、step2、step3、all
      default: inspect
outputs:
  - name: product_config
    type: directory
    path: "{target_repo}/config/{new_product}/"
    description: 新产品配置目录
  - name: product_manifests
    type: files
    path: "{target_repo}/manifests/"
    description: mcu_<new_product>.xml 与 mpu_<new_product>.xml
  - name: nfcreator_checklist
    type: markdown
    description: NFCreator Images/<CODE> 与 partition 迁移检查清单
  - name: doc_updates
    type: files
    description: 项目层 README、差异表、list.env 说明、FAQ、11 指南更新建议
errors:
  - code: ERR_TARGET_REPO_NOT_FOUND
    message: 目标集成仓不存在或不可读
    handling: 停止操作，要求提供正确路径
  - code: ERR_BASE_PRODUCT_NOT_FOUND
    message: 基线产品 config/<base_product>/ 不存在
    handling: 停止复制，要求选择已有产品
  - code: ERR_NEW_PRODUCT_EXISTS
    message: config/<new_product>/ 已存在
    handling: 停止覆盖，要求确认更新或换名
  - code: ERR_PRODUCT_CODE_MISSING
    message: 新产品编码缺失
    handling: 停止生成 list.env 和 NFCreator 目录
  - code: ERR_MPU_MAPPING_MISSING
    message: 新产品 MPU 型号无法映射到 build 脚本族
    handling: 要求补充 mpu_model 或修改 build/mpu/build.sh
  - code: ERR_PARTITION_NOT_READY
    message: 新产品 NFCreator/partition 配置未准备
    handling: 标记 Step1/Step3 阻断，说明新模式和旧兼容处理方式
  - code: ERR_PRODUCT_CODE_CONFLICT
    message: product_code 已被其他 config/list.env 或 NFCreator Images 目录使用
    handling: 停止修改，提示用户确认产品编码是否重复
  - code: ERR_XML_SOURCE_MISSING
    message: 基线产品 mcu/mpu XML 不完整
    handling: 要求用户指定 XML 复制来源或先补齐基线 XML
---

# hq_ctp_autobuild_add_product

## 何时使用

用户要求在已有集成仓新增产品配置、添加产品配置、新增 `config/<product>/` 时使用。

## 必读参考

- `doc/autobuild/ctp_template/07-已有集成仓新增产品流程.md`
- 当前项目层 `11-新增产品配置指南.md`
- 当前项目层 `03-项目差异表.md`、`05-产品配置list.env说明.md`、`09-整包升级打包.md`

## 工作流

1. 判断新增类型：只换配置、换应用清单、换 MPU、换 MCU。换 MCU 芯片时先评估是否应新建集成仓。
2. 先做静态冲突检查：`config/<new_product>/`、`manifests/*_<new_product>.xml`、NFCreator `Images/<new_product_code>/`、旧兼容 partition 目录是否已存在。若 `dry_run=true`，只输出计划和补丁建议，不实际修改文件。
3. 复制 `config/<base_product>/` 到 `config/<new_product>/`，更新 `list.env`。
4. 复制 `manifests/mcu_<base_product>.xml` 和 `mpu_<base_product>.xml` 为新产品 XML。
5. 修改 XML 中 MCU/MPU 业务仓 revision，保留 CI 仓和 NFCreator `project path` 的一致性。
6. 准备 NFCreator `Images/<NEW_CODE>/`。新模式将 partition 放在 NFCreator 产品目录。
7. 若脚本仍检查 `tools/batch_nfcreator/partition_ini/<product>/`，短期补齐旧兼容目录，长期计划迁移并调整 `pack_mcu_mpu.bat`。
8. 按 `run_validation` 范围执行或输出 Step1/Step2/Step3 smoke 命令。
9. 更新项目层 README、03、05、09、10、11，并同步刷新每份文档的“时间属性”块。

## list.env 必改字段

- `G_PROJ_PRODUCT_CODE`
- `G_PRODUCT_BUILD_MPU_MODEL` 或 `G_BUILD_MODEL`
- `G_PROJ_HUBS_PRODUCT_NAME`
- `G_PROJ_HUBS_PCB_VERSION`
- `G_PROJ_HUBS_BOOM_VERSION`
- `G_PROJ_HUBS_SOFT_VERSION`
- `G_PROJ_MPU_SOFT_VERSION`
- `G_PROJ_HD_VER`
- `G_PROJ_LIST_*`
- 安全和调试开关

不要复用旧产品编码和 TEE key。

## 验证命令

Step1:

```bat
cd build\mcu
call build.bat <new_product>
call pack.bat <new_product> "new product smoke"
```

Step2:

```sh
cd build/mpu
sh build.sh <project_name> <new_product> <mpu_model>
```

Step3:

```bat
cd build
call pack_mcu_mpu.bat <new_product> "new product smoke"
```

## 完成标准

- 新产品 config 和两份 XML 已存在。
- NFCreator `Images/<NEW_CODE>/` 已准备。
- 旧 partition 兼容目录状态明确。
- Step1/2/3 smoke 结果明确。
- 项目层文档已同步，并记录文档生成/更新时间、仓库扫描基准时间、适用分支/revision、新增产品配置和时效性说明。
## 文档时间属性

新增产品配置会改变项目文档的适用范围。更新 README、03、05、09、10、11 时，必须在每份文档标题后维护：

```markdown
> 时间属性
> - 文档生成时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 仓库扫描基准时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 适用仓库/分支：<target_repo>/<branch 或 revision>
> - 适用产品配置：<base_product>, <new_product>
> - 时效性说明：本文基于上述时间点的 manifests/config/build/tools 生成；新增产品的 list.env、XML、NFCreator、partition 或 Jenkins 参数变化后需要复核。
> - 建议复核周期：新增产品合入前、Step1/2/3 验证后、Jenkins 参数上线前、每次发版前复核。
```

不要只写“已更新”“当前配置”；必须说明信息来源时间和适用范围。

## dry-run 要求

当 `dry_run=true` 时，只输出以下内容，不写文件：

- 将新增/修改/检查的路径列表。
- `list.env` 字段修改建议。
- XML 复制来源和 revision 修改建议。
- NFCreator `Images/<CODE>/` 与旧兼容 partition 目录检查结果。
- Step1/Step2/Step3 验证命令。

## 冲突检查

执行任何复制前必须检查：

- `config/<new_product>/` 是否已存在。
- `manifests/mcu_<new_product>.xml` 与 `manifests/mpu_<new_product>.xml` 是否已存在。
- `G_PROJ_PRODUCT_CODE` 是否已被其他 `config/*/list.env` 使用。
- NFCreator `Images/<new_product_code>/` 是否已存在且是否属于同一产品。
- `tools/batch_nfcreator/partition_ini/<new_product>/` 是否已存在。

发现冲突时停止覆盖，改为询问用户是检查现有配置、合并补齐，还是更换产品名。