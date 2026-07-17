# 异常处理指南

## 常见异常

### ERR_FILE_NOT_FOUND

**症状**：返回错误码 `ERR_FILE_NOT_FOUND`

**原因**：
- 输入路径不存在
- 文件被移动/删除
- 路径包含特殊字符未转义

**解决**：
```python
import os
if not os.path.exists(pdf_path):
    # 尝试规范化路径
    pdf_path = os.path.abspath(pdf_path)
    if not os.path.exists(pdf_path):
        print(f"文件不存在: {pdf_path}")
        print(f"当前目录: {os.getcwd()}")
        print(f"目录内容: {os.listdir(os.path.dirname(pdf_path) or '.')}")
```

### ERR_NOT_API_DOC

**症状**：返回错误码 `ERR_NOT_API_DOC`

**原因**：
- PDF 是产品规格书、硬件设计手册、应用笔记
- PDF 内容被加密/扫描质量差
- PDF 已转换但 .md 无函数原型

**解决**：
1. 确认 PDF 类型：搜索"函数原型"、"参数"、"返回值"等关键词
2. 若是扫描版 PDF：使用 `az-pdf2md --heavy` 启用 OCR
3. 若仍识别不到：手动标注关键章节后重试

### ERR_PARSE_FAILED

**症状**：返回错误码 `ERR_PARSE_FAILED`，部分接口未识别

**原因**：
- PDF 转换有乱码
- 章节编号不连续（如从 3.3.1 跳到 3.3.5）
- 函数原型格式特殊

**解决**：
1. 重新执行 `az-pdf2md` 并检查 .md 质量
2. 在 `parsing_rules.md` 中添加新规则
3. 手动补充识别结果

### ERR_OUTPUT_WRITE

**症状**：返回错误码 `ERR_OUTPUT_WRITE`

**原因**：
- 输出目录无写权限
- 磁盘空间不足
- 文件被其他程序占用

**解决**：
```python
import os
output_path = "..."
output_dir = os.path.dirname(output_path)
if not os.access(output_dir, os.W_OK):
    print(f"目录无写权限: {output_dir}")
    output_path = os.path.expanduser(f"~/{os.path.basename(output_path)}")
```

## 性能调优

### 大文件处理

对于超过 200 页的 PDF：
1. 分章节处理（按 3.x 模块切分）
2. 并行处理（multiprocessing）
3. 分批写入（避免内存爆炸）

### 提高准确率

1. 使用 `--quality high` 标志（如果 az-pdf2md 支持）
2. 手动校正关键章节的解析规则
3. 提供参考样本训练解析器

## 已知限制

1. **加密 PDF**：无法处理，需先解密
2. **多语言混合**：函数名带中文时可能漏识别
3. **图片/表格密集**：解析可能丢失部分信息
4. **非 Quectel 文档**：需调整解析规则
