# QTEE API 接口清单（已处理示例）

**源文档**：Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0
**头文件**：`qteecrypto.h`（位于 `ql-sysrootfs/usr/include/qtee`）
**接口总数**：43 个

---

## 1. 接口总览

| 模块 | 接口数 |
|------|--------|
| RSA | 15 |
| AES | 12 |
| ECC | 13 |
| 安全存储 | 3 |
| **合计** | **43** |

> 本文件为 `az-api-list-from-pdf` 技能处理示例。完整版接口清单见：
> `Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0_接口清单.md`

---

## 2. 处理流程示例

### 输入

```
D:\【模组资料】\Quectel 移远\AG35Evd_高通\软件资料\Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0.pdf
```

### 处理步骤

1. **PDF → MD**（az-pdf2md）
   ```
   输入：Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0.pdf
   输出：Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0.md (103 KB, 62 页)
   ```

2. **解析 + 生成清单**（az-api-list-from-pdf）
   ```
   输入：Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0.md
   输出：Quectel_AG35系列(R12)_QuecOpen_QTEE_API_参考手册_V1.0_接口清单.md (13 KB)
   提取：43 个函数（RSA 15 / AES 12 / ECC 13 / 安全存储 3）
   ```

3. **追加索引到原 .md**
   ```
   章节：5. 接口清单索引
   链接：xxx_接口清单.md
   ```

### 耗时

| 步骤 | 时长 |
|------|------|
| PDF → MD | 30 秒 |
| 解析 + 生成清单 | 10 秒 |
| 追加索引 | < 1 秒 |
| **合计** | **~40 秒** |
