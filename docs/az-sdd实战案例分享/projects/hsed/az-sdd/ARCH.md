# HSED 项目级技术架构与实现基线

## 文档目的

本文是 HSED 交接实现入口，回答“代码如何组成、如何启动、各层之间如何调用、当前实现与已批准功能契约有何差异”。重新实现时应同时读取：

1. `az-sdd/spec/*.spec.md`：产品行为与验收口径；
2. 本文：项目级依赖、生命周期、配置、构建和当前代码基线；
3. `az-sdd/arch/*.arch.md`：功能级实现细节。

## 生效与冲突规则

| 优先级 | 来源 | 说明 |
|---:|---|---|
| 1 | 已批准功能规范 | 决定应交付的产品行为；代码现状不能反向覆盖已批准验收口径 |
| 2 | 本文“目标实现契约”及功能级 arch 的最新生效基线 | 决定实现边界、接口和验证条件 |
| 3 | 本文“当前代码快照” | 用于精确复现当前版本、定位未完成项和兼容问题 |
| 4 | 功能级 arch 中标为“历史/已被替代”的章节 | 只保留决策历史，不作为新实现依据 |

如果目标是复现当前二进制行为，应连同“当前代码快照”中的限制一起实现；如果目标是完成产品交付，应修复快照与已批准规范之间的差异，不得复制已知缺口。

## 技术规范索引

| 编号 | 技术规范名称 | 关联功能 | 状态 | 更新日期 |
|---|---|---|---|---|
| [0003](arch/0003-hsed-SPI设备通信自检指令.arch.md) | SPI 设备通信自检指令 | 0003 | 已批准；已补充代码逆向基线 | 2026-07-27 |
| [0004](arch/0004-hsed-BSC-HSE硬件密码能力组件化.arch.md) | BSC HSE 硬件密码能力组件化 | 0002、0004～0009 | 已批准；已补充代码逆向基线 | 2026-07-27 |
| 0010 | 程序退出 SPI 引脚低功耗状态 | 0010 | 待创建；功能规范仍为草稿 | - |

## 系统上下文

```mermaid
flowchart LR
    C["业务进程 / 测试客户端"] -->|"SPC 同步 RPC"| G["生成的 HSED SPC 层"]
    G --> A["hsed_spc_adapter"]
    A --> B["BSC HSE 公共组件"]
    B --> V["TbscHseVendorOps"]
    V --> R["RTE 动态库加载边界"]
    R --> D["libbsc_hse_ciu98_ylb04_a6.so"]
    D --> S["CIU98 Host SDK"]
    S --> H["CIU98 / SPI"]
```

### 模块与代码所有权

| 层级 | 主要路径 | 职责 | 禁止依赖 |
|---|---|---|---|
| app | `common/main.c`、`common/config.c` | 参数解析、日志/信号、配置、RTE/BSC/SPC 生命周期 | 不直接调用 CIU98 SDK 密码接口 |
| SPC 服务 | `business/spc/hsed/` | 注册 `hsed` 服务、连接归属、11 项 RPC 到 BSC 的映射 | 不包含供应商 SDK 类型 |
| BSC | `framework/bsc_hse/common/` | 公共类型、对象池、属性、统一错误、进程内串行化 | 不使用 `dlopen`，不包含供应商头文件 |
| RTE | `rte_component/rte_hse/` | Linux `dlopen/dlsym/dlclose`、供应商绑定加载、自检入口 | 不实现对象/SPC 语义 |
| vendor 插件 | `mpu/lib/bsc_hse_plug/vendor/ciu98/`（独立仓库） | BSC 参数到 CIU98 SDK 的映射、SDK 生命周期和错误映射 | 不管理 SPC 连接；不纳入 HSED CMake |
| 生成模型 | `doc/hsed.xlsx`、`doc/spc-build.bat` | 对外 SPC 数据类型和 11 项接口的权威生成源 | 手工修改生成文件后不得遗漏模型同步 |

## 目标实现契约

### 对外能力

HSED 对外固定提供 11 项同步 RPC：

- 对象：`HseCreate`、`HseDestroy`；
- 属性：`SetPropertyInt`、`SetPropertyBytes`、`GetPropertyInt`、`GetPropertyBytes`；
- 密码：`Encrypt`、`Decrypt`、`Sign`；
- 公钥：`ExportPublicKey`、`ImportPublicKey`。

私钥导出、验签、密钥生成、密钥删除、摘要、随机数和固件升级不属于当前范围。

### 生命周期

正常服务的目标顺序为：

```text
日志/OFC/SFC/SRT 基础设施
  -> TCIni 加载配置
  -> RteHseInit（只加载动态库）
  -> BscHseInit（校验 ABI、初始化 vendor、对象池和 BSC 锁）
  -> HsedSpcServiceStart
  -> 等待 SIGINT/SIGTERM
  -> HsedSpcServiceStop
  -> BscHseDone
  -> RteHseDone
  -> 基础设施反初始化
```

任一步失败必须按已成功步骤逆序清理；SPC 只能在 RTE 与 BSC 均成功后开放。

### 配置契约

| 项目 | 当前契约 |
|---|---|
| 默认配置文件 | `/etc/hsed.ini` |
| 覆盖方式 | 命令行 `--cfg <path>` |
| 读取接口 | `TCIniCreate`、`TCIniGetKeyValueStringEx`、`TCIniDestroy` |
| 必需键 | `[bsc_hse] vendor_so_path` |
| 缓冲上限 | 256 字节，包含结尾 NUL |
| 默认值 | 空字符串；键缺失、空值或文件不可读均初始化失败 |
| 当前 CIU98 动态库 | `/usr/lib/libbsc_hse_ciu98_ylb04_a6.so` |

`[spi] device_path` 已由 `common/config.c` 使用 `TCIni` 统一读取：自检显式传入 `--cfg` 时读取该文件，未传入时固定使用 `/dev/spidev1.0`；正常服务从其运行 INI 读取。RTE 在加载 vendor 后、初始化 SDK 前调用 `BscHseVendorConfigure()` 下发路径，CIU98 port 层以受限 256 字节缓冲保存路径。

### BSC 公共 ABI

公共头文件为：

- `framework/bsc_hse/common/include/bsc_hse/bsc_hse_types.h`
- `framework/bsc_hse/common/include/bsc_hse/bsc_hse_vendor.h`
- `framework/bsc_hse/common/include/bsc_hse/bsc_hse.h`

生命周期和 11 项接口以 `bsc_hse.h` 的函数签名为准。当前 `BscHseInit` 接收：

```c
Int32 BscHseInit(const TbscHseVendorBinding *binding,
                 TbscHseObject *object_pool,
                 UInt32 object_pool_count);
void BscHseDone(void);
```

BSC 不拥有对象池内存；app 提供静态数组。`BscHseDone` 当前无返回值。

### Vendor ABI

| 项目 | 精确约定 |
|---|---|
| ABI 版本 | `BSC_HSE_VENDOR_ABI_VERSION == 2` |
| 动态库必需导出 | `BscHseVendorSdkApi` |
| 自检导出 | `BscHseVendorTest`，正常服务可缺失，自检时缺失返回 `UNSUPPORTED` |
| 操作表必需成员 | `Init`、`Done`、`Encrypt`、`Decrypt`、`Sign`、`ExportPublicKey`、`ImportPublicKey` |
| ABI 校验位置 | BSC `BscHseInit` 校验 `abi_version`、`struct_size` 和全部操作指针 |
| binding 生命周期 | 从动态库加载成功到 `RteHseDone/dlclose` 前持续有效 |

RTE 使用 `RTLD_NOW | RTLD_LOCAL`。同一进程重复 `RteHseInit` 返回 `BUSY`；非 Linux 构建返回 `UNSUPPORTED`。

### 对象、句柄与容量

| 项目 | 当前精确值 |
|---|---:|
| 对象池 | 8 |
| SPC 最大连接数 | 8 |
| SPC listen queue | 8 |
| SPC request queue | 16 |
| SPC response/receive/cache 线程 | 各 1 |
| USER_ID 最大长度 | 128 |
| SM2 使用 USER_ID 长度 | 前 16 字节 |
| 最大业务输入 | 4096 |
| 最大签名 | 512 |
| 最大公钥 | 1024 |
| 最大 CHIP_ID | 128 |
| SPC 加解密输出栈缓冲 | 4608 |

句柄为 64 位非零值：低 32 位保存 `slot + 1`，位 `[62:32]` 保存 31 位代际，高位必须为 0。销毁时对象清零并增加代际，旧句柄返回 `OBJECT_NOT_FOUND`。

SPC 适配层另维护 8 项 `(connection, handle)` 表；任何跨连接句柄访问返回 `OBJECT_NOT_FOUND`。连接结束、服务停止时销毁其遗留对象。

### 线程与 OS 边界

- BSC 和 SPC 适配层都使用 SFC `TSfCriticalSection`；当前 SPC 适配层在整个 BSC 同步调用期间持锁，因此同一服务内的 11 项调用全局串行。
- BSC 再用自身 `TSfCriticalSection` 保护生命周期、对象池、属性及 vendor 调用。
- `signal` 仅位于 app；`dlopen/dlsym/dlclose` 仅位于 RTE；供应商 `open/ioctl` 等位于 CIU98 Linux port。
- 当前代码没有实现跨进程 `flock`。正常服务与 `--test-spi` 的跨进程互斥仍是 0003/0004 的未完成目标，不得误写为已具备。

## 当前代码快照（2026-07-27）

### app 行为

| 场景 | 当前实际行为 |
|---|---|
| 自检识别 | 仅当 `argv[1]` 精确等于 `--test-spi` |
| `--cfg` | 从参数 1 开始找到首个 `--cfg`，取下一参数；缺少下一参数时等同未指定 |
| 未知参数 | 不报错；若首参数不是 `--test-spi` 则进入正常服务 |
| 自检 | 加载 `[bsc_hse] vendor_so_path`，调用 `RteHseSelfTest`，不启动 BSC/SPC |
| 信号 | `SIGINT/SIGTERM` 只设置全局退出标志；主循环每 10 ms 检查一次 |
| 正常退出 | SPC -> BSC -> RTE -> SRT -> ALOG/日志锁 |
| 低功耗 GPIO | 尚未实现；0010 仍为草稿 |

### 对象初值和属性行为

对象创建只把空闲对象的 `used` 设为真；其余字段保持清零后的值。因此当前默认行为是：

- 所有 key/public/private slot 为 `0`；
- 模式、补位、哈希、签名输入模式、公钥格式、SM9 模式均为 `0`；
- USER_ID 长度为 0；
- 所有 IV 均未设置。

当前代码尚未从物料表初始化 14 项槽位；该行为与 0005/0009 的已批准目标不一致，重做产品版本时必须补齐，不能把全 0 默认值当成最终物料契约。整数属性已校验非负密钥槽、模式、补位、哈希、签名输入、SM9 模式和当前仅支持的 BIN 公钥格式；由于物料槽位表缺失，尚不能在 BSC 内确认物理槽位存在性、算法和角色。

字节属性当前行为：

| 属性 | Set | Get |
|---|---|---|
| `USER_ID` | 允许 0～128 字节；0 字节清空；超限 `DATA_TOO_LARGE` | 返回实际内容；容量不足更新所需长度并返回 `DATA_TOO_LARGE` |
| 四种 IV | 必须分别为 16/8/16/16 字节 | 未设置返回 `OBJECT_STATE_ERROR` |
| `CHIP_ID` | `UNSUPPORTED` | `UNSUPPORTED` |
| 整数属性误用字节接口 | `INVALID_PARAM` | `UNSUPPORTED` |

### 密码调用当前行为

- Encrypt/Decrypt：哈希算法和 `NONE` 返回 `UNSUPPORTED`；对称算法取 KEY_ID，非对称加密取公钥槽，非对称解密取私钥槽。
- CBC 必须已设置对应 IV；非 CBC 值在 CIU98 适配中均映射为 ECB。
- BSC 校验整数属性的枚举范围、补位算法适用性和 slot 非负值；SPC 入口拒绝空输入并限制 4096 字节。具体槽位的存在性、算法和角色仍须由待补充的物料配置/供应商能力验证。
- Sign：主算法必须为非对称算法；SM2 仅支持 `MESSAGE + SM3`，要求 USER_ID 至少 16 字节，并向 vendor 传入独立公私钥槽。
- CIU98 SM2：导出当前公钥槽的 64 字节公钥，计算 ZA，签署 `ZA || M`；输入超过 4096 返回 `DATA_TOO_LARGE`；临时缓冲在返回前清零。
- ECC256/SM2 的签名分量按返回总长均分为 `r_size` 和 `s_size`；RSA/SM9 两者为 0。
- CIU98 `ImportPublicKey` 当前固定返回 `UNSUPPORTED`；`CHIP_ID` 读写、物料槽位检查和默认密钥生成尚未接入操作表。

### CIU98 映射

| BSC | CIU98 SDK |
|---|---|
| AES128 / TDES128 / SM1 / SM4 | `ALG_AES128 / ALG_DES128 / ALG_SM1 / ALG_SM4` |
| RSA1024 / RSA2048 / ECC256 / SM2 / SM9 | `ALG_RSA1024_STANDAND / ALG_RSA2048_STANDAND / ALG_ECC256 / ALG_SM2 / ALG_SM9` |
| SHA1 / SHA256 / SHA384 / SHA512 / SM3 / NONE | `ALG_SHA1 / ALG_SHA256 / ALG_SHA384 / ALG_SHA512 / ALG_SM3 / ALG_NONE` |
| ECB / CBC | `SYM_MODE_ECB / SYM_MODE_CBC` |
| NONE / PKCS7 / PKCS1 | `PADDING_NOPADDING / PADDING_PKCS7 / PADDING_PKCS1` |

初始化调用序列是 `api_register -> api_select -> api_connect`；关闭调用 `api_disconnect`。自检在连接后额外调用 `api_get_info(CHIP_ID)`。

## 规范—代码差异与交接优先级

| 优先级 | 差异 | 处理要求 |
|---|---|---|
| P0 | 0010 退出 GPIO 低功耗未实现 | 规范批准并建立板级 arch 后实现，不能由现有代码推断 GPIO 编号 |
| P1 | 0003 仅实现 0/2/3/4/5 退出码，尚未区分 SDK 初始化、连接和信息读取阶段的 5/6/7 | 扩展 vendor 自检阶段回传并补目标机测试 |
| P0 | 0005/0009 物料 14 槽位初始化、CHIP_ID、公钥导入/覆盖权限未实现 | 补物料配置和 vendor 能力，不得继续使用全 0 默认 |
| P1 | 物料槽位表缺失，BSC 无法验证物理槽位存在性、算法和角色 | 提供 CIU98 物料配置并接入 vendor 能力查询 |
| P1 | 0004 历史文本曾描述独立 runtime API，但当前公共 ABI 没有该 API | 以当前 `BscHseInit/Done` ABI 为准；若再次引入需重新审核 ABI |
| P2 | 完整 vendor 构建受 CIU98 SDK 板级宏/头文件配置影响 | 先修复 SDK 物料构建配置，再做全量设备回归 |

## 测试工程目录与独立构建约束

### 架构决策

测试代码组织属于跨 BSC、RTE、SPC 和 vendor 模块的项目级架构约束，不归属于单一业务功能。
项目统一采用“根目录集中测试 + 一个测试目标一个独立 CMake 工程”的方式：

1. 测试代码统一存放在项目根目录 `test/`，生产模块目录不再新增私有 `test/` 子目录；
2. 每个可独立生成可执行文件或测试动态库的目标使用 `test/<test-project>/` 独立目录；
3. 每个测试工程必须提供独立 `CMakeLists.txt`，可从空构建目录单独配置和编译；
4. 生产 CMake 默认只构建和安装产品交付物，不直接包含测试目标；
5. host 测试与目标机测试分别显式使用本机编译器或交叉工具链，不通过生产构建缓存隐式继承；
6. 生产代码禁止包含、链接、调用或以条件编译方式依赖 `test/` 中的测试代码、fake、stub、测试专用头文件和测试数据；测试代码只允许依赖生产接口；
7. 正式编译出包环境会临时删除全部测试代码，因此生产构建、安装、部署与运行不得依赖任何测试目录或测试产物。
6. fake、stub、shim、测试数据和测试配置只能被对应测试工程引用，不得进入生产目标。

### 目录模型

```text
<project-root>/
├── common/
├── framework/
├── business/
├── rte_component/
├── doc/
└── test/
    ├── <test-project-a>/
    │   ├── CMakeLists.txt
    │   ├── <test-source>.c
    │   ├── fake/                 # 可选：完整外部依赖的轻量替身
    │   ├── stub/                 # 可选：单函数或系统接口替身
    │   ├── data/                 # 可选：固定输入和期望输出
    │   └── config/               # 可选：仅供本工程使用的配置
    └── <test-project-b>/
        └── CMakeLists.txt
```

目录名称应与测试目标一致或能明确表达被测对象和测试类型，例如
`<module>_unit`、`<module>_adapter`、`<module>_loader`、`<service>_smoke`。
不允许将多个无关测试目标平铺在 `test/` 根目录。

### CMake 与依赖边界

每个测试工程的 `CMakeLists.txt` 必须自行声明：测试源文件、被测生产源码或链接库、
公共 include 路径、测试替身路径、链接库和工具链条件。测试工程不得依赖生产工程
CMake 中预先定义的变量、临时目录的源码，或开发者本机绝对路径。

推荐独立构建入口：

```sh
cmake -S test/<test-project> -B build/test/<test-project>
cmake --build build/test/<test-project>
```

目标机测试必须显式提供工具链：

```sh
cmake -S test/<target-test-project> -B build/test/<target-test-project> \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=<toolchain-file>
cmake --build build/test/<target-test-project>
```

生成的生产接口代码保留在所属生产模块的 `generated/` 目录中；测试工程可以引用它，
但不得复制生成代码。产品部署配置和公开示例配置保留在 `doc/`；只有单一测试专用的
配置才放入其 `config/` 子目录。

### 当前 HSED 测试工程

```text
test/
├── bsc_hse_runtime/
│   ├── CMakeLists.txt
│   ├── bsc_hse_sfc_runtime_test.c
│   └── fake/sfc_mutex_test_shim.c
├── ciu98_za_adapter/
│   ├── CMakeLists.txt
│   └── ciu98_za_adapter_test.c
├── rte_hse_loader/
│   ├── CMakeLists.txt
│   ├── rte_hse_loader_test.c
│   └── fake/
├── hsed_spc_adapter/
│   ├── CMakeLists.txt
│   └── hsed_spc_adapter_test.c
└── hsed_spc_smoke/
    ├── CMakeLists.txt
    └── hsed_spc_smoke.c
```

| 工程 | 类型 | 被测边界 | 验证方式 |
|---|---|---|---|
| `bsc_hse_runtime` | host 白盒测试 | BSC 生命周期、属性校验和 SFC 串行化 | 本机 CMake/CTest |
| `ciu98_za_adapter` | host 白盒测试 | CIU98 SM2 ZA 签名适配 | 本机 CMake/CTest，读取插件仓库源码 |
| `rte_hse_loader` | host 白盒测试 | RTE 动态库加载、缺符号和 ABI 拒绝 | 本机 CMake/CTest |
| `hsed_spc_adapter` | host 白盒测试 | SPC 适配层的句柄归属、接口转发和断连清理 | 本机 CMake 编译后直接执行 |
| `hsed_spc_smoke` | 目标机客户端测试 | 已运行 HSED 服务的 SPC 传输与对象生命周期 | AG35 交叉编译后部署执行 |

`doc/hsed.ini` 是 HSED 产品部署配置示例，不属于任何测试工程。

### 验收条件

- 每个新建或迁移的测试目标都位于 `test/` 下且拥有独立 `CMakeLists.txt`；
- 测试工程能从空构建目录独立配置、编译；host 测试能执行并返回明确结果；
- 目标机测试可通过显式工具链独立交叉编译；
- 生产默认构建不编译、不安装测试源码、fake、stub 或测试可执行文件；
- 在临时删除全部测试代码后，正式产品仍能从空构建目录完成配置、编译、安装和部署；
- 生产源码、生产 CMake、部署脚本和运行配置均不得引用 `test/` 路径或测试专用符号；
- 移动后不存在旧路径引用，正式产品构建仍通过。

## 构建与产物

| 项目 | 当前值 |
|---|---|
| HSED CMake 入口 | `build/common/CMakeLists.txt` |
| Windows 构建入口 | `C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\az_build_linux_hsed.bat` |
| Linux 脚本入口 | `C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\az_build_linux_app_hsed.sh` |
| app 目标 | `hsed` |
| 插件仓库 | `mpu/lib/bsc_hse_plug` |
| 插件 CMake 目标 / 输出名 | `ciu98_hse_vendor` / `libbsc_hse_ciu98_ylb04_a6.so` |
| app 直接源码 | app/config/cini、BSC、RTE、SPC adapter/contract/service、生成 app/srv |

CIU98 vendor 由 `mpu/lib/bsc_hse_plug` 的独立 CI 先行构建并发布；HSED CMake 只构建服务本体，运行时仍通过 INI 路径加载该 `.so`。插件 ABI 头文件必须与 HSED 的 BSC HSE 公共头文件保持一致。

## 验证入口

| 层级 | 入口/证据 |
|---|---|
| SPC 连接隔离 | `test/hsed_spc_adapter` |
| RTE 加载异常 | `rte_component/rte_hse/test/rte_hse_loader_test.c` |
| BSC SFC 串行化 | `framework/bsc_hse/test/bsc_hse_sfc_runtime_test.c` |
| CIU98 ZA | `test/ciu98_za_adapter/ciu98_za_adapter_test.c` |
| 目标 SPC 冒烟 | `test/hsed_spc_smoke` |
| SM2 多通道压力 | `test/hsed_spc_sm2_stress` |
| 既有报告 | `az-sdd/test/reports/` |

交接验收至少应同时完成：静态契约检查、主机回归、AG35 交叉构建、真实 CIU98 单请求、8 连接隔离、并发串行化、SM2 外部验签、退出低功耗引脚与功耗测量。
