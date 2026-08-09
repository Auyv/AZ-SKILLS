# hq-check-add-patch 手动操作指南

> 如果无法使用 AI Agent，可按照以下步骤手动执行。

## 前置条件

- Python 3.6+
- `pip install openpyxl`（如需 .xlsx 格式）
- 项目构建脚本（.sh 格式）

## 手动执行步骤

### 步骤 1：准备输入

确认以下信息：
- **入口脚本路径**：构建流程的顶层脚本（如 `build/mpu/build_ag35_evd.sh`）
- **目标变量名**：要追踪的变量名（如 `G_SDK_PATH`）
- **输出路径**（可选）：Excel 输出位置

### 步骤 2：运行扫描引擎

```bash
cd <项目根目录>
python skills/hq-check-add-patch/scripts/scan_build.py \
    build/mpu/build_ag35_evd.sh \
    G_SDK_PATH \
    扫描结果.json
```

扫描引擎会：
1. 解析入口脚本中的变量定义
2. 递归追踪所有 `sh`/`source`/`.` 调用的子脚本
3. 识别 `cp`、`ln -sf`、`mv`、`rm -rf` 等文件操作
4. 筛选出目标路径包含 `G_SDK_PATH` 的操作
5. 输出 JSON 格式的扫描结果

输出示例：

```json
{
  "status": "success",
  "data": {
    "excel_path": "G_SDK_PATH_替换清单.xlsx",
    "total_entries": 70,
    "scripts_scanned": 8,
    "subdir_stats": {"ql-ol-rootfs": 25, "tools": 20, ...},
    "op_stats": {"cp -rf": 35, "ln -sf": 15, ...},
    "entries": [...]
  }
}
```

### 步骤 3：生成 Excel

```bash
python skills/hq-check-add-patch/scripts/generate_excel.py \
    扫描结果.json \
    G_SDK_PATH_替换清单.xlsx
```

### 步骤 4：查看结果

打开生成的 Excel 文件，包含 9 列：
1. 序号
2. 执行脚本
3. 行号
4. 操作类型
5. 源文件路径
6. 目标文件路径
7. 原始内容(被替换前)
8. 影响的SDK子目录
9. 备注

### 一键执行

也可以合并为一条命令：

```bash
python scripts/scan_build.py build/mpu/build_ag35_evd.sh G_SDK_PATH | \
  python -c "
import sys, json, subprocess
data = json.load(sys.stdin)
if data['status'] != 'success':
    print('扫描失败:', data)
    sys.exit(1)
entries = data['data']['entries']
path = data['data']['excel_path']
# 写入临时 JSON
with open('_tmp_scan.json', 'w', encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False)
subprocess.run([sys.executable, 'scripts/generate_excel.py',
                '_tmp_scan.json', path])
print(f'Done: {path}')
"
```

## 常见问题

### Q: 提示 "openpyxl 不可用"
**A**: 运行 `pip install openpyxl`。如果无法安装，脚本会自动回退生成 CSV 格式。

### Q: 扫描到的条目太多/太少？
**A**: 
- 太多：检查目标变量名是否过于宽泛
- 太少：检查变量名拼写是否正确

### Q: 变量引用显示 "${G_SDK_PATH}" 而不是实际路径？
**A**: 正常行为。脚本保留变量引用以便追踪变量链。

### Q: 某些脚本调用的子脚本未被扫描？
**A**: 
- 检查子脚本路径是否使用了变量引用（需能被解析）
- 检查 max_depth 是否足够（默认 20）
- 确认子脚本是 `.sh` 文件

## 故障排除

| 现象 | 可能原因 | 解决方案 |
|------|---------|---------|
| `ERR_ENTRY_NOT_FOUND` | 入口脚本路径错误 | 使用绝对路径 |
| `ERR_NO_OPERATIONS` | 变量名不对或无替换操作 | 确认 export 变量名 |
| 子脚本未递归 | 使用 `bash -c` 或 `exec` | 暂时不支持，手动添加 |
| Excel 列宽不理想 | 路径表达式太长 | 手动调整列宽 |
