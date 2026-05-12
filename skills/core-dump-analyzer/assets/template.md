# Core Dump 分析报告

## 基本信息

| 项目 | 值 |
|------|-----|
| **Core Dump 文件** | `{core_file}` |
| **可执行文件** | `{executable}` (文件名仅为示例) |
| **崩溃信号** | `{signal}` |
| **崩溃地址** | `{crash_address}` |
| **架构** | `{architecture}` |
| **分析时间** | `{timestamp}` |

---

## 崩溃基本信息

```
{basic_info}
```

---

## 调用栈

```
{call_stack}
```

---

## 寄存器状态

```
{registers}
```

---

## 加载的共享库

```
{shared_libraries}
```

---

## 崩溃地址反汇编

```
{disassembly}
```

---

## 可疑代码分析

### 调用链分析

{call_chain_analysis}

### 可能原因

| 优先级 | 可能原因 | 说明 |
|--------|----------|------|
| 高 | {reason_1} | {reason_1_desc} |
| 中 | {reason_2} | {reason_2_desc} |
| 低 | {reason_3} | {reason_3_desc} |

---

## 代码审查

### 问题代码位置

**文件**: `{problem_file}`
**函数**: `{problem_function}`
**行号**: `{problem_line}`

### 代码问题

```c
{problem_code}
```

### 上下文

```c
{context_code}
```

---

## 调试建议

### 1. 添加日志验证

```c
// {file}:{line}
{suggested_fix}
```

### 2. 验证检查点

- [ ] 检查 `{check_1}`
- [ ] 检查 `{check_2}`
- [ ] 检查 `{check_3}`

### 3. 快速修复方案

{quick_fix}

---

## 相关文件

| 文件路径 | 说明 |
|----------|------|
| `{file_1}` | {file_1_desc} |
| `{file_2}` | {file_2_desc} |

---

## 结论

{conclusion}

---

*报告生成时间：{timestamp}*
*分析工具：ARM Cross-compile GDB + 静态代码审查*
