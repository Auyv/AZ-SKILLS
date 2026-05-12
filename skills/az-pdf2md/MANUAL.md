# PDF转MD 手动操作指南

本文档供没有AI Agent的用户手动执行PDF转MD操作。

## 环境要求

- Python 3.8+
- pip 包管理器

## 一、安装依赖

打开终端，执行以下命令安装核心依赖：

```bash
pip install pdfplumber Pillow pytesseract
```

如需OCR识别扫描件，还需安装Tesseract OCR引擎：
- Windows: 下载安装 https://github.com/UB-Mannheim/tesseract/wiki
- macOS: `brew install tesseract`
- Linux: `sudo apt install tesseract-ocr tesseract-ocr-chi-sim`

如需MinerU高级模式（技术文档还原度更高）：

```bash
pip install "magic-pdf[full]"
```

注意：MinerU依赖PyTorch，安装包约2GB。

## 二、使用方式

### 方式一：命令行一键转换

```bash
cd az-sdk-pdf2md
python scripts/main.py "你的PDF文件路径.pdf"
```

批量转换（逗号分隔）：

```bash
python scripts/main.py "file1.pdf, file2.pdf, file3.pdf"
```

指定转换模式：

```bash
python scripts/main.py "file.pdf" --mode pdfplumber
python scripts/main.py "file.pdf" --mode mineru
```

### 方式二：分步执行

#### 2.1 检查依赖

```bash
python scripts/install_deps.py
```

输出 `[OK]` 表示已安装，`[FAIL]` 表示需要手动安装。

#### 2.2 验证路径

```bash
python scripts/validate_paths.py "你的PDF文件路径.pdf"
```

输出 `[OK]` 表示路径有效，`[ERR]` 表示路径有问题。

#### 2.3 转换单个PDF

```bash
python scripts/convert_pdf.py "你的PDF文件路径.pdf" "输出路径.md"
```

第三个参数可选模式：`auto`（默认）、`pdfplumber`、`mineru`。

#### 2.4 优化MD文件

```bash
python scripts/optimize_md.py "生成的MD文件.md"
```

此步骤会清理页码、水印、修复表格格式、包裹代码块。

## 三、输出说明

- MD文件生成在PDF文件的**同级目录**下
- 文件名与PDF相同，后缀改为 `.md`
- 编码格式：UTF-8
- 不会修改原始PDF文件

## 四、常见问题

### Q: pdfplumber安装失败？

```bash
pip install --upgrade pip
pip install pdfplumber
```

如果仍然失败，检查Python版本是否>=3.8。

### Q: 扫描件PDF转换效果不好？

1. 确保安装了pytesseract和Tesseract OCR引擎
2. 使用MinerU模式：`python scripts/main.py "scan.pdf" --mode mineru --heavy`
3. MinerU对扫描件的识别效果优于pdfplumber

### Q: 转换后的MD表格错乱？

运行优化脚本修复：

```bash
python scripts/optimize_md.py "输出的MD文件.md"
```

### Q: PDF加密无法转换？

需要先解除PDF加密。可以使用：
- Adobe Acrobat（输入密码后另存为未加密版本）
- 命令行工具：`qpdf --decrypt encrypted.pdf decrypted.pdf`

### Q: 如何使用MinerU模式？

```bash
# 先安装MinerU（约2GB依赖）
pip install "magic-pdf[full]"

# 使用MinerU模式转换
python scripts/main.py "file.pdf" --mode mineru
```
