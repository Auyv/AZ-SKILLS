# C代码规范

## 1. 命名规范

### 文件命名
- 全小写字母 + 下划线分隔
- 示例：`xrpc_codec.c`, `xrpc_link.h`
- `IXxx` 接口集对象对应 `xxx_if.h/.c`，例如 `IGpio` 对应 `gpio_if.h/.c`；`TXxx` 具体对象默认对应 `xxx.h/.c`，文件名不带 I/T 类型前缀。

### 函数命名

#### 对象方法
- 格式：`TXxxx(TXxxTag *self, ...)`
- 首参数为对象指针 `self`
- 函数名以对象类型名开头：`TTypeName + 动作`
- 示例：
  - `TNfUpgradeAppRunOnce(TNfUpgradeApp *self)`
  - `TCIniGetKeyValueString(TCIni *self, ...)`

#### 接口集对象的成员函数

- 成员函数首参数为当前接口集指针，默认使用 `const IXxx *self`。
- 对应安全调用函数使用“接口集类型名 + 成员名”，例如 `IGpioWrite(const IGpio *self, ...)`。
- 安全调用层只检查成员函数指针，不检查 `self` 或其他参数；详见第8节“接口集对象（IXxx）”。

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
| 具体对象或参数结构体 | `T`前缀+Tag+typedef | `typedef struct TNfUpgradeAppTag TNfUpgradeApp` |
| 接口集对象 | `I`前缀+Tag+typedef，只包含函数指针 | `typedef struct IGpioTag IGpio` |
| 枚举 | 默认使用 `enum TxxxTag { ... }; typedef enum TxxxTag Txxx;` | `enum TNfUpgradeStateTag { ... }; typedef enum TNfUpgradeStateTag TNfUpgradeState;` |
| 枚举值 | `k`前缀+模块+描述 | `kNfUpgradeStateIdle` |
| 固定位宽的枚举标识 | 仅在字节对齐或固定位宽有明确要求时使用；见第4节 | `typedef /*enum TXxxTag*/ UInt32 TXxx;` |

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

### 枚举默认写法

- 枚举值不需要用于字节对齐、结构体固定布局、协议或其他固定位宽约束时，默认使用 `typedef enum TXxxTag TXxx;`，紧邻枚举定义之后。
- 不因枚举位于公共头文件或用于函数参数，就自动改为固定宽度整数别名。先确认是否存在明确的对齐或位宽要求。
- 普通枚举的大小和对齐由编译器及编译选项决定；不能仅依赖 `typedef enum` 保证固定为 1、2 或 4 字节。

```c
enum TCanSocketImplementationTag
{
    kCanSocketImplementationLegacy = 0,
    kCanSocketImplementationBatch = 1,
};
typedef enum TCanSocketImplementationTag TCanSocketImplementation;
```

### 固定位宽的枚举标识

- 只有字节对齐、固定结构体布局、公共 ABI、跨平台协议或持久化字段明确要求稳定宽度时，才使用 `enum TXxxTag` 定义具名常量，再使用固定宽度整数定义语义类型 `TXxx`。枚举常量、接口参数和字段统一围绕这一类型命名，避免为同一概念再定义另一套类型名。
- 此例外场景使用 `typedef /*enum TXxxTag*/ UInt32 TXxx;`，紧邻枚举定义之后。注释保留常量与语义类型的对应关系，实际类型是 UInt32；这不会固定 enum TXxxTag 本身的位宽。
- 函数参数、返回值及结构体字段使用 `TXxx`，不直接使用 `enum TXxxTag`。该类型不受编译器短枚举选项影响；`typedef enum TXxxTag TXxx` 本身不保证固定宽度。
- 按范围选择 UInt8、UInt16、UInt32 等固定宽度类型；包含负值时选择对应的 Int 类型。全部常量必须落在所选整数类型的范围内，外部输入仍需检查合法取值。
- 修改已有接口时检查类型宽度、结构体布局和调用方兼容性；有变化时同步接口版本。需要稳定 ABI 的类型应在目标编译环境检查 sizeof，必要时同时检查成员偏移。
- 无上述要求时，无论内部枚举还是公共接口枚举，均采用前述默认 `typedef enum` 写法。固定整数别名也不代表可以直接复制结构体作为通信报文，协议字节序和序列化仍需单独定义。

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

### 参数数量与封装选择

- **除 `self` 外，独立输入、输出参数合计不超过 4 个时，优先直接传参。** 不为接口形式统一而给简单函数创建 IParam/OParam。
- 计数以封装前实际需要的独立输入、输出项为准，输出指针也计入；例如数据指针、数据长度、结果缓冲区、缓冲区容量分别计数，不能先包装成两个结构体后再声称只有两个参数。
- 超过 4 个时，再按复杂度考虑结构体封装，并继续遵守输入、输出分离的规则。IParam、OParam 只是整理参数长度的手段，分别决定是否需要，不要求成对出现；允许只使用其中一种，也允许结构体与直接参数混用，不为配对而额外封装另一侧。
- 输入配置较多、输出只有少量长度、句柄、引用或诊断时，可以使用 `const TXxxIParam *in` 加直接输出指针；输入简单、输出字段较多时，可以使用直接输入加 `TXxxOParam *out`。已有独立业务语义的数据对象（如密钥引用或密钥属性）可以作为一个参数传入，不要求拆开其内部字段；不能仅为减少参数数量而额外制造数据对象。
- 输出项较少时优先直接使用对应指针，不因输入已使用 IParam 就创建 OParam。固定长度操作成功时结果长度已由请求确定，可不重复返回长度，但必须说明成功是否保证完成全部请求。

```c
typedef struct IDeviceTag IDevice;

/* self 不计数，其余四项直接传参；buf 是结果存储，len 返回实际字节数。 */
Int32 IDeviceRead(const IDevice *self, UInt32 id, UInt8 *buf, UInt32 buf_size, UInt32 *len);
```

### 输入输出参数与缓冲区

- 优先按上一节分别判断输入、输出是否需要封装。使用结构体时，输入参数与输出结果不得混在同一个传参结构体中；输入命名为 `TXxxIParam` 并使用 `const TXxxIParam *in`，输出命名为 `TXxxOParam` 并使用 `TXxxOParam *out`。两者无需同时存在；直接传参时同样区分只读输入、可写结果存储和返回结果。
- IParam 保存调用方提供的操作配置、只读数据及结果存储配置；OParam 只保存函数返回的长度、状态等结果，不要求调用方预填存储地址或大小。
- 只读数据使用 `const UInt8 *`。结果存储使用 `UInt8 *xxx_buf` 和 `UInt32 xxx_buf_size`，封装时放在 IParam；实际字节数使用 `UInt32 *xxx_len` 直接输出，或作为 `UInt32 xxx_len` 字段放在确有必要的 OParam。避免以 capacity、length 两个含糊字段混合表达调用前配置与调用后结果。
- `const IParam *` 保证结构体字段不变；其中的可写 buf 仍允许写入。不得修改原输入数据或通过去除 const 写入字段；需要调整存储地址时复制一份输入配置后修改副本。
- buf_size 表示最多可写的字节数，len 表示成功时的实际字节数，单位在注释中明确。结果存储不足时返回对应错误，len 回填所需字节数，不写入部分数据；其他失败时结果长度清零。不得超过 buf_size 写入。
- 输入结构、原输入数据和结果存储由调用方管理，调用期间保持有效。接口说明必须明确空指针、输入输出重叠及异步调用的生命周期要求。简单接口可使用分开的普通参数，无需为单个标量强行建结构体。
- 上述参数校验、缓冲区写入和失败时的结果处理由具体实现负责。接口集的安全调用层只转发参数及返回结果，成员缺失时也不清零或修改输出；是否允许 `NULL` 由具体实现按接口契约说明并检查。

下例展开后包含输入数据、输入长度、输出缓冲区、缓冲区容量和结果长度，共 5 项。输入配置用 IParam 分组，只有一个输出长度，直接传指针即可。

```c
typedef struct TDemoProcessIParamTag TDemoProcessIParam;
struct TDemoProcessIParamTag
{
    const UInt8 *input_data;      ///< 调用期间只读的原始数据。
    UInt32       input_len;       ///< 输入字节数。
    UInt8       *output_buf;      ///< 调用方提供的可写结果存储。
    UInt32       output_buf_size; ///< 最多可写的字节数。
};

/**
 * @brief 处理输入数据，将结果写入调用方提供的存储。
 * @param[in] in 操作配置和结果存储配置，结构体字段不变。
 * @param[out] output_len 实际或所需结果长度，其他失败时为零。
 * @return 0 成功，负值为错误码。
 */
Int32 DemoProcess(const TDemoProcessIParam *in, UInt32 *output_len);
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

### 接口集对象（IXxx）

#### 定义与所属对象

- `IXxx` 表示 interface xxx，统一称为“接口集对象”；`TXxx` 表示保存状态和资源的具体对象。接口集按 `typedef struct IXxxTag IXxx;` 与对应结构体定义组织。
- 接口集只包含函数指针，不放 `context`、`version`、`struct_size`、`role`、驱动句柄或指向其他分组表的数据成员。功能分类使用注释组织，状态和配置放在具体对象中，需要对外查询时提供方法。
- 具体对象内嵌接口集。成员实现通过 `OF_CONTAINER_OF(sender, TConcrete, interface_member)` 按偏移还原所属对象，不通过 `context` 查找，也不假定接口集位于首成员、直接强转为具体对象。
- 接口集指针必须指向对应具体对象内嵌成员的地址。具体对象构造时绑定实现方法；接口集不能复制到其他位置后独立调用，方法也不能绑定安全调用函数自身而形成递归。
- `const IXxx *` 表示调用方不修改函数表；具体对象仍可维护运行状态。构造、借用、在途调用和销毁由所有者管理，接口集与所属对象的生命周期一致，销毁前先结束调用。

#### 成员函数说明

- 每个函数指针声明前都必须紧邻对应的 Doxygen 注释，面向实现该接口的适配者，说明成功时应完成什么。不能只重复方法名称，或以“安全调用 Xxx”代替功能说明。
- `@brief` 描述功能和可观察的目标；`@param[in]`、`@param[out]` 描述参数用途、结果去向及必要的单位、范围、编码和 `NULL` 语义。复杂字段可引用对应的具名参数结构体说明，共有约定可引用接口集说明。
- 有返回值的方法用 `@return` 明确成功结果和主要失败含义，例如不支持、超时、状态不满足或验证不通过；`void` 方法说明执行效果，不编造返回状态。
- 按实际需要用 `@pre`、`@note` 说明调用前置状态、资源所有权、生命周期、并发及锁边界、硬件或持久化副作用、失败后的状态和清理责任。只写已经确认的接口契约，具体实现的限制或尚未确定的行为须明确标注。
- 功能契约写在接口集成员处，安全调用声明可用 `@see IXxx::Method` 引用。注释中的校验、输出处理和资源操作均由具体实现负责，不因此给安全调用层增加检查或处理逻辑。

#### 安全调用边界

- 定义接口集后，在同名 `xxx_if.h/.c` 中提供对应的成员函数安全调用方法，声明和实现分别放在 `.h`、`.c` 中。
- 为减少重复判断，安全调用方法只检查成员函数指针是否为空，不检查 `self`，不添加断言或其他等价的运行时有效性检查。调用者须在外部判断并保证 `self` 非空且指向有效、存活的接口集对象。
- 其余参数全部原样传递，包括 `NULL`、零长度及其他取值。安全调用层不判断参数是否合法，不添加默认值，不初始化、清零或改写输出；具体实现负责参数检查、`NULL` 语义、设备状态及输出处理。
- 成员函数存在时调用一次，原样返回其返回值及输出。成员为空时，使用状态码的 `Int32` 方法返回所在模块的 Unsupported 错误码，`void` 方法不执行动作，输出保持原状；其他返回类型须在接口中明确缺省语义，不伪造操作成功。
- 安全调用层不加锁、不分配资源、不编排生命周期或业务流程。锁和具体操作由实现负责，继续遵守单一 `return` 出口及禁止递归加锁的规则。
- 头文件注释必须明确 `@pre`：调用前由外部保证 `self` 有效；`@note` 说明仅保护空成员调用，其余参数合法性及 `NULL` 语义由具体实现负责。

#### 示例

下例中的基本类型、`OF_CONTAINER_OF` 和 `kGpioUnsupported` 由工程公共头文件提供，`BoardGpioWrite` 表示工程已有的底层驱动方法。

```c
/* gpio_if.h */
typedef struct IGpioTag IGpio;
struct IGpioTag
{
    /**
     * @brief 将指定逻辑管脚设置为给定物理输出电平。
     * @param[in] self 所属具体 GPIO 对象内嵌的接口集。
     * @param[in] pin 驱动映射的逻辑管脚号，不直接假定为硬件引脚号。
     * @param[in] level 物理输出电平，0 为低，1 为高。
     * @return 0 表示电平设置完成，负值表示参数、状态或驱动错误。
     * @pre 具体驱动已将该管脚配置为输出，调用方持有其使用权。
     * @note 实现负责检查管脚及电平，不改变其他管脚或隐式重新初始化。
     */
    Int32 (*Write)(const IGpio *self, UInt32 pin, UInt32 level);
};

/**
 * @brief 安全调用 GPIO 写方法。
 * @pre 外部保证 self 非空，且指向有效、存活的接口集对象。
 * @note 只检查成员函数；参数合法性由具体实现负责，参数和结果原样传递。
 * @see IGpio::Write
 */
Int32 IGpioWrite(const IGpio *self, UInt32 pin, UInt32 level);

/* gpio_if.c */
Int32 IGpioWrite(const IGpio *self, UInt32 pin, UInt32 level)
{
    Int32 result = kGpioUnsupported;
    if (self->Write != NULL)
    {
        result = self->Write(self, pin, level);
    }
    return result;
}

/* board_gpio.h/.c */
typedef struct TBoardGpioTag TBoardGpio;
struct TBoardGpioTag
{
    UInt32 port_id;
    IGpio  gpio_if;
};

static Int32 TBoardGpioWrite(const IGpio *sender, UInt32 pin, UInt32 level)
{
    TBoardGpio *self = OF_CONTAINER_OF(sender, TBoardGpio, gpio_if);
    return BoardGpioWrite(self->port_id, pin, level);
}

void TBoardGpioCreate(TBoardGpio *self, UInt32 port_id)
{
    self->port_id       = port_id;
    self->gpio_if.Write = TBoardGpioWrite;
}
```

使用方取得 `&board.gpio_if`，在外部保证其有效后调用 `IGpioWrite`。接口集声明、具体对象实现和部署流程分别维护，安全调用方法保持为简短的转发函数。

### 生命周期函数
- `ModuleCreate()` / `ModuleDestroy()`
- `ModuleInit()` / `ModuleDone()`
- `ModuleRunOnce()`

---

## 9. 注释规范

- 新增或修改的说明性注释默认使用简体中文；代码标识符、协议名称、工具名称及标准术语可保留原文。

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
