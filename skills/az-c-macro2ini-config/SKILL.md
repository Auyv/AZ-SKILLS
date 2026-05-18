---
name: az-c-macro2ini-config
description: 将嵌入式C项目中的编译时宏定义改为通过INI文件加载运行时配置，INI不存在时使用默认值
---

# 嵌入式C宏定义转INI配置

## 做什么

将嵌入式 C 项目中的编译时 `#define` 宏定义改为运行时从 INI 文件加载配置，当 INI 文件不存在时自动使用编译时默认值。

## 何时使用

当用户提到以下场景时触发：
- "宏定义改为配置文件"
- "编译时常量改为运行时配置"
- "添加 INI 配置支持"
- "配置外置到文件"
- "用 ini 文件管理配置"
- "不想每次改配置都重新编译"

## 怎么做

### 第一步：分析现有宏定义

1. 读取配置头文件（通常是 `xxx_cfg.h`）
2. 分类整理所有宏定义：
   - **字符串类型**：路径、URL 格式等
   - **整数类型**：端口、ID、开关等
   - **计算类型**：如 `1024*1024`，需要展开为实际值
3. 识别宏之间的依赖关系（如 `LOG_FILE_DIR` 依赖 `BASE_PATH`）

### 第二步：设计 INI 文件结构

INI 文件格式建议：

```ini
; 配置文件说明
[section_name]
key = value
```

设计原则：
- 按功能模块划分 Section
- Key 名称使用小写字母和下划线
- 字符串值不需要引号
- 整数值直接写数字
- 添加注释说明每个配置项的用途

### 第三步：修改头文件

将原头文件改为三部分：

```c
// 1. 默认值宏定义（INI 不存在时使用）
#define DEFAULT_XXX "default_value"

// 2. INI 文件路径
#define CFG_INI_PATH "/path/to/config.ini"

// 3. 运行时配置结构体
typedef struct TCfgTag {
    char field_name[256];
    Int32 int_field;
} TCfg;

// 4. 兼容性宏（保持原有代码编译通过）
#define XXX g_cfg.field_name

// 5. 接口声明
extern TUpgradeCfg g_cfg;
Int32 CfgInit(const char *ini_file_path);
```

### 第四步：提供 cini 组件

检查项目中是否已有 INI 解析库。如果没有，询问用户：

> "项目中没有找到 INI 解析库，是否需要拷贝 cini 组件？"

如果用户同意：
1. 将 cini 组件拷贝到项目的指定目录（通常是 `app/xxx/cini/`）
2. 更新 CMakeLists.txt 添加 cini 目录到头文件和源文件列表

**cini 组件文件**：
- `cini.h` - C API 头文件
- `cini.cpp` - C++ 实现（封装 rfc::TMemIniFile）

**CMakeLists.txt 修改**：
```cmake
# 添加头文件路径
include_directories(${SRC_PATH}/cini)

# 添加源文件
aux_source_directory(${SRC_PATH}/cini SRC_FILES)
```

### 第五步：创建配置加载实现

新建 `xxx_cfg.c`：

```c
#include "xxx_cfg.h"
#include "cini/cini.h"  // 或其他 INI 解析库

TCfg g_cfg;

Int32 CfgInit(const char *ini_file_path)
{
    TCIni ini;
    const char *path = ini_file_path ? ini_file_path : CFG_INI_PATH;
    char str_buf[256];
    Int32 int_val;

    // 1. 用默认值初始化
    strncpy(g_cfg.field, DEFAULT_FIELD, sizeof(g_cfg.field) - 1);
    g_cfg.int_field = DEFAULT_INT_FIELD;

    // 2. 尝试打开 INI 文件
    if (TCIniCreate(&ini, path) != 0) {
        return -1;  // 文件不存在，使用默认值
    }

    // 3. 逐项读取
    if (TCIniGetKeyValueStringEx(&ini, "section", "key",
        str_buf, sizeof(str_buf), g_cfg.field)) {
        strncpy(g_cfg.field, str_buf, sizeof(g_cfg.field) - 1);
    }
    if (TCIniGetKeyValueIntEx(&ini, "section", "key",
        &int_val, g_cfg.int_field)) {
        g_cfg.int_field = int_val;
    }

    TCIniDestroy(&ini);
    return 0;
}
```

### 第六步：集成到 main 函数

在 `main()` 中尽早调用初始化：

```c
int main(int argc, char *argv[])
{
    // 其他初始化...
    
    // 初始化配置（在使用配置的模块之前）
    CfgInit(NULL);  // NULL 使用默认路径
    
    // 后续代码...
}
```

### 第七步：处理特殊情况

#### 字符串字面量拼接

原代码：
```c
result = PEM_KEYS_DIR"/file.hex";
```

改为：
```c
static char path_buf[256];
snprintf(path_buf, sizeof(path_buf), "%s/file.hex", PEM_KEYS_DIR);
result = path_buf;
```

#### 静态初始化

原代码（编译时）：
```c
static const Config cfg = {
    .dir = LOG_DIR,
};
```

改为（运行时）：
```c
static Config cfg = {0};
// 在初始化函数中赋值
cfg.dir = LOG_DIR;
```

### 技能文件结构

```
az-c-macro2ini-config/
├── SKILL.md           # 技能说明文档
├── MANUAL.md          # 手动操作指南
├── cini/              # cini 组件源码（可直接拷贝到项目）
│   ├── cini.h
│   └── cini.cpp
└── assets/
    └── example.ini    # INI 文件示例模板
```

## 注意事项

1. **保留宏定义**：原有宏改为 `DEFAULT_*` 前缀，作为默认值参考
2. **兼容性宏**：使用 `#define OLD_MACRO g_cfg.field` 保持原有代码编译通过
3. **静态初始化问题**：C 语言不允许变量用于静态初始化，必须改为运行时赋值
4. **字符串拼接问题**：宏变为变量后，字面量拼接 `MACRO"str"` 会编译失败，需改用 `snprintf`
5. **INI 文件不存在时**：自动回退到默认值，不影响程序启动
