# 常见问题排查

## 依赖安装问题

### pdfplumber 安装失败

**症状**：`pip install pdfplumber` 报错

**排查**：
1. 检查Python版本：`python --version`（需要3.8+）
2. 升级pip：`python -m pip install --upgrade pip`
3. 使用国内镜像：`pip install pdfplumber -i https://pypi.tuna.tsinghua.edu.cn/simple`

### MinerU (magic-pdf) 安装失败

**症状**：`pip install "magic-pdf[full]"` 超时或报错

**原因**：MinerU依赖PyTorch，安装包约2GB

**解决方案**：
1. 确保网络稳定，使用国内镜像安装PyTorch
2. 或者仅使用pdfplumber模式（不安装MinerU）
3. 如果已安装PyTorch，可尝试 `pip install magic-pdf`（不带 [full]）

### Tesseract OCR 未找到

**症状**：`pytesseract.pytesseract.TesseractNotFoundError`

**解决**：
1. Windows: 下载安装 https://github.com/UB-Mannheim/tesseract/wiki
2. 安装后将Tesseract路径加入PATH，或设置环境变量：
   ```python
   import pytesseract
   pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
   ```

## 转换质量问题

### 标题层级识别不准确

**原因**：pdfplumber依赖字体大小推断标题层级，部分PDF字体大小不规范

**改善方法**：
1. 使用MinerU模式转换：`--mode mineru`
2. 转换后手动调整MD中的标题层级

### 代码块未正确包裹

**原因**：PDF中的代码格式多样，启发式检测可能漏判

**改善方法**：
1. 运行优化脚本：`python scripts/optimize_md.py "file.md"`
2. 检查 `assets/md_cleanup_patterns.json` 中的代码检测规则，按需添加

### 表格内容错乱

**原因**：PDF表格结构复杂（合并单元格、跨页表格等）

**改善方法**：
1. 运行优化脚本修复基本格式问题
2. 对于复杂表格，可能需要手动调整

### 扫描件识别效果差

**原因**：扫描件质量低、分辨率不足、倾斜等

**改善方法**：
1. 使用MinerU模式（对扫描件优化更好）
2. 确保安装了中文语言包：`tesseract --list-langs` 应包含 `chi_sim`
3. 提高扫描件分辨率（建议300DPI以上）

## 文件处理问题

### 提示"PDF已加密"

**解决**：
1. 使用知道密码的PDF阅读器打开并另存为未加密版本
2. 使用qpdf工具：`qpdf --decrypt input.pdf output.pdf`

### 路径包含中文/空格报错

**解决**：
1. 确保使用Python 3.8+
2. 路径用引号包裹：`python scripts/main.py "C:\我的文档\测试.pdf"`
3. 或使用原始字符串：`r"C:\我的文档\测试.pdf"`

### 转换超时

**原因**：PDF文件过大或页面过多

**解决**：
1. MinerU模式默认超时10分钟，可修改 `convert_pdf.py` 中的 `timeout` 参数
2. 对于超大PDF，考虑分批处理
