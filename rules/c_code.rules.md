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

#### 数据流向命名

- 变量名先表达业务用途。业务已定义上下行时，上行使用完整的 `upload`，下行使用完整的 `download`，避免 `upld` 等缩写。
- 在模块注释中明确上下行的业务方向，并在两端保持一致。例如 XCU CAN 上行是 MCU → MPU，下行是 MPU → MCU；MPU 接收上行数据时仍命名为 `upload`。
- 同一方向的队列、缓存、解析状态、会话、序号和统计使用一致的方向词。全局/静态变量采用 `g_<模块>_<方向>_<用途>`，末尾下划线沿用所在模块习惯；结构体成员采用 `<方向>_<用途>`。
- 底层驱动、物理接口的发送/接收状态可使用 `tx`/`rx`；业务上下行队列和缓存应使用 `upload`/`download` 表达其用途。共用状态不强行归入某一方向。
- 对应配置宏采用 `<模块>_UPLOAD_<用途>` / `<模块>_DOWNLOAD_<用途>`。名称区分单块上限、队列总容量和单轮处理预算；字节数、记录数、时间等单位在名称或注释中明确。

```c
static TOfRingBuf g_xcu_can_download_queue_;
static UInt8 g_xcu_can_upload_buffer_[1024];
static UInt32 g_xcu_can_download_sequence_;

/* 结构体成员示例：upload_buffer、download_parser、download_stats。 */

#define RTE_XCU_CAN_DOWNLOAD_BLOCK_MAX       (1024) ///< 单次接收块上限，字节。
#define RTE_XCU_CAN_DOWNLOAD_QUEUE_CAPACITY  (4144) ///< 队列总容量，字节，含内部块头。
#define RTE_XCU_CAN_DOWNLOAD_RUN_BYTE_BUDGET (4096) ///< 每轮消费预算，字节，含内部块头。
```

### 类型命名

| 类别 | 风格 | 示例 |
|------|------|------|
| 结构体 | `T`前缀+Tag+typedef | `typedef struct TNfUpgradeAppTag TNfUpgradeApp` |
| 枚举 | `enum TxxxTag { ... }; typedef enum TxxxTag Txxx;` | `enum TNfUpgradeStateTag { ... }; typedef enum TNfUpgradeStateTag TNfUpgradeState;` |
| 枚举值 | `k`前缀+模块+描述 | `kNfUpgradeStateIdle` |
| 固定位宽的枚举标识 | enum 常量 + 固定宽度整数 typedef；见第4节 | `typedef /*enum TXxxTag*/ UInt32 TXxx;` |

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

### 空行与定义分组

- 独立的结构体、枚举定义组前后保留一行空行，相邻定义组之间共用这一行，避免连续多行空白。
- 枚举定义与其后对应的 typedef（枚举类型或固定宽度整数）紧邻；结构体前置 `typedef struct` 与对应定义紧邻，组内不插入空行。
- 函数定义之间保留一行空行；带注释的函数声明组之间也保留一行空行。说明注释与对应声明或定义紧邻，空行放在上一组结尾与下一组注释之间。
- 同一结构体的成员、同一枚举的枚举值默认连续排列；仅在确有不同用途的分组时增加空行。

```c
enum TDemoModeTag
{
    kDemoModeMessage = 0,
    kDemoModeDigest  = 1,
};
typedef enum TDemoModeTag TDemoMode;

typedef struct TDemoBufferTag TDemoBuffer;
struct TDemoBufferTag
{
    UInt8 *buf;
    UInt32 buf_size;
};

/**
 * @brief 初始化模块。
 */
Int32 DemoInit(void)
{
    return 0;
}

/**
 * @brief 关闭模块。
 */
void DemoDone(void)
{
}
```

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

### 固定位宽的枚举标识

- 公共 ABI、跨平台接口、协议或持久化字段需要稳定宽度时，使用 `enum TXxxTag` 定义具名常量，再使用固定宽度整数定义语义类型 `TXxx`。枚举常量、接口参数和字段统一围绕这一类型命名，避免为同一概念再定义另一套类型名。
- 标准写法为 `typedef /*enum TXxxTag*/ UInt32 TXxx;`，紧邻枚举定义之后。注释保留常量与语义类型的对应关系，实际类型是 UInt32；这不会固定 enum TXxxTag 本身的位宽。
- 函数参数、返回值及结构体字段使用 `TXxx`，不直接使用 `enum TXxxTag`。该类型不受编译器短枚举选项影响；`typedef enum TXxxTag TXxx` 本身不保证固定宽度。
- 按范围选择 UInt8、UInt16、UInt32 等固定宽度类型；包含负值时选择对应的 Int 类型。全部常量必须落在所选整数类型的范围内，外部输入仍需检查合法取值。
- 修改已有接口时检查类型宽度、结构体布局和调用方兼容性；有变化时同步接口版本。需要稳定 ABI 的类型应在目标编译环境检查 sizeof，必要时同时检查成员偏移。
- 无固定宽度要求的模块内部枚举可继续使用普通 enum typedef。本约定也不代表可以直接复制结构体作为通信报文，协议字节序和序列化另行定义。

```c
enum TDemoAlgorithmIdTag
{
    kDemoAlgorithmAes128 = 0x1101,
    kDemoAlgorithmSm2    = 0x2320,
};
typedef /*enum TDemoAlgorithmIdTag*/ UInt32 TDemoAlgorithmId;

TDemoAlgorithmId algorithm_id = kDemoAlgorithmSm2;
```

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

### 输入输出参数与缓冲区

- 输入参数与输出结果不得混在同一个传参结构体中。需要封装时，输入命名为 `TXxxIParam`，输出命名为 `TXxxOParam`，函数使用 `const TXxxIParam *in`、`TXxxOParam *out`。
- IParam 保存调用方提供的操作配置、只读数据及结果存储配置；OParam 只保存函数返回的长度、状态等结果，不要求调用方预填存储地址或大小。
- 只读数据使用 `const UInt8 *`。结果存储使用 `UInt8 *xxx_buf` 和 `UInt32 xxx_buf_size`，放在 IParam；实际字节数使用 `UInt32 xxx_len`，放在 OParam。避免以 capacity、length 两个含糊字段混合表达调用前配置与调用后结果。
- `const IParam *` 保证结构体字段不变；其中的可写 buf 仍允许写入。不得修改原输入数据或通过去除 const 写入字段；需要调整存储地址时复制一份输入配置后修改副本。
- buf_size 表示最多可写的字节数，len 表示成功时的实际字节数，单位在注释中明确。结果存储不足时返回对应错误，len 回填所需字节数，不写入部分数据；其他失败时结果长度清零。不得超过 buf_size 写入。
- 输入结构、原输入数据和结果存储由调用方管理，调用期间保持有效。接口说明必须明确空指针、输入输出重叠及异步调用的生命周期要求。简单接口可使用分开的普通参数，无需为单个标量强行建结构体。

```c
typedef struct TDemoProcessIParamTag TDemoProcessIParam;
struct TDemoProcessIParamTag
{
    const UInt8 *input_data;      ///< 调用期间只读的原始数据。
    UInt32       input_len;       ///< 输入字节数。
    UInt8       *output_buf;      ///< 调用方提供的可写结果存储。
    UInt32       output_buf_size; ///< 最多可写的字节数。
};

typedef struct TDemoProcessOParamTag TDemoProcessOParam;
struct TDemoProcessOParamTag
{
    UInt32 output_len; ///< 实际字节数，结果存储不足时为所需字节数。
};

/**
 * @brief 处理输入数据，将结果写入调用方提供的存储。
 * @param[in] in 操作配置和结果存储配置，结构体字段不变。
 * @param[out] out 实际或所需结果长度，其他失败时为零。
 * @return 0 成功，负值为错误码。
 */
Int32 DemoProcess(const TDemoProcessIParam *in, TDemoProcessOParam *out);
```

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
