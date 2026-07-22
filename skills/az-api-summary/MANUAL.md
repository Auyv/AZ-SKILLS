# az-api-summary 手动执行指南

## 目标

把 CHM、PDF 或 Doxygen HTML 接口资料整理成带术语表、分层架构、API 清单、调用流程和项目范围映射的 Markdown。

## 准备输入

至少准备一份接口源文档。可选准备：

- 用户手册：确定整体架构和移植边界。
- API 示例文档：确定推荐调用顺序和限制。
- 参考排版文档：只借鉴章节组织。
- 项目说明：确定哪些 SDK 能力允许对外开放。

## 输入校验

在技能目录执行：

```powershell
python scripts/validate_inputs.py input.json
```

`input.json` 示例：

```json
{
  "source_documents": [
    "C:\\docs\\vendor-api.chm",
    "C:\\docs\\vendor-api-examples.pdf"
  ],
  "reference_document": "C:\\docs\\style-reference.pdf",
  "audience": "密码学初学者和 Linux 应用开发者",
  "overwrite": false
}
```

## 解包 CHM

Windows 自带 HTML Help 时执行：

```powershell
New-Item -ItemType Directory -Force -Path .ai_tmp\api-summary\chm
Start-Process -FilePath C:\Windows\hh.exe `
  -ArgumentList @('-decompile', '.ai_tmp\api-summary\chm', 'C:\docs\vendor-api.chm') `
  -WindowStyle Hidden -Wait
```

确认解包目录中存在 `index.html`、`modules.html`、`group_*.html` 或 `*_source.html`。

## 提取 Doxygen API

```powershell
python scripts/extract_doxygen_api.py `
  .ai_tmp\api-summary\chm `
  .ai_tmp\api-summary\api_manifest.json
```

脚本需要 `lxml`。若环境中没有，不要自动安装；可直接从 `group_*.html` 和 `*_source.html` 人工提取，或使用已有 HTML 解析工具。

## 解析 PDF

1. 提取书签、目录和正文文本。
2. 渲染架构图、函数概览、API 详情和流程图页面。
3. 对照页面检查表格列、箭头方向和备注框。
4. 参考文档只提炼篇章结构，不复制内容。

## 建立分层

按“文档明示优先、目录和调用关系次之”的顺序分类：

```text
业务应用
  -> 公共 API
    -> 命令/APDU
      -> 协议抽象/TPDU
        -> 链路协议
          -> 硬件适配/Port
```

明确标记每个模块是：业务直接调用、SDK 内部、平台实现，还是当前项目不开放。

## 编写 Markdown

复制 `assets/api-summary-template.md`，依次填写：

1. 来源、版本、受众和适用范围。
2. 术语表及容易混淆的概念。
3. 分层架构图和职责表。
4. 公共 API 完整清单。
5. 各模块函数概览和原型。
6. 内部层接口索引。
7. 核心结构体和错误模型。
8. 推荐调用流程。
9. 项目能力映射与安全限制。

## 最终检查

- 公共 API 清单与 Markdown 做集合差，结果必须为空。
- 术语表覆盖正文中的缩写、算法、模式、填充和 ID 语义。
- 代码围栏数量为偶数。
- UTF-8 解码无 `U+FFFD replacement character`。
- 不把内部函数写成业务 API。
- 不把 SDK 全量能力自动扩展为当前项目能力。
