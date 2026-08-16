# HSED SPC 客户端接口使用指南

> 适用对象：需要在 MPU Linux 应用中通过 SPC 调用 HSED 硬件密码服务的开发、集成和测试人员。  
> 接口基线：`business/spc/hsed/generated/hsed_client.h`、`hsed_datatype.h`。  
> 文档日期：2026-07-27。

## 1. 功能与边界

HSED 通过 SPC 同步 RPC 提供以下能力：

- 创建和销毁 HSE 功能对象；
- 设置、读取对象属性；
- 数据加密、数据解密；
- 数据签名；
- 公钥导入、公钥导出。

当前阶段不提供私钥导入/导出、验签、密钥生成、密钥删除、独立摘要、随机数和固件升级接口。

`key_id` 表示 CIU98 芯片中的密钥槽位，不代表用户、进程或会话。当前版本固定使用槽位 1～14，客户端只能读取，不能修改、生成或覆盖密钥。

## 2. 调用架构

```mermaid
flowchart LR
    APP["MPU 应用进程"] --> CLIENT["HSED SPC 客户端"]
    CLIENT --> SPC["SPC 同步 RPC"]
    SPC --> SERVER["HSED 服务"]
    SERVER --> BSC["BSC HSE 业务层"]
    BSC --> ADAPTER["CIU98 适配层"]
    ADAPTER --> CHIP["CIU98 芯片"]
```

HSED 服务内部串行访问 CIU98。客户端可以建立多个连接并并发发起请求，但硬件操作最终由服务端调度，客户端线程数不等于芯片并行度。

## 3. 术语与数据约定

| 名称 | 含义 |
|---|---|
| SPC 返回值 | `HcHsed_*` 函数本身的返回值，表示连接、序列化、超时等 RPC 传输状态 |
| 业务返回值 | 参数 `_ret_` 指向的 `THcHsedHsedResult`，表示 HSED/CIU98 操作结果 |
| HSE 对象/handle | 由 `HcHsed_HseCreate` 创建的功能对象句柄 |
| 连接/connection | 客户端到 HSED 服务的一条 SPC 连接 |
| 密钥槽位/key_id | CIU98 内部的公钥、私钥或对称密钥槽位编号 |
| USER_ID | 对象级字节属性；SM2 原文签名时参与 ZA 计算 |
| ZA | SM2 用户身份、公钥和曲线参数共同计算出的预处理摘要 |

所有长度参数均以字节为单位。字节数组不是以 `'\0'` 结尾的字符串，必须显式传入长度。

输出长度参数（例如 `_count_signature`）是输入/输出参数：

1. 调用前填写输出缓冲区容量；
2. 调用成功后返回实际输出长度；
3. 只有 SPC 返回值和业务返回值同时成功时，输出数据和长度才有效。

## 4. 集成客户端代码

### 4.1 需要加入目标程序的文件

普通单连接应用需编译：

```text
business/spc/hsed/generated/hsed_client.c
business/spc/hsed/generated/hsed_conn.c
```

业务源文件包含：

```c
#include "generated/hsed_client.h"
#include <SpanCall.h>
```

`hsed_client.h` 会包含 `hsed_datatype.h`，其中定义算法、属性和业务错误码。

### 4.2 CMake 示例

以下片段与项目现有测试工程的依赖保持一致，路径可按调用方工程结构调整：

```cmake
set(HSED_SPC_PATH "${HSED_ROOT_PATH}/business/spc/hsed")

target_sources(your_app PRIVATE
    your_app.c
    ${HSED_SPC_PATH}/generated/hsed_client.c
    ${HSED_SPC_PATH}/generated/hsed_conn.c)

target_include_directories(your_app PRIVATE
    ${HSED_SPC_PATH}
    ${HSED_SPC_PATH}/generated
    ${BUILD_OUTPUT_PATH}/include
    ${BUILD_OUTPUT_PATH}/include/cbb_srt
    ${SDK_PATH}/include
    ${SDK_PATH}/include/ofc
    ${SDK_PATH}/include/sfc
    ${SDK_PATH}/include/mfc
    ${SDK_PATH}/include/ffc
    ${SDK_PATH}/include/rfc
    ${SDK_PATH}/include/hcc
    ${SDK_PATH}/include/span-call)

target_link_libraries(your_app PRIVATE
    logclient smqueue spancall ffc rfc sfc mfc ofc pthread rt)

set_target_properties(your_app PROPERTIES LINKER_LANGUAGE CXX)
```

## 5. 最小生命周期

普通单连接客户端按以下顺序调用：

```text
SCInit
  -> HcHsed_Init
  -> HcHsed_Open
  -> HcHsed_HseCreate
  -> 设置属性
  -> 执行 Encrypt / Decrypt / Sign / 公钥导入导出
  -> HcHsed_HseDestroy
  -> HcHsed_Close
  -> HcHsed_Done
  -> SCFini
```

注意：

- `SCInit()` 和 `SCFini()` 管理进程级 SPC 运行环境；
- `HcHsed_Init()` 创建生成客户端内部的全局连接；
- `HcHsed_Open()` 建立到 HSED 服务的连接；
- `handle` 绑定创建它的 SPC 连接，不允许跨连接或跨进程使用；
- 清理顺序应与创建顺序相反；
- 连接中断后，旧 `handle` 不应继续使用，应重新连接并重新创建对象。

## 6. 返回值必须分两层检查

```c
int transport_result;
int business_result = kHsedHsedResult_DEVICE_ERROR;
TInt64 handle = 0;

transport_result = HcHsed_HseCreate(&business_result, &handle);
if (transport_result != SC_rSuccess) {
    /* SPC 连接、编码、超时或远端调用失败 */
}
else if (business_result != kHsedHsedResult_OK) {
    /* HSED 参数、对象、密钥槽位或设备操作失败 */
}
else {
    /* handle 有效 */
}
```

不要只判断 `_ret_`，也不要在 SPC 返回失败时读取业务返回值或输出缓冲区。

## 7. SM2 带 ZA 签名完整示例

以下示例使用：

- SM2 固定公钥槽位 `11`；
- SM2 固定私钥槽位 `12`；
- SM3；
- 原文签名模式；
- 16 字节固定用户 ID；
- 输出格式为 64 字节 `r || s`，其中 `r`、`s` 各 32 字节。

槽位编号由 HSED 固定策略分配，客户端不得修改。

```c
#include <stdio.h>
#include <string.h>

#include <SpanCall.h>
#include "generated/hsed_client.h"

static int CheckResult(const char *name, int transport, int business)
{
    if (transport != SC_rSuccess) {
        (void)fprintf(stderr, "%s: SPC error=%d\n", name, transport);
        return -1;
    }
    if (business != kHsedHsedResult_OK) {
        (void)fprintf(stderr, "%s: HSED error=%d\n", name, business);
        return -1;
    }
    return 0;
}

int main(void)
{
    static unsigned char user_id[16] = {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBB, 0xCC, 0xC0,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xBC, 0xCC
    };
    static unsigned char message[] = "HSED SM2 signing example";
    unsigned char signature[128];
    unsigned char public_key[128];
    int signature_size = (int)sizeof(signature);
    int public_key_size = (int)sizeof(public_key);
    unsigned int r_size = 0;
    unsigned int s_size = 0;
    int business = kHsedHsedResult_DEVICE_ERROR;
    int transport;
    TInt64 handle = 0;
    int object_created = 0;
    int result = 1;

    SCInit();

    transport = HcHsed_Init();
    if (transport != SC_rSuccess) {
        (void)fprintf(stderr, "HcHsed_Init failed: %d\n", transport);
        goto finish_spc;
    }

    transport = HcHsed_Open();
    if (transport != SC_rSuccess) {
        (void)fprintf(stderr, "HcHsed_Open failed: %d\n", transport);
        goto finish_client;
    }

    transport = HcHsed_HseCreate(&business, &handle);
    if (CheckResult("HseCreate", transport, business) != 0) {
        goto finish_connection;
    }
    object_created = 1;

    transport = HcHsed_SetPropertyBytes(
        handle, kHsedHsedPropertyId_USER_ID,
        user_id, (int)sizeof(user_id), &business);
    if (CheckResult("Set USER_ID", transport, business) != 0) {
        goto finish_object;
    }

    /*
     * SM2 固定使用公钥槽位 11、私钥槽位 12。
     * HASH_ALGORITHM 默认 SM3，SIGN_INPUT_MODE 默认 MESSAGE，
     * 本例无需重复设置。
     */

    transport = HcHsed_ExportPublicKey(
        handle, kHsedHsedAlgorithm_SM2, &business,
        public_key, &public_key_size);
    if (CheckResult("ExportPublicKey", transport, business) != 0) {
        goto finish_object;
    }

    transport = HcHsed_Sign(
        handle, kHsedHsedAlgorithm_SM2,
        message, (int)(sizeof(message) - 1),
        &business, signature, &signature_size, &r_size, &s_size);
    if (CheckResult("Sign", transport, business) != 0) {
        goto finish_object;
    }

    if ((signature_size != 64) || (r_size != 32U) || (s_size != 32U)) {
        (void)fprintf(stderr,
                      "unexpected SM2 signature layout: total=%d r=%u s=%u\n",
                      signature_size, r_size, s_size);
        goto finish_object;
    }

    (void)printf("SM2 sign success: public_key=%d, signature=%d\n",
                 public_key_size, signature_size);
    result = 0;

finish_object:
    if (object_created != 0) {
        business = kHsedHsedResult_DEVICE_ERROR;
        transport = HcHsed_HseDestroy(handle, &business);
        if (CheckResult("HseDestroy", transport, business) != 0) {
            result = 1;
        }
    }
finish_connection:
    HcHsed_Close();
finish_client:
    HcHsed_Done();
finish_spc:
    SCFini();
    return result;
}
```

### 7.1 SM2 USER_ID 与 ZA 规则

当前 CIU98 适配实现对 SM2 原文签名执行带 ZA 的签名：

```text
ZA = SM3(ENTL || USER_ID || a || b || Gx || Gy || Px || Py)
e  = SM3(ZA || M)
signature = SM2Sign(private_key, e)
```

调用约束：

- `USER_ID` 至少提供 16 字节；
- CIU98 适配层只使用 `USER_ID` 的前 16 字节计算 ZA；
- 为避免调用方误解，SM2 建议始终恰好传 16 字节；
- 相同密钥、相同原文但 `USER_ID` 不同时，ZA 不同，验签必须使用对应的用户 ID；
- `DIGEST` 模式表示输入已经是摘要，不会按原文模式再次组合 ZA；
- 生成头文件中 `USER_ID` 的“固定 32 字节”注释与当前实现不一致，应以本节和实际适配实现为准。

网页验证时，十六进制用户 ID
`123456789ABBCCC00123456789ABBCCC` 恰好表示 16 个原始字节。不要把这 32 个字符本身作为 32 字节 ASCII 数据传入。

### 7.2 SM2 输出格式

当前二进制格式约定：

| 输出 | 格式 |
|---|---|
| SM2 公钥 | 64 字节 `X || Y`，X、Y 各 32 字节，不含 `04` 前缀 |
| SM2 签名 | 64 字节 `r || s`，r、s 各 32 字节，不是 ASN.1 DER |

对接只接受 DER 签名的第三方库时，调用方需要把两个 32 字节无符号整数编码为 ASN.1 `SEQUENCE(INTEGER r, INTEGER s)`；该转换不是 HSED 接口的一部分。

## 8. 公共客户端 API

### 8.1 运行与连接

| 函数 | 作用 | 说明 |
|---|---|---|
| `HcHsed_Init` | 创建生成客户端的全局连接对象 | 进程内调用一次 |
| `HcHsed_Done` | 释放全局连接对象 | 应在 `HcHsed_Close` 后调用 |
| `HcHsed_Open` | 打开连接 | 返回 SPC 状态 |
| `HcHsed_Close` | 关闭连接 | 无返回值 |
| `HcHsed_ConnState` | 查询当前连接状态 | 用于诊断连接状态 |
| `HcHsed_Conn` | 头文件声明获取连接对象 | 当前生成 `.c` 未实现，不要调用 |

### 8.2 对象与属性

| 函数 | 作用 | 关键参数/输出 |
|---|---|---|
| `HcHsed_HseCreate` | 创建功能对象 | 输出业务结果和 `handle` |
| `HcHsed_HseDestroy` | 销毁功能对象 | `handle` 必须属于当前连接 |
| `HcHsed_SetPropertyInt` | 设置可写整数属性 | KEY_ID 类属性只读，修改返回 `PERMISSION_DENIED` |
| `HcHsed_SetPropertyBytes` | 设置字节属性 | 缓冲区及显式长度 |
| `HcHsed_GetPropertyInt` | 读取整数属性 | 输出业务结果和整数值 |
| `HcHsed_GetPropertyBytes` | 读取字节属性 | 输出缓冲区容量/实际长度 |

### 8.3 密码运算

| 函数 | 作用 | 关键约定 |
|---|---|---|
| `HcHsed_Encrypt` | 加密 | 使用固定槽位；按需配置模式、填充和必要的 IV |
| `HcHsed_Decrypt` | 解密 | 属性组合必须与密文生成方式一致 |
| `HcHsed_Sign` | 签名 | 返回签名、总长度以及 `r_size`、`s_size` |
| `HcHsed_ExportPublicKey` | 导出公钥 | 私钥禁止导出 |
| `HcHsed_ImportPublicKey` | 兼容保留 | 当前禁止客户端导入或覆盖，返回 `PERMISSION_DENIED` |

函数原型的权威来源为 `business/spc/hsed/generated/hsed_client.h`。接口模型发生变化后，应重新生成客户端并同步更新本文档。

## 9. 算法与属性配置

`THcHsedHsedAlgorithm` 声明了：

- 对称算法：`AES128`、`TDES128`、`SM1`、`SM4`；
- 非对称算法：`RSA1024`、`RSA2048`、`ECC256`、`SM2`、`SM9`；
- 摘要标识：`SHA1`、`SHA256`、`SHA384`、`SHA512`、`SM3`。

摘要标识主要作为签名的 `HASH_ALGORITHM` 属性值；当前阶段没有独立摘要 RPC。

### 9.1 属性模式

`HcHsed_HseCreate` 会装载全部属性默认值。固定密钥槽位如下：

| 算法 | 固定槽位 |
|---|---|
| AES128 / TDES128 / SM1 / SM4 | 1 / 2 / 3 / 4 |
| RSA1024 公钥 / 私钥 | 5 / 6 |
| RSA2048 公钥 / 私钥 | 7 / 8 |
| ECC256 公钥 / 私钥 | 9 / 10 |
| SM2 公钥 / 私钥 | 11 / 12 |
| SM9 公钥 / 私钥 | 13 / 14 |

所有 `KEY_ID`、`PUBLIC_KEY_ID`、`PRIVATE_KEY_ID` 均为 Int 只读属性。客户端调用 `HcHsed_SetPropertyInt` 修改时，业务返回值为 `kHsedHsedResult_PERMISSION_DENIED`。

| 场景 | 需要关注的属性 |
|---|---|
| 通用 | `USER_ID`、`CHIP_ID` |
| 对称加解密 | `<ALG>_KEY_ID`、`<ALG>_CIPHER_MODE`、`<ALG>_PADDING`、CBC 时的 `<ALG>_IV` |
| RSA 签名 | `RSA*_PRIVATE_KEY_ID`、`RSA*_HASH_ALGORITHM`、`RSA*_SIGN_INPUT_MODE`、填充 |
| ECC 签名 | `ECC256_PRIVATE_KEY_ID`、`ECC256_HASH_ALGORITHM`、`ECC256_SIGN_INPUT_MODE` |
| SM2 签名 | `USER_ID`、`SM2_PUBLIC_KEY_ID`、`SM2_PRIVATE_KEY_ID`、`SM2_HASH_ALGORITHM`、`SM2_SIGN_INPUT_MODE` |
| 公钥导出 | 对应算法的只读 `PUBLIC_KEY_ID` |
| 非对称解密 | 对应算法的 `PRIVATE_KEY_ID` |

仅在业务需要偏离默认模式时设置可写属性。SM2 默认使用 SM3 和 MESSAGE 模式；槽位属性不需要、也不允许重复设置。

### 9.2 属性写入类型

- `*_CIPHER_MODE`、`*_PADDING`、`*_HASH_ALGORITHM`、`*_SIGN_INPUT_MODE`、`*_PUBLIC_KEY_FORMAT`、`*_SM9_MODE` 使用 `HcHsed_SetPropertyInt`；
- `*_KEY_ID`、`*_PUBLIC_KEY_ID`、`*_PRIVATE_KEY_ID` 是 Int 只读属性，只能通过 `HcHsed_GetPropertyInt` 读取；
- `USER_ID`、`CHIP_ID`、`*_IV` 使用 `HcHsed_SetPropertyBytes`；
- 当前固定密钥策略禁止客户端调用 `HcHsed_ImportPublicKey` 更新公钥。

## 10. 单进程多线程、多连接

### 10.1 不要在线程间共享普通生成客户端

`hsed_client.c` 内部只有一个进程级静态连接。所有 `HcHsed_HseCreate`、`HcHsed_Sign` 等高层函数都使用该连接，因此它适合普通单连接调用，不适合把每个线程作为独立通道。

对象句柄也不能从线程 A 的连接传到线程 B 的连接。

### 10.2 每线程独立连接

需要“单进程、多线程、多次连接”时：

1. 主线程只调用一次 `SCInit()`；
2. 每个工作线程调用 `HcHsedConn_Create(NULL)`；
3. 每个线程调用 `HcHsedConn_Open(connection)`；
4. 使用 `SCCmdCreate`、`SCCmdBegin`、参数编码和 `SCCmdExecute` 在指定连接上调用 HSED 命令；
5. 每条连接独立执行 `HseCreate` 并保存自己的 `handle`；
6. 线程结束前执行 `HseDestroy`、`HcHsedConn_Close`、`HcHsedConn_Free`；
7. 所有线程退出后，主线程调用 `SCFini()`。

已验证的完整实现位于：

```text
test/hsed_spc_sm2_stress/hsed_spc_sm2_stress.c
```

该测试实现了每通道独立连接、每通道独立 16 字节 `USER_ID`、对相同原文进行 SM2 签名，并记录每次 `Sign` RPC 的微秒延迟和每秒调用次数。

底层协议命令名称与公共 RPC 一一对应：

| 命令名 | 用途 |
|---|---|
| `HseCreate` | 创建对象 |
| `HseDestroy` | 销毁对象 |
| `SetPropertyInt` | 设置整数属性 |
| `SetPropertyBytes` | 设置字节属性 |
| `GetPropertyInt` | 读取整数属性 |
| `GetPropertyBytes` | 读取字节属性 |
| `Encrypt` | 加密 |
| `Decrypt` | 解密 |
| `Sign` | 签名 |
| `ExportPublicKey` | 导出公钥 |
| `ImportPublicKey` | 导入公钥 |

底层 `SCCmd*` 的字段序号属于生成协议内部约定。除多连接需求外，优先使用 `HcHsed_*` 公共包装接口；需要底层调用时应复用现有压力测试的封装，不要自行猜测字段顺序。

## 11. 业务错误码

| 枚举 | 值 | 含义/处理建议 |
|---|---:|---|
| `kHsedHsedResult_OK` | 0 | 成功 |
| `kHsedHsedResult_INVALID_PARAM` | -1 | 参数、必需属性或属性组合错误 |
| `kHsedHsedResult_UNSUPPORTED` | -2 | 算法、模式、属性或功能不支持 |
| `kHsedHsedResult_PERMISSION_DENIED` | -3 | 无权限访问操作或槽位 |
| `kHsedHsedResult_KEY_NOT_FOUND` | -4 | 指定密钥槽位不存在 |
| `kHsedHsedResult_KEY_OCCUPIED` | -5 | 槽位已占用且不允许覆盖 |
| `kHsedHsedResult_BUSY` | -6 | 对象、设备或槽位忙 |
| `kHsedHsedResult_OBJECT_NOT_FOUND` | -7 | 对象不存在、已销毁或不属于当前连接 |
| `kHsedHsedResult_OBJECT_STATE_ERROR` | -8 | 当前对象状态不允许此操作 |
| `kHsedHsedResult_DATA_TOO_LARGE` | -9 | 数据超过 RPC、算法或 CIU98 限制 |
| `kHsedHsedResult_NO_RESOURCE` | -10 | 对象、内存或任务队列资源不足 |
| `kHsedHsedResult_TIMEOUT` | -11 | IPC 或硬件操作超时 |
| `kHsedHsedResult_DEVICE_UNAVAILABLE` | -12 | 芯片未连接或不可用 |
| `kHsedHsedResult_AUTH_LOCKED` | -13 | 芯片认证失败并进入锁定状态 |
| `kHsedHsedResult_DEVICE_ERROR` | -14 | 其他芯片/SDK 错误，查看 HSED 服务日志 |

`BUSY`、`TIMEOUT` 是否重试应由业务决定。涉及签名、加密等有副作用或审计要求的请求，重试前应确认上一次请求是否已经在服务端完成。

## 12. 公钥导入安全约束

- 只允许导入公钥，禁止通过 HSED 导入私钥；
- `replace_existing_key = False` 时，目标槽位已存在应返回 `KEY_OCCUPIED`；
- 覆盖公钥槽位属于高风险操作，调用方必须经过业务授权；
- 通信、认证或算法不匹配时，不应通过盲目覆盖槽位“修复”；
- 导入后建议立即导出并由调用方比较公钥内容。

## 13. 服务运行前提

客户端成功调用依赖：

- HSED 服务已启动；
- SPC 运行环境与服务名配置正确；
- CIU98 设备可访问；
- HSED 配置已加载正确的厂商适配库；
- 使用的密钥槽位已正确配置并具备权限。

本项目配置文件 `doc/hsed.ini` 中，CIU98 适配库配置项为：

```ini
[bsc_hse]
vendor_so_path=/usr/lib/libbsc_hse_ciu98_ylb04_a6.so
```

## 14. 常见问题

### 14.1 SPC 返回成功，但 `_ret_` 失败

说明 RPC 已到达 HSED，失败发生在参数校验、对象状态、槽位权限或 CIU98 操作层。按业务错误码检查属性和服务日志。

### 14.2 返回 `OBJECT_NOT_FOUND`

常见原因是对象已经销毁、连接重连后继续使用旧句柄，或把其他连接创建的句柄拿到当前连接使用。

### 14.3 SM2 签名在外部工具中验签失败

依次检查：

1. 外部工具选择的是 SM2 带 Z/ZA 验签；
2. 用户 ID 的原始字节与签名时一致；
3. 签名输入是相同原文，不是原文的十六进制字符串；
4. 公钥按 `X || Y` 解析，必要时为工具补 `04` 前缀；
5. 签名按 `r || s` 解析；若工具要求 DER，先转换格式；
6. 不要把 32 字符的十六进制 ID 当成 32 字节 ASCII。

### 14.4 不同通道对相同数据签名结果是否应不同

当各通道的 `USER_ID` 前 16 字节不同时，ZA 和待签摘要不同，因此验签必须使用各通道对应的用户 ID。SM2 签名本身还使用随机数，即使用户 ID 相同，合法签名值也不要求每次完全相同。

### 14.5 能否共享一个 handle

同一连接内可以按业务串行使用同一对象；不能跨 SPC 连接或跨进程共享。多线程独立连接时，每个线程必须创建自己的对象。

## 15. 参考实现与权威来源

| 文件 | 用途 |
|---|---|
| `business/spc/hsed/generated/hsed_client.h` | 公共客户端函数原型 |
| `business/spc/hsed/generated/hsed_datatype.h` | 算法、属性、业务结果枚举 |
| `business/spc/hsed/generated/hsed_conn.h` | 指定连接的生命周期辅助函数 |
| `test/hsed_spc_smoke/hsed_spc_smoke.c` | 单连接基础接口示例 |
| `test/hsed_spc_sm2_stress/hsed_spc_sm2_stress.c` | 单进程、多线程、每线程独立连接的 SM2 示例 |
| `doc/hsed.xlsx` | SPC 对外接口权威模型 |
| `doc/hsed.ini` | HSED 服务配置示例 |

生成代码与 `doc/hsed.xlsx` 不一致时，以模型确认并重新生成接口代码；运行行为与生成注释不一致时，应同时修正模型、实现和文档。
