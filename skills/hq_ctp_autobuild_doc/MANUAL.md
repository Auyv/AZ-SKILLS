# hq_ctp_autobuild_doc 手动执行指南

## 步骤

1. 确认 `target_repo`、`output_dir`、`template_dir`。
2. 若 `output_dir` 已存在，先备份。
3. 扫描 `manifests/*.xml`、`config/*/list.env`、`build/`、`tools/`。
4. 建立 Step1/Step2/Step3 表：XML、入口脚本、输入、输出、消费方。
5. 按产品解析 `list.env`，整理产品编码、MPU 型号、HUBS 版本、模块清单和开关。
6. 生成 README、01-11 文档和变量表。
7. 检查 partition 来源和旧兼容目录，写入迁移说明。
8. 检查乱码，不复制乱码原文。
9. 写 changelog 或在最终总结列出更新文件、未确认项、风险。


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
## 验收

- README 能快速定位项目。
- 所有文档都包含时间属性块，能说明生成时间、扫描基准时间、适用分支/revision、产品范围和复核条件。
- 03 差异表能回答本项目与平台模板的差异。
- 05 覆盖所有产品配置。
- 06/07/09 写清入口、参数、产物。
- 10 包含 manifest、list.env、Step 产物、partition 兼容、编码问题。