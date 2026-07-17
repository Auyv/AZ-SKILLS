# 使用指南

## 完整流程

### 场景：从 PDF 提取 API 清单

**输入**：`D:\模组资料\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.pdf`

**步骤**：

1. **PDF → MD 转换**（使用 az-pdf2md）
   ```bash
   python "C:\Users\jpz\.agents\skills\az-skills\az-pdf2md\scripts\main.py" "D:\模组资料\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.pdf"
   ```
   输出：`D:\模组资料\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.md`

2. **解析 .md 提取 API**
   ```python
   from scripts.parse_md_api import parse_api_doc
   
   result = parse_api_doc("D:\\模组资料\\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.md")
   print(f"模块数: {len(result['modules'])}")
   print(f"函数总数: {sum(len(m['functions']) for m in result['modules'].values())}")
   ```

3. **生成接口清单 .md**
   ```python
   from scripts.generate_api_list import generate
   
   generate(
       md_path="D:\\模组资料\\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.md",
       output_path="D:\\模组资料\\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0_接口清单.md",
       title="QTEE API 接口清单"
   )
   ```

4. **在原 .md 追加索引**
   ```python
   from scripts.append_index import append_index
   
   append_index(
       md_path="D:\\模组资料\\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0.md",
       list_md_path="D:\\模组资料\\Quectel_AG35系列_QuecOpen_QTEE_API_参考手册_V1.0_接口清单.md",
       api_count=43
   )
   ```

## 解析规则

详见 [parsing_rules.md](parsing_rules.md)

## 异常处理

详见 [troubleshooting.md](troubleshooting.md)
