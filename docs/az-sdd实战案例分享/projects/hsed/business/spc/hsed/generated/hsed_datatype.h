/**
 * @file hsed_datatype.h
 * @brief
 * @note
 * @author spc_builder
 * @date 2026-07-27 21:27:08
 * @version 1.0

 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _HSED_DATATYPE_H_
#define _HSED_DATATYPE_H_

#include <ofc_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * HSED统一返回码
 */
typedef enum THcHsedHsedResultTag
{
    kHsedHsedResult_OK = 0,  ///< 操作成功
    kHsedHsedResult_INVALID_PARAM = -1,  ///< 参数非法、缺少必要属性或属性组合不匹配
    kHsedHsedResult_UNSUPPORTED = -2,  ///< 算法、模式、属性或功能不支持
    kHsedHsedResult_PERMISSION_DENIED = -3,  ///< 调用方无权访问该操作或密钥槽位
    kHsedHsedResult_KEY_NOT_FOUND = -4,  ///< 密钥槽位不存在
    kHsedHsedResult_KEY_OCCUPIED = -5,  ///< 密钥槽位已占用且未允许覆盖
    kHsedHsedResult_BUSY = -6,  ///< 对象、设备或密钥槽位正在执行操作
    kHsedHsedResult_OBJECT_NOT_FOUND = -7,  ///< 对象不存在、已销毁或不属于当前SPC连接
    kHsedHsedResult_OBJECT_STATE_ERROR = -8,  ///< 对象状态不允许当前操作，例如执行期间修改属性
    kHsedHsedResult_DATA_TOO_LARGE = -9,  ///< 输入或输出超过SPC、算法或CIU98 SDK实际限制
    kHsedHsedResult_NO_RESOURCE = -10,  ///< 对象、内存或任务队列资源不足
    kHsedHsedResult_TIMEOUT = -11,  ///< IPC或硬件操作超时
    kHsedHsedResult_DEVICE_UNAVAILABLE = -12,  ///< 加密芯片未连接或不可用
    kHsedHsedResult_AUTH_LOCKED = -13,  ///< 芯片认证失败并进入锁定状态
    kHsedHsedResult_DEVICE_ERROR = -14,  ///< 其他芯片或SDK错误，详细原因写入服务日志
} THcHsedHsedResult;

/**
 * 16位密码算法标识，格式0xTFMS：T=类型、F=算法族、M=成员或代际、S=规格序号；由密码业务接口直接指定，也嵌入完整HsedPropertyId
 */
typedef enum THcHsedHsedAlgorithmTag
{
    kHsedHsedAlgorithm_NONE = 0x0000,  ///< 无效值
    kHsedHsedAlgorithm_AES128 = 0x1101,  ///< 对称/AES/基础/规格1（128位）
    kHsedHsedAlgorithm_TDES128 = 0x1231,  ///< 对称/DES/3DES/规格1（双长度3DES）
    kHsedHsedAlgorithm_SM1 = 0x1310,  ///< 对称/SM/成员1
    kHsedHsedAlgorithm_SM4 = 0x1340,  ///< 对称/SM/成员4
    kHsedHsedAlgorithm_RSA1024 = 0x2401,  ///< 非对称/RSA/基础/规格1（1024位）
    kHsedHsedAlgorithm_RSA2048 = 0x2402,  ///< 非对称/RSA/基础/规格2（2048位）
    kHsedHsedAlgorithm_ECC256 = 0x2501,  ///< 非对称/ECC/基础/规格1（CIU98配置的256位曲线）
    kHsedHsedAlgorithm_SM2 = 0x2320,  ///< 非对称/SM/成员2
    kHsedHsedAlgorithm_SM9 = 0x2390,  ///< 非对称/SM/成员9
    kHsedHsedAlgorithm_SHA1 = 0x3610,  ///< 哈希/SHA/SHA-1
    kHsedHsedAlgorithm_SHA256 = 0x3622,  ///< 哈希/SHA/SHA-2/规格2（256位）
    kHsedHsedAlgorithm_SHA384 = 0x3623,  ///< 哈希/SHA/SHA-2/规格3（384位）
    kHsedHsedAlgorithm_SHA512 = 0x3624,  ///< 哈希/SHA/SHA-2/规格4（512位）
    kHsedHsedAlgorithm_SM3 = 0x3330,  ///< 哈希/SM/成员3
} THcHsedHsedAlgorithm;

/**
 * 对称加解密模式；通过SetPropertyInt直接传递枚举值
 */
typedef enum THcHsedHsedCipherModeTag
{
    kHsedHsedCipherMode_NONE = 0,  ///< 非对称算法或不适用
    kHsedHsedCipherMode_ECB = 1,  ///< ECB模式
    kHsedHsedCipherMode_CBC = 2,  ///< CBC模式
} THcHsedHsedCipherMode;

/**
 * 密码运算补位方式；通过SetPropertyInt直接传递枚举值
 */
typedef enum THcHsedHsedPaddingTag
{
    kHsedHsedPadding_NONE = 0,  ///< 不补位
    kHsedHsedPadding_PKCS7 = 1,  ///< 对称加解密PKCS#7补位
    kHsedHsedPadding_PKCS1 = 2,  ///< RSA PKCS#1补位
} THcHsedHsedPadding;

/**
 * 签名输入类型；通过SetPropertyInt直接传递枚举值
 */
typedef enum THcHsedHsedSignInputModeTag
{
    kHsedHsedSignInputMode_MESSAGE = 0,  ///< 输入原文，由芯片完成摘要和签名
    kHsedHsedSignInputMode_DIGEST = 1,  ///< 输入调用方预计算摘要，并严格校验摘要长度
} THcHsedHsedSignInputMode;

/**
 * SM9加解密模式；通过SetPropertyInt直接传递枚举值
 */
typedef enum THcHsedHsedSm9ModeTag
{
    kHsedHsedSm9Mode_NONE = 0,  ///< 非SM9算法
    kHsedHsedSm9Mode_STREAM = 1,  ///< SM9序列密码模式
    kHsedHsedSm9Mode_SYMMETRIC = 2,  ///< SM9分组密码模式
} THcHsedHsedSm9Mode;

/**
 * 可直接传给属性接口的完整属性ID枚举；格式0x00AAAAPP，AAAA为16位HsedAlgorithm，PP为属性编号；公共属性算法段为0000
 */
typedef enum THcHsedHsedPropertyIdTag
{
    kHsedHsedPropertyId_USER_ID = 0x00000001,  ///< Bytes；默认空；可读写。长度1~128字节；SM2签名仅使用前16字节参与ZA计算。
    kHsedHsedPropertyId_CHIP_ID = 0x00000002,  ///< Bytes；默认读取芯片当前值；只允许首次配置，后续修改返回KEY_OCCUPIED。
    kHsedHsedPropertyId_AES128_KEY_ID = 0x00110103,  ///< Int；固定默认槽位1；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK无持久对称密钥的芯片内生成接口，自动补齐需补充安全注入方案。
    kHsedHsedPropertyId_AES128_CIPHER_MODE = 0x00110104,  ///< Int；默认ECB；可读写；值为HsedCipherMode。
    kHsedHsedPropertyId_AES128_PADDING = 0x00110105,  ///< Int；默认NONE；可读写；值为HsedPadding。
    kHsedHsedPropertyId_AES128_IV = 0x00110109,  ///< Bytes；默认未设置；可读写；CBC模式必须设置16字节。
    kHsedHsedPropertyId_TDES128_KEY_ID = 0x00123103,  ///< Int；固定默认槽位2；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK无持久对称密钥的芯片内生成接口，自动补齐需补充安全注入方案。
    kHsedHsedPropertyId_TDES128_CIPHER_MODE = 0x00123104,  ///< Int；默认ECB；可读写；值为HsedCipherMode。
    kHsedHsedPropertyId_TDES128_PADDING = 0x00123105,  ///< Int；默认NONE；可读写；值为HsedPadding。
    kHsedHsedPropertyId_TDES128_IV = 0x00123109,  ///< Bytes；默认未设置；可读写；CBC模式必须设置8字节。
    kHsedHsedPropertyId_SM1_KEY_ID = 0x00131003,  ///< Int；固定默认槽位3；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK无持久对称密钥的芯片内生成接口，自动补齐需补充安全注入方案。
    kHsedHsedPropertyId_SM1_CIPHER_MODE = 0x00131004,  ///< Int；默认ECB；可读写；值为HsedCipherMode。
    kHsedHsedPropertyId_SM1_PADDING = 0x00131005,  ///< Int；默认NONE；可读写；值为HsedPadding。
    kHsedHsedPropertyId_SM1_IV = 0x00131009,  ///< Bytes；默认未设置；可读写；CBC模式必须设置16字节。
    kHsedHsedPropertyId_SM4_KEY_ID = 0x00134003,  ///< Int；固定默认槽位4；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK无持久对称密钥的芯片内生成接口，自动补齐需补充安全注入方案。
    kHsedHsedPropertyId_SM4_CIPHER_MODE = 0x00134004,  ///< Int；默认ECB；可读写；值为HsedCipherMode。
    kHsedHsedPropertyId_SM4_PADDING = 0x00134005,  ///< Int；默认NONE；可读写；值为HsedPadding。
    kHsedHsedPropertyId_SM4_IV = 0x00134009,  ///< Bytes；默认未设置；可读写；CBC模式必须设置16字节。
    kHsedHsedPropertyId_RSA1024_PUBLIC_KEY_ID = 0x0024010C,  ///< Int；固定默认槽位5；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_RSA1024_PRIVATE_KEY_ID = 0x0024010D,  ///< Int；固定默认槽位6；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_RSA1024_PADDING = 0x00240105,  ///< Int；默认PKCS1；可读写；值为HsedPadding。
    kHsedHsedPropertyId_RSA1024_HASH_ALGORITHM = 0x00240106,  ///< Int；默认SHA256；可读写；值为哈希类型HsedAlgorithm。
    kHsedHsedPropertyId_RSA1024_SIGN_INPUT_MODE = 0x00240107,  ///< Int；默认MESSAGE；可读写；值为HsedSignInputMode。
    kHsedHsedPropertyId_RSA1024_PUBLIC_KEY_FORMAT = 0x0024010A,  ///< Int；默认BIN(0)；可读写；值为HsedPublicKeyFormat。
    kHsedHsedPropertyId_RSA2048_PUBLIC_KEY_ID = 0x0024020C,  ///< Int；固定默认槽位7；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_RSA2048_PRIVATE_KEY_ID = 0x0024020D,  ///< Int；固定默认槽位8；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_RSA2048_PADDING = 0x00240205,  ///< Int；默认PKCS1；可读写；值为HsedPadding。
    kHsedHsedPropertyId_RSA2048_HASH_ALGORITHM = 0x00240206,  ///< Int；默认SHA256；可读写；值为哈希类型HsedAlgorithm。
    kHsedHsedPropertyId_RSA2048_SIGN_INPUT_MODE = 0x00240207,  ///< Int；默认MESSAGE；可读写；值为HsedSignInputMode。
    kHsedHsedPropertyId_RSA2048_PUBLIC_KEY_FORMAT = 0x0024020A,  ///< Int；默认BIN(0)；可读写；值为HsedPublicKeyFormat。
    kHsedHsedPropertyId_ECC256_PUBLIC_KEY_ID = 0x0025010C,  ///< Int；固定默认槽位9；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_ECC256_PRIVATE_KEY_ID = 0x0025010D,  ///< Int；固定默认槽位10；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_ECC256_HASH_ALGORITHM = 0x00250106,  ///< Int；默认SHA256；可读写；值为哈希类型HsedAlgorithm。
    kHsedHsedPropertyId_ECC256_SIGN_INPUT_MODE = 0x00250107,  ///< Int；默认MESSAGE；可读写；值为HsedSignInputMode。
    kHsedHsedPropertyId_ECC256_PUBLIC_KEY_FORMAT = 0x0025010A,  ///< Int；默认BIN(0)；可读写；值为HsedPublicKeyFormat。
    kHsedHsedPropertyId_SM2_PUBLIC_KEY_ID = 0x0023200C,  ///< Int；固定默认槽位11；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_SM2_PRIVATE_KEY_ID = 0x0023200D,  ///< Int；固定默认槽位12；只读；SetPropertyInt修改返回PERMISSION_DENIED。服务初始化时检查密钥；仅在明确不存在时调用芯片内部生成。
    kHsedHsedPropertyId_SM2_HASH_ALGORITHM = 0x00232006,  ///< Int；默认SM3；可读写；值为哈希类型HsedAlgorithm。
    kHsedHsedPropertyId_SM2_SIGN_INPUT_MODE = 0x00232007,  ///< Int；默认MESSAGE；可读写；值为HsedSignInputMode。
    kHsedHsedPropertyId_SM2_PUBLIC_KEY_FORMAT = 0x0023200A,  ///< Int；默认BIN(0)；可读写；值为HsedPublicKeyFormat。
    kHsedHsedPropertyId_SM9_PUBLIC_KEY_ID = 0x0023900C,  ///< Int；固定默认槽位13；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK不支持SM9密钥对生成，自动补齐需供应商能力。
    kHsedHsedPropertyId_SM9_PRIVATE_KEY_ID = 0x0023900D,  ///< Int；固定默认槽位14；只读；SetPropertyInt修改返回PERMISSION_DENIED。CIU98现有SDK不支持SM9密钥对生成，自动补齐需供应商能力。
    kHsedHsedPropertyId_SM9_HASH_ALGORITHM = 0x00239006,  ///< Int；默认SM3；可读写；值为哈希类型HsedAlgorithm。
    kHsedHsedPropertyId_SM9_SIGN_INPUT_MODE = 0x00239007,  ///< Int；默认MESSAGE；可读写；值为HsedSignInputMode。
    kHsedHsedPropertyId_SM9_SM9_MODE = 0x00239008,  ///< Int；默认STREAM；可读写；值为HsedSm9Mode。
    kHsedHsedPropertyId_SM9_PUBLIC_KEY_FORMAT = 0x0023900A,  ///< Int；默认BIN(0)；可读写；值为HsedPublicKeyFormat。
} THcHsedHsedPropertyId;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // _HSED_DATATYPE_H_
