# hq_ctp_autobuild_create 手动执行指南

## 目标

从零创建一个 CTP autobuild 持续集成仓库。

## 步骤

1. 填写输入表：CI 仓名、产品名、产品编码、MCU/MPU 仓、MPU 型号和脚本族、NFCreator 仓、Docker 镜像、partition 来源、安全策略。
2. 创建目录骨架：`manifests/`、`build/`、`config/<product>/`、`package/`、`tools/`、`doc/autobuild/<project>/`。
3. 从最接近的项目复制 `build/` 和 `tools/`，删除旧产品配置和产物。
4. 编写 `mcu_<product>.xml`、`mpu_<product>.xml`、`mcu_master.xml`。
5. 编写 `config/<product>/list.env`，先放最小字段和最小模块清单。
6. 准备 NFCreator `Images/<CODE>/`。新项目优先将 partition 放在 NFCreator 产品目录，不依赖旧兼容目录。
7. 编写项目层 README、差异表、FAQ。
8. 本地跑 Step1、Step2、Step3。
9. 三阶段通过后再配置 Jenkins。

## 阻断项

- 缺少产品编码。
- 不知道 MPU 型号到脚本族的映射。
- NFCreator `project path` 不明确。
- partition 未准备。
- 要求把私钥直接入仓。
## 时间属性模板

每份项目层文档标题后必须加入：

```markdown
> 时间属性
> - 文档生成时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 仓库扫描基准时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
> - 适用仓库/分支：<repo>/<branch 或 revision>
> - 适用产品配置：<product list>
> - 时效性说明：本文基于上述时间点的 manifests/config/build/tools 生成；仓库分支、Jenkins 参数、NFCreator、partition 或 list.env 变化后需要复核。
> - 建议复核周期：新增产品、切分支、调整 Jenkins、升级 SDK/工具链、修改 secure OTA/partition 或每次发版前复核。
```

不要使用无时间锚点的“当前”“最新”“现在”。必须绑定具体日期、分支、revision 或来源文件。
