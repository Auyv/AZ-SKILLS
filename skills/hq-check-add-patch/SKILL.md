---
name: hq-check-add-patch
description: 从构建入口脚本递归扫描整个构建过程，找出对目标目录的所有文件替换/修改操作，输出到 Excel 文件。触发场景：用户提到"检查patch替换"、"扫描SDK替换"、"构建替换分析"、"check add patch"、"check_add_patch"、或提供构建脚本路径分析文件替换情况。
version: 1.0.0

inputs:
  required:
    - name: entry_script
      type: string
      description: 构建入口脚本的绝对路径（如 build/mpu/build_ag35_evd.sh）
      validation: 必须是存在的 .sh 文件
    - name: target_variable
      type: string
      description: 需要追踪的目标变量名（如 G_SDK_PATH）。只要命令的源或目标路径引用了此变量或其子变量（如 G_SDK_ROOTFS_PATH），就会被收录
      validation: 非空字符串，通常以 G_ 或 SDK_ 开头
  optional:
    - name: output_path
      type: string
      description: Excel 输出路径（绝对路径）。默认在 entry_script 所在目录生成 <目标变量>_替换清单.xlsx
      default: <entry_script_dir>/<target_variable>_替换清单.xlsx
    - name: max_depth
      type: integer
      description: 最大递归深度（防止无限循环）。超过此深度后跳过
      default: 20

outputs:
  - name: excel_file
    type: file
    description: Excel 文件（.xlsx），包含 9 列
    format: 9 列制表数据
    columns:
      - "序号": 从 1 开始的整数
      - "执行脚本": 相对于 entry_script 所在项目根目录的脚本路径
      - "行号": 脚本中的行号（范围）
      - "操作类型": cp / cp -rf / ln -sf / mv / rm -rf / tar -zxvf / echo >> / sed -i / mkdir / touch
      - "源文件路径": 源文件路径表达式（保留变量引用）
      - "目标文件路径": G_SDK_PATH 内的目标路径表达式（保留变量引用）
      - "原始内容(被替换前)": 被替换的原始目标路径（变量引用形式）
      - "影响的SDK子目录": 目标路径在 G_SDK_PATH 下的一级子目录（如 tools/、ql-ol-rootfs/ 等）
      - "备注": 操作用途的文字说明（自动生成或需人工补充）
  - name: summary
    type: object
    description: 扫描结果摘要
    format: |
      {
        "total_entries": 整数,
        "scripts_scanned": 整数,
        "subdir_stats": {"子目录名": 条目数, ...},
        "op_stats": {"操作类型": 条目数, ...}
      }

errors:
  - code: ERR_ENTRY_NOT_FOUND
    message: 入口脚本文件不存在
    handling: 检查路径是否正确，确认文件存在且为 .sh 文件
  - code: ERR_VAR_NOT_FOUND
    message: 在全量脚本中未找到目标变量的定义
    handling: 检查变量名拼写，确认变量在构建脚本中被 export 声明
  - code: ERR_OPENPYXL_MISSING
    message: 缺少 openpyxl 库
    handling: 运行 pip install openpyxl 安装，或回退到 CSV 格式输出
  - code: ERR_NO_OPERATIONS
    message: 扫描完成但未发现任何匹配的替换操作
    handling: 检查 target_variable 是否正确，或目标目录是否未被任何脚本修改
  - code: ERR_RECURSION_LIMIT
    message: 递归深度超过限制
    handling: 增大 max_depth 参数，或检查脚本中是否存在循环引用
  - code: ERR_INVALID_VARIABLE
    message: target_variable 不在可追踪的变量集中
    handling: 确认变量名正确，变量在构建入口脚本中被 defined 或 inherited
---

# hq-check-add-patch

## 做什么

从嵌入式构建入口脚本出发，递归追踪所有被调用的子脚本，扫描其中对指定目标目录（如 SDK 目录）的文件替换/修改操作，生成结构化的 Excel 报告。

### 核心能力

1. **变量解析**：识别 `export VAR=value` 定义，追踪变量引用链（如 `G_SDK_PATH → G_SDK_ROOTFS_PATH → G_SDK_ROOTFS_PATH/bin`）
2. **递归追踪**：自动发现并扫描 `sh script.sh`、`source env.sh`、`. file.sh`、`bash -c` 等脚本调用
3. **操作识别**：检测 `cp`、`cp -rf`、`ln -sf`、`mv`、`rm -rf`、`tar -zxvf`、`echo >>`、`sed -i`、`mkdir`、`touch` 等文件操作
4. **子目录分类**：自动将目标路径归类到 `<目标变量>` 的一级子目录（如 `tools/`、`ql-ol-rootfs/`、`oemapp/`）
5. **Excel 输出**：9 列表格，带样式、筛选、冻结表头

## 何时使用

满足任一条件时激活：

- 用户提到"检查 patch 替换"、"扫描 SDK 替换"、"构建替换分析"、"check add patch"、"check_add_patch"
- 用户提供构建脚本路径，要求分析文件替换情况
- 用户说"从 XX.sh 扫描所有 G_SDK_PATH 替换"
- 用户想了解构建过程中哪些文件被修改/替换了

显式调用格式：`/hq-check-add-patch <entry_script> <target_variable> [output_path]`

## 怎么做

### 步骤 1：验证输入

```python
# 使用 scripts/scan_build.py 中的 validate_inputs()
from scripts.scan_build import validate_inputs, ScanContext

ctx = validate_inputs(entry_script, target_variable, output_path, max_depth)
```

### 步骤 2：解析变量定义

扫描入口脚本及其 source 的 env 文件，建立变量解析表：

```python
# entry_script 中定义:
#   export G_SDK_PATH=${SDK_PATH}
#   export G_SDK_ROOTFS_PATH=${G_SDK_PATH}/ql-ol-rootfs
#
# → 变量表：
#   G_SDK_PATH → ${SDK_PATH}
#   G_SDK_ROOTFS_PATH → ${SDK_PATH}/ql-ol-rootfs
```

变量解析规则：
- `export VAR=${OTHER_VAR}/sub/path` 记录原始表达式
- 后续使用时展开到一级（保留 `${G_SDK_PATH}` 引用）
- `${VAR}` 和 `$VAR` 两种语法均支持

### 步骤 3：递归扫描脚本

```
entry_script.sh
├── [source] list.env          ← 扫描
├── [source] sdk.env           ← 扫描
├── [sh] build_partition.sh    ← 递归进入
├── [sh] build_app.sh          ← 递归进入
│   ├── [sh] update_sdk_so.sh  ← 继续进入
│   └── [sh] load_mpu_ver.sh   ← 跳过（不涉及目标变量）
└── [sh] build_images.sh       ← 递归进入
    └── [source] selinux/import.sh ← 继续进入
```

递归规则：
- 匹配模式：`sh <path>`、`source <path>`、`. <path>`、`bash <path>`
- 跳过条件：路径不在文件系统中、已扫描过（防止循环）、深度超限
- 相对路径相对于当前脚本所在目录解析

### 步骤 4：识别文件操作

对每个脚本的每一行，检测是否包含以下操作且目标路径引用了目标变量：

| 操作模式 | 示例 |
|---------|------|
| `cp -rf SRC DST` | `cp -rf ${G_PROJ_KERNEL_PATH}/dts/*.dtsi ${G_SDK_KERNEL_DTS_PATH}/` |
| `cp SRC DST` | `cp ${G_PROJ_BUILD_CONFIG_PATH}/diffupdate.cfg ${G_SDK_FOTA_TOOL}/config/` |
| `ln -sf SRC DST` | `ln -sf /bin/hqtoolbox ${G_SDK_ROOTFS_PATH}/bin/hqstart` |
| `mv SRC DST` | `mv ${UPDATE_FIRMWARE_PATH}/rpm.mbn ${SE_TOOL_FIRMWARE_PATH}` |
| `rm -rf PATH` | `rm -rf ${G_SDK_USRDATA_PATH}` |
| `tar -zxvf SRC -C DST` | `tar -zxvf tool.tar.gz -C ${G_SDK_TOOLS_PATH}` |
| `echo TEXT >> FILE` | `echo "Build Date: ..." >> ${G_SDK_ROOTFS_PATH}/etc/version` |
| `sed -i EXPR FILE` | `sed -i "s/old/new/g" ${G_SDK_ROOTFS_PATH}/etc/config` |
| `mkdir PATH` | `mkdir -p ${G_SDK_ROOTFS_PATH}/mnt/card` |
| `touch FILE` | `touch ${G_SDK_USRDATA_DATA_PATH}/emmc_format.flag` |
| `${FAKEROOT} cp -rf ...` | `${G_SDK_FAKEROOT_TOOL_PATH} cp -rf ... `${G_SDK_ROOTFS_PATH}/bin`` |

### 步骤 5：生成 Excel

执行 `scripts/generate_excel.py`，使用 openpyxl 生成 9 列 Excel 文件。

### 步骤 6：返回结果

输出标准结构：

```json
{
  "status": "success",
  "data": {
    "excel_path": "C:/.../G_SDK_PATH_替换清单.xlsx",
    "total_entries": 70,
    "scripts_scanned": 8,
    "subdir_stats": {
      "ql-ol-rootfs": 25,
      "tools": 20,
      "oemapp": 17,
      "ql-ol-kernel": 7,
      "selinux": 6,
      "ql-sysroots": 4,
      "usrdata": 3
    },
    "op_stats": {
      "cp -rf": 35,
      "cp": 10,
      "ln -sf": 15,
      "rm -rf": 3,
      "tar -zxvf": 1,
      "echo >>": 3,
      "mkdir": 1,
      "touch": 1,
      "sed -i": 1
    }
  }
}
```

### 实现脚本

主要逻辑分布在两个 Python 脚本中：

```
hq-check-add-patch/
├── SKILL.md
├── MANUAL.md
├── scripts/
│   ├── scan_build.py        # 扫描引擎：变量解析 + 递归追踪 + 操作识别
│   └── generate_excel.py    # Excel 生成器：数据转 9 列 Excel
└── evals/
    └── evals.json           # 测试用例
```

**执行流程**：

```
用户输入 → scan_build.py（扫描+提取）
         → generate_excel.py（格式化为Excel）
         → 返回 summary + 文件路径
```

## 异常场景

| 异常场景 | 输入 | 预期行为 | 错误码 |
|---------|------|---------|--------|
| 入口脚本不存在 | entry_script="no_such.sh" | 返回文件不存在错误 | ERR_ENTRY_NOT_FOUND |
| 目标变量未定义 | target_variable="UNDEFINED_VAR" | 扫描完成但无匹配，提示检查变量名 | ERR_VAR_NOT_FOUND |
| 缺少 openpyxl | 系统未安装 openpyxl | 提示安装或回退 CSV | ERR_OPENPYXL_MISSING |
| 无任何替换操作 | target_variable 正确但无匹配 | 返回空报告 | ERR_NO_OPERATIONS |
| 循环引用 | 脚本 A 调 B，B 调 A | 检测重复，跳过已扫描脚本 | ERR_RECURSION_LIMIT |
| 输出路径无权限 | output_path 指向无写权限目录 | 降级到 entry_script 所在目录 | — |

## 注意事项

1. 此技能仅分析 Shell 脚本中的静态引用，不执行脚本
2. 变量引用支持 `${VAR}` 和 `$VAR` 两种格式
3. 不解析条件分支（if/else），所有路径均被收录
4. `cp -rf` 通配符（`*.so`、`*`）在 Excel 中保留原始表达式
5. 操作类型为循环（for loop）时，行号标注为"行A-行B (循环)"，源/目标保留宏变量
