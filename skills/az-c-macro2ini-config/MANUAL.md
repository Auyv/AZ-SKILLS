# 手动操作指南：嵌入式C宏定义转INI配置

## 概述

本文档指导如何将嵌入式 C 项目中的编译时 `#define` 宏定义改为运行时从 INI 文件加载。

## 操作步骤

### 1. 分析现有配置

找到配置头文件（如 `xxx_cfg.h`），列出所有需要转换的宏：

```c
// 原始宏定义
#define BASE_PATH       "/media/card/"
#define LOG_FILE_DIR    BASE_PATH"/log/"
#define LOG_FILE_SIZE   (1024 * 1024)
#define NET_ENABLE      1
```

### 2. 创建 INI 配置文件

创建 `config.ini`：

```ini
[log]
file_dir = /media/card/log/
file_size = 1048576

[net]
enable = 1
```

### 3. 修改头文件

```c
// 默认值
#define DEFAULT_LOG_FILE_DIR    "/media/card/log/"
#define DEFAULT_LOG_FILE_SIZE   1048576
#define DEFAULT_NET_ENABLE      1

// INI 路径
#define CFG_INI_PATH "/vendor/app/etc/config.ini"

// 结构体
typedef struct TCfgTag {
    char log_file_dir[256];
    Int32 log_file_size;
    Int32 net_enable;
} TCfg;

extern TCfg g_cfg;

// 兼容性宏
#define LOG_FILE_DIR    g_cfg.log_file_dir
#define LOG_FILE_SIZE   g_cfg.log_file_size
#define NET_ENABLE      g_cfg.net_enable
```

### 4. 创建实现文件

```c
#include "xxx_cfg.h"
#include "cini/cini.h"

TCfg g_cfg;

Int32 CfgInit(const char *ini_path)
{
    TCIni ini;
    const char *path = ini_path ? ini_path : CFG_INI_PATH;
    char buf[256];
    Int32 val;

    // 默认值
    strncpy(g_cfg.log_file_dir, DEFAULT_LOG_FILE_DIR, sizeof(g_cfg.log_file_dir) - 1);
    g_cfg.log_file_size = DEFAULT_LOG_FILE_SIZE;
    g_cfg.net_enable = DEFAULT_NET_ENABLE;

    if (TCIniCreate(&ini, path) != 0) return -1;

    if (TCIniGetKeyValueStringEx(&ini, "log", "file_dir", buf, sizeof(buf), g_cfg.log_file_dir))
        strncpy(g_cfg.log_file_dir, buf, sizeof(g_cfg.log_file_dir) - 1);
    
    if (TCIniGetKeyValueIntEx(&ini, "log", "file_size", &val, g_cfg.log_file_size))
        g_cfg.log_file_size = val;
    
    if (TCIniGetKeyValueIntEx(&ini, "net", "enable", &val, g_cfg.net_enable))
        g_cfg.net_enable = val;

    TCIniDestroy(&ini);
    return 0;
}
```

### 5. 修改 main 函数

```c
int main(int argc, char *argv[])
{
    CfgInit(NULL);  // 在使用配置之前调用
    // ...
}
```

### 6. 修复编译问题

**字符串拼接**：
```c
// 原
result = DIR"/file.hex";
// 改
static char buf[256];
snprintf(buf, sizeof(buf), "%s/file.hex", DIR);
result = buf;
```

**静态初始化**：
```c
// 原
static const X x = { .dir = DIR };
// 改
static X x = {0};
// 在初始化函数中赋值
x.dir = DIR;
```

## 验证

1. 编译通过
2. 无 INI 文件时使用默认值正常运行
3. 有 INI 文件时读取配置正确
