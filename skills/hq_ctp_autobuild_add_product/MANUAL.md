# hq_ctp_autobuild_add_product 手动执行指南

## 步骤

1. 选择基线产品：从最接近的新产品复制。
2. 复制配置目录：`config/<base_product>` 到 `config/<new_product>`。
3. 修改 `list.env`：产品编码、MPU 型号、HUBS 版本、硬件版本、软件版本、模块清单、安全开关。
4. 复制 XML：`mcu_<base_product>.xml` 到 `mcu_<new_product>.xml`，`mpu_<base_product>.xml` 到 `mpu_<new_product>.xml`。
5. 修改 XML revision 和业务仓清单。
6. 准备 NFCreator `Images/<NEW_CODE>/`。
7. 若当前脚本仍检查旧兼容目录，则补齐 `tools/batch_nfcreator/partition_ini/<new_product>/`，并记录迁移计划。
8. 本地跑 Step1、Step2、Step3。
9. 更新项目层 README、03、05、09、10、11。
10. 更新 Jenkins 产品参数和 XML 选择。

## 常见风险

- 新产品编码未改，导致覆盖旧产品出包。
- XML 复制后 revision 仍指向旧产品分支。
- MPU 型号无法分发到正确 build 脚本族。
- 旧 partition 兼容目录缺失导致 Step3 失败。
- 直接复用旧产品 TEE key。
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
