# C代码规范

## 1. 命名规范

### 文件命名
- 全小写字母 + 下划线分隔
- 示例：`xrpc_codec.c`, `xrpc_link.h`

### 函数命名

#### 对象方法
- 格式：`TXxxx(TXxxTag *self, ...)`
- 首参数为对象指针 `self`
- 函数名以对象类型名开头：`TTypeName + 动作`
- 示例：
  - `TNfUpgradeAppRunOnce(TNfUpgradeApp *self)`
  - `TCIniGetKeyValueString(TCIni *self, ...)`

#### 功能函数
- 大驼峰命名法（PascalCase）
- 模块前缀 + 功能描述
- 示例：`CmdLine(...)`, `CmdDownloadImg(...)`, `http_download_file(...)`

### 变量命名

| 类别   | 风格         | 示例                              |
| ---- | ---------- | ------------------------------- |
| 全局变量 | `g_`前缀+下划线 | `g_mpu_upgrade_`, `g_file_log_` |
| 局部变量 | 全小写+下划线    | `file_handle`, `cmdparams`      |
| 静态变量 | 与全局变量相同    | `g_file_log_cache_buf`          |

### 类型命名

| 类别 | 风格 | 示例 |
|------|------|------|
| 结构体 | `T`前缀+Tag+typedef | `typedef struct TNfUpgradeAppTag TNfUpgradeApp` |
| 枚举 | `enum TxxxTag { ... }; typedef enum TxxxTag Txxx;` | `enum TNfUpgradeStateTag { ... }; typedef enum TNfUpgradeStateTag TNfUpgradeState;` |
| 枚举值 | `k`前缀+模块+描述 | `kNfUpgradeStateIdle` |

### 宏定义
- 全大写字母 + 下划线分隔
- 模块前缀
- 示例：`FILE_LOG_MAX_SIZE`, `RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE`

---

## 2. 代码风格

### 缩进和对齐
- 缩进：4个空格（非Tab）
- 结构体成员对齐
- 赋值语句对齐（部分情况）

### 花括号位置
- K&R风格（左花括号行尾，右花括号单独一行）
```c
if (condition)
{
    // code
}
else
{
    // code
}
```

### 空格使用规则
- 运算符前后有空格：`result = 0;`, `i + 1`
- 逗号后有空格：`func(a, b, c)`
- 括号内无空格：`if (condition)`

### 行长度限制
- 建议：80-120字符

---

## 3. 头文件规范

### include顺序
1. 当前模块的头文件（`.c`文件包含对应的`.h`）
2. 项目内部头文件（使用相对路径）
3. 第三方库头文件
4. 系统标准库头文件

### 头文件保护方式
```c
#ifndef _FILENAME_H_
#define _FILENAME_H_

#ifdef __cplusplus
extern "C"
{
#endif

// 函数声明

#ifdef __cplusplus
}
#endif

#endif  ///< _FILENAME_H_
```

---

## 4. 数据类型规范

### 使用的数据类型
- 整数类型：`Int32`, `UInt32`, `Int16`, `UInt16`, `Int8`, `UInt8`
- 布尔类型：`Bool`（`True`/`False`）
- 字符串：`char *` 或 `const char *`
- 句柄类型：`THandle`

### 结构体定义方式
```c
typedef struct TNfUpgradeAppTag TNfUpgradeApp;
struct TNfUpgradeAppTag
{
    UInt8   req_id;
    UInt8   rsp_id;
    Int8    upgrade_precent;
    UInt32  m_state_enter_tick;
    Int32   m_errcode;
    UInt16  m_flag;
    UInt8   m_prev_state;
    UInt8   m_state;
};
```

---

## 5. 函数规范

### 参数数量限制
- 建议：不超过5个参数
- 复杂函数使用结构体封装参数

### 返回值处理
- 成功/失败：返回 `int` 或 `Int32`，0表示成功，负值表示失败
- 布尔值：返回 `Bool` 类型（`True`/`False`）
- 错误码：使用枚举错误码

### 函数出口规范（单一 return 出口）
- **强制要求**：每个函数必须有且只有 1 个 `return` 语句
  - void 函数允许 0 个 `return`，但若有则只能 1 个
  - 禁止在函数体中间使用 `return` 作为提前退出
- **实现风格**：使用"结果变量 + 嵌套 if"模式
  - 在函数顶部声明结果变量（如 `Int32 result = 0;` 或 `Bool ok = TRUE;`）
  - 通过逐层嵌套 `if` 判断决定是否进入下一步逻辑
- **典型改写**：
```c
// 不允许：多 return 出口
Int32 DoWork(TFoo *self)
{
    if (self == NULL) return -1;
    if (self->ready == 0) return -2;
    if (Step1(self) < 0) return -3;
    return Step2(self);
}

// 允许：单 return 出口（结果变量 + 嵌套 if）
Int32 DoWork(TFoo *self)
{
    Int32 result = 0;
    if (self == NULL)
    {
        result = -1;
    }
    else if (self->ready == 0)
    {
        result = -2;
    }
    else
    {
        result = Step1(self);
        if (result >= 0)
        {
            result = Step2(self);
        }
        else
        {
            result = -3;
        }
    }
    return result;
}
```
- **例外**：无。switch-case、循环体内同样禁止提前 `return`；如需跳出循环使用 `break` 并通过标志位汇聚到末尾。

### 错误处理方式
- 日志记录：使用 `ALOG_E()` 记录错误
- 错误传播：函数返回错误码，调用者决定处理方式
- 资源清理：失败时清理已分配资源

---

## 6. 内存和资源管理

### 动态内存使用
- 原则：**禁止** 使用 `malloc`/`calloc`/`realloc`/`free`
- 替代方案：
  - 静态全局变量：`static TFileLog g_file_log_ = {0};`
  - 栈上分配：`char log[128] = {0};`
  - 调用方传入：函数参数传递缓冲区指针

### buffer管理方式
- 固定大小缓冲区：定义宏常量
  - `#define FILE_LOG_RAM_CACHE_SIZE (1024 * 4)`
- 缓冲区初始化：使用 `{0}` 或 `memset` 清零

---

## 7. 日志和调试

### 日志打印方式
- 日志系统：使用 `alog` 库
- 日志标签：使用 `#define ALOG_TAG "ModuleName"`

```c
ALOG_D()  // 调试信息
ALOG_I()  // 一般信息
ALOG_W()  // 警告信息
ALOG_E()  // 错误信息
ALOG_T()  // 跟踪信息
ALOG_HEX() // 十六进制数据
```

### 错误码定义
- 枚举定义：使用枚举类型定义错误码
```c
enum TNfUpgradeErrCodeTag
{
    kNfUpgradeErrCodeSuccess = 1,
    kNfUpgradeErrCodeFailed = 0,
    kNfUpgradeErrCodeMpuUpgrade = -1,
    kNfUpgradeErrCodeMcuUpgrade = -2,
};
```

---

## 8. 模块化设计

### .c和.h的职责划分
- **头文件（.h）**：类型定义、函数声明、宏常量、外部变量声明
- **源文件（.c）**：函数实现、静态变量、模块初始化/清理

### 生命周期函数
- `ModuleCreate()` / `ModuleDestroy()`
- `ModuleInit()` / `ModuleDone()`
- `ModuleRunOnce()`

---

## 9. 注释规范

### 文件头注释
```c
/**
 * @addtogroup module_name
 * module_name
 * @{
 */
/**
 * @file        filename.h
 * @brief       简要描述
 * @note        备注
 * @author      作者(邮箱)
 * @date        日期
 * @version     版本
 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>日期         <th>版本        <th>作者        <th> 描述
 * @copyright 公司名称
 */
```

### 函数注释
```c
/**
 * @brief 函数简要描述
 * @param 参数名 参数描述
 * @return 返回值描述
 * @note 备注
 */
```

### 行内注释
- 单行注释：`//` 或 `///<`（Doxygen风格）
- 多行注释：`/** ... */`
