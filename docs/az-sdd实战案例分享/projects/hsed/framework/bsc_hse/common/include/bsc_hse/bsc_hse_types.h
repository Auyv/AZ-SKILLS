/**
 * @addtogroup bsc_hse
 * bsc_hse
 * @{
 */
/**
 * @file        bsc_hse_types.h
 * @brief       BSC HSE 公共独立类型定义
 *              - 不包含任何供应商 SDK 头文件
 *              - 不暴露 HSED 生成代码类型
 * @note        枚举采用 project C 编码规范格式
 * @author      HSED
 * @date        2026/07/21
 * @version     2.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2026/07/21   <td>1.0         <td>HSED        <td> 创建初始版本
 * <tr><td>2026/07/21   <td>2.0         <td>HSED        <td> 编码规范对齐，删除TBscHseConfig
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _BSC_HSE_TYPES_H_
#define _BSC_HSE_TYPES_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ============ BSC HSE 统一结果 ============ */

enum TbscHseResultTag
{
    kBscHseResultOk               =  0,
    kBscHseResultInvalidParam     = -1,
    kBscHseResultUnsupported      = -2,
    kBscHseResultPermissionDenied = -3,
    kBscHseResultKeyNotFound      = -4,
    kBscHseResultKeyOccupied      = -5,
    kBscHseResultBusy             = -6,
    kBscHseResultObjectNotFound   = -7,
    kBscHseResultObjectStateError = -8,
    kBscHseResultDataTooLarge     = -9,
    kBscHseResultNoResource       = -10,
    kBscHseResultTimeout          = -11,
    kBscHseResultDeviceUnavailable = -12,
    kBscHseResultAuthLocked       = -13,
    kBscHseResultDeviceError      = -14,
};
typedef enum TbscHseResultTag TbscHseResult;

/* ============ 算法标识 ============ */

enum TbscHseAlgorithmTag
{
    kBscHseAlgorithmNone    = 0x0000,
    kBscHseAlgorithmAes128  = 0x1101,
    kBscHseAlgorithmTdes128 = 0x1231,
    kBscHseAlgorithmSm1     = 0x1310,
    kBscHseAlgorithmSm4     = 0x1340,
    kBscHseAlgorithmRsa1024 = 0x2401,
    kBscHseAlgorithmRsa2048 = 0x2402,
    kBscHseAlgorithmEcc256  = 0x2501,
    kBscHseAlgorithmSm2     = 0x2320,
    kBscHseAlgorithmSm9     = 0x2390,
    kBscHseAlgorithmSha1    = 0x3610,
    kBscHseAlgorithmSha256  = 0x3622,
    kBscHseAlgorithmSha384  = 0x3623,
    kBscHseAlgorithmSha512  = 0x3624,
    kBscHseAlgorithmSm3     = 0x3330,
};
typedef enum TbscHseAlgorithmTag TbscHseAlgorithm;

/* ============ 模式 / 补位 / 签名输入 / SM9 ============ */

enum TbscHseCipherModeTag
{
    kBscHseCipherModeNone = 0,
    kBscHseCipherModeEcb  = 1,
    kBscHseCipherModeCbc  = 2,
};
typedef enum TbscHseCipherModeTag TbscHseCipherMode;

enum TbscHsePaddingTag
{
    kBscHsePaddingNone  = 0,
    kBscHsePaddingPkcs7 = 1,
    kBscHsePaddingPkcs1 = 2,
};
typedef enum TbscHsePaddingTag TbscHsePadding;

enum TbscHseSignInputModeTag
{
    kBscHseSignInputModeMessage = 0,
    kBscHseSignInputModeDigest  = 1,
};
typedef enum TbscHseSignInputModeTag TbscHseSignInputMode;

enum TbscHseSm9ModeTag
{
    kBscHseSm9ModeNone      = 0,
    kBscHseSm9ModeStream    = 1,
    kBscHseSm9ModeSymmetric = 2,
};
typedef enum TbscHseSm9ModeTag TbscHseSm9Mode;

enum TbscHseKeyRoleTag
{
    kBscHseKeyRoleInvalid   = 0,
    kBscHseKeyRoleSymmetric = 1,
    kBscHseKeyRolePublic    = 2,
    kBscHseKeyRolePrivate   = 3,
};
typedef enum TbscHseKeyRoleTag TbscHseKeyRole;

/* ============ 完整属性 ID（0x00AAAAPP） ============ */

enum TbscHsePropertyIdTag
{
    kBscHsePropertyIdUserId                  = 0x00000001,  ///< Bytes；默认空；可读写
    kBscHsePropertyIdChipId                  = 0x00000002,  ///< Bytes；默认读取芯片当前值；只允许首次配置
    kBscHsePropertyIdAes128KeyId             = 0x00110103,  ///< Int；默认 1；只读
    kBscHsePropertyIdAes128CipherMode        = 0x00110104,  ///< Int；默认 ECB；可读写
    kBscHsePropertyIdAes128Padding           = 0x00110105,  ///< Int；默认 NONE；可读写
    kBscHsePropertyIdAes128Iv                = 0x00110109,  ///< Bytes；默认未设置；可读写
    kBscHsePropertyIdTdes128KeyId            = 0x00123103,  ///< Int；默认 2；只读
    kBscHsePropertyIdTdes128CipherMode       = 0x00123104,  ///< Int；默认 ECB；可读写
    kBscHsePropertyIdTdes128Padding          = 0x00123105,  ///< Int；默认 NONE；可读写
    kBscHsePropertyIdTdes128Iv               = 0x00123109,  ///< Bytes；默认未设置；可读写
    kBscHsePropertyIdSm1KeyId                = 0x00131003,  ///< Int；默认 3；只读
    kBscHsePropertyIdSm1CipherMode           = 0x00131004,  ///< Int；默认 ECB；可读写
    kBscHsePropertyIdSm1Padding              = 0x00131005,  ///< Int；默认 NONE；可读写
    kBscHsePropertyIdSm1Iv                   = 0x00131009,  ///< Bytes；默认未设置；可读写
    kBscHsePropertyIdSm4KeyId                = 0x00134003,  ///< Int；默认 4；只读
    kBscHsePropertyIdSm4CipherMode           = 0x00134004,  ///< Int；默认 ECB；可读写
    kBscHsePropertyIdSm4Padding              = 0x00134005,  ///< Int；默认 NONE；可读写
    kBscHsePropertyIdSm4Iv                   = 0x00134009,  ///< Bytes；默认未设置；可读写
    kBscHsePropertyIdRsa1024PublicKeyId      = 0x0024010C,  ///< Int；默认 5；只读
    kBscHsePropertyIdRsa1024PrivateKeyId     = 0x0024010D,  ///< Int；默认 6；只读
    kBscHsePropertyIdRsa1024Padding          = 0x00240105,  ///< Int；默认 PKCS1；可读写
    kBscHsePropertyIdRsa1024HashAlgorithm    = 0x00240106,  ///< Int；默认 SHA256；可读写
    kBscHsePropertyIdRsa1024SignInputMode    = 0x00240107,  ///< Int；默认 MESSAGE；可读写
    kBscHsePropertyIdRsa1024PublicKeyFormat  = 0x0024010A,  ///< Int；默认 BIN(0)；可读写
    kBscHsePropertyIdRsa2048PublicKeyId      = 0x0024020C,  ///< Int；默认 7；只读
    kBscHsePropertyIdRsa2048PrivateKeyId     = 0x0024020D,  ///< Int；默认 8；只读
    kBscHsePropertyIdRsa2048Padding          = 0x00240205,  ///< Int；默认 PKCS1；可读写
    kBscHsePropertyIdRsa2048HashAlgorithm    = 0x00240206,  ///< Int；默认 SHA256；可读写
    kBscHsePropertyIdRsa2048SignInputMode    = 0x00240207,  ///< Int；默认 MESSAGE；可读写
    kBscHsePropertyIdRsa2048PublicKeyFormat  = 0x0024020A,  ///< Int；默认 BIN(0)；可读写
    kBscHsePropertyIdEcc256PublicKeyId       = 0x0025010C,  ///< Int；默认 9；只读
    kBscHsePropertyIdEcc256PrivateKeyId      = 0x0025010D,  ///< Int；默认 10；只读
    kBscHsePropertyIdEcc256HashAlgorithm     = 0x00250106,  ///< Int；默认 SHA256；可读写
    kBscHsePropertyIdEcc256SignInputMode     = 0x00250107,  ///< Int；默认 MESSAGE；可读写
    kBscHsePropertyIdEcc256PublicKeyFormat   = 0x0025010A,  ///< Int；默认 BIN(0)；可读写
    kBscHsePropertyIdSm2PublicKeyId          = 0x0023200C,  ///< Int；默认 11；只读
    kBscHsePropertyIdSm2PrivateKeyId         = 0x0023200D,  ///< Int；默认 12；只读
    kBscHsePropertyIdSm2HashAlgorithm        = 0x00232006,  ///< Int；默认 SM3；可读写
    kBscHsePropertyIdSm2SignInputMode        = 0x00232007,  ///< Int；默认 MESSAGE；可读写
    kBscHsePropertyIdSm2PublicKeyFormat      = 0x0023200A,  ///< Int；默认 BIN(0)；可读写
    kBscHsePropertyIdSm9PublicKeyId          = 0x0023900C,  ///< Int；默认 13；只读
    kBscHsePropertyIdSm9PrivateKeyId         = 0x0023900D,  ///< Int；默认 14；只读
    kBscHsePropertyIdSm9HashAlgorithm        = 0x00239006,  ///< Int；默认 SM3；可读写
    kBscHsePropertyIdSm9SignInputMode        = 0x00239007,  ///< Int；默认 MESSAGE；可读写
    kBscHsePropertyIdSm9Sm9Mode              = 0x00239008,  ///< Int；默认 STREAM；可读写
    kBscHsePropertyIdSm9PublicKeyFormat      = 0x0023900A,  ///< Int；默认 BIN(0)；可读写
};
typedef enum TbscHsePropertyIdTag TbscHsePropertyId;

/* ============ 句柄 ============ */

typedef UInt64 TbscHseHandle;

#define BSC_HSE_HANDLE_SLOT_MASK   0x00000000FFFFFFFFULL
#define BSC_HSE_HANDLE_GEN_SHIFT   32
#define BSC_HSE_HANDLE_GEN_MASK    0x7FFFFFFF00000000ULL
#define BSC_HSE_HANDLE_HIGH_BIT    0x8000000000000000ULL

#define BSC_HSE_HANDLE_IS_VALID(h) \
    (((h) != 0) && (((h) & BSC_HSE_HANDLE_HIGH_BIT) == 0))

#define BSC_HSE_HANDLE_SLOT(h) \
    ((UInt32)(((h) & BSC_HSE_HANDLE_SLOT_MASK) - 1u))

#define BSC_HSE_HANDLE_GEN(h) \
    ((UInt32)(((h) & BSC_HSE_HANDLE_GEN_MASK) >> BSC_HSE_HANDLE_GEN_SHIFT))

#define BSC_HSE_HANDLE_BUILD(gen, slot) \
    ((((UInt64)((UInt32)(gen)) & 0x7FFFFFFFu) << BSC_HSE_HANDLE_GEN_SHIFT) \
     | ((UInt64)((UInt32)((slot) + 1u))))

/* ============ 对象池容量 ============ */

#define BSC_HSE_OBJECT_POOL_SIZE    8 // 由外部配置
#define BSC_HSE_INVALID_SLOT_INDEX  0xFFFFFFFFu

/* ============ USER_ID / IV 长度限制 ============ */

#define BSC_HSE_USER_ID_MAX_LEN     128
#define BSC_HSE_USER_ID_SM2_LEN     16
#define BSC_HSE_AES128_IV_LEN       16
#define BSC_HSE_TDES128_IV_LEN      8
#define BSC_HSE_SM1_IV_LEN          16
#define BSC_HSE_SM4_IV_LEN          16

/* ============ CIU98 固定密钥槽位 ============ */

#define BSC_HSE_RESERVED_KEY_ID_MIN             1
#define BSC_HSE_RESERVED_KEY_ID_MAX             24
#define BSC_HSE_AES128_DEFAULT_KEY_ID            1
#define BSC_HSE_TDES128_DEFAULT_KEY_ID           2
#define BSC_HSE_SM1_DEFAULT_KEY_ID               3
#define BSC_HSE_SM4_DEFAULT_KEY_ID               4
#define BSC_HSE_RSA1024_DEFAULT_PUBLIC_KEY_ID    5
#define BSC_HSE_RSA1024_DEFAULT_PRIVATE_KEY_ID   6
#define BSC_HSE_RSA2048_DEFAULT_PUBLIC_KEY_ID    7
#define BSC_HSE_RSA2048_DEFAULT_PRIVATE_KEY_ID   8
#define BSC_HSE_ECC256_DEFAULT_PUBLIC_KEY_ID     9
#define BSC_HSE_ECC256_DEFAULT_PRIVATE_KEY_ID   10
#define BSC_HSE_SM2_DEFAULT_PUBLIC_KEY_ID        11
#define BSC_HSE_SM2_DEFAULT_PRIVATE_KEY_ID       12
#define BSC_HSE_SM9_DEFAULT_PUBLIC_KEY_ID        13
#define BSC_HSE_SM9_DEFAULT_PRIVATE_KEY_ID       14

/* ============ 输出尺寸约束 ============ */

#define BSC_HSE_MAX_PUBLIC_KEY_LEN  1024
#define BSC_HSE_MAX_SIGNATURE_LEN   512
#define BSC_HSE_MAX_CHIP_ID_LEN     128
#define BSC_HSE_MAX_INPUT_LEN       4096

/* ============ 供应商信息 ============ */

typedef struct TbscHseVendorInfoTag TbscHseVendorInfo;
struct TbscHseVendorInfoTag
{
    const char *vendor_name;
    const char *sdk_version;
    const char *chip_model;
};



/* ============ 内存对象 ============ */

typedef struct TbscHseObjectTag TbscHseObject;
struct TbscHseObjectTag
{
    Bool        used;
    UInt32      generation;

    /* 各算法 KEY_ID / PUBLIC_KEY_ID / PRIVATE_KEY_ID */
    Int32       aes128_key_id;
    Int32       tdes128_key_id;
    Int32       sm1_key_id;
    Int32       sm4_key_id;
    Int32       rsa1024_public_key_id;
    Int32       rsa1024_private_key_id;
    Int32       rsa2048_public_key_id;
    Int32       rsa2048_private_key_id;
    Int32       ecc256_public_key_id;
    Int32       ecc256_private_key_id;
    Int32       sm2_public_key_id;
    Int32       sm2_private_key_id;
    Int32       sm9_public_key_id;
    Int32       sm9_private_key_id;

    /* 各算法模式/补位/输入模式 */
    Int32       aes128_cipher_mode;
    Int32       aes128_padding;
    Int32       tdes128_cipher_mode;
    Int32       tdes128_padding;
    Int32       sm1_cipher_mode;
    Int32       sm1_padding;
    Int32       sm4_cipher_mode;
    Int32       sm4_padding;

    Int32       rsa1024_padding;
    Int32       rsa1024_hash_algorithm;
    Int32       rsa1024_sign_input_mode;
    Int32       rsa1024_public_key_format;
    Int32       rsa2048_padding;
    Int32       rsa2048_hash_algorithm;
    Int32       rsa2048_sign_input_mode;
    Int32       rsa2048_public_key_format;
    Int32       ecc256_hash_algorithm;
    Int32       ecc256_sign_input_mode;
    Int32       ecc256_public_key_format;
    Int32       sm2_hash_algorithm;
    Int32       sm2_sign_input_mode;
    Int32       sm2_public_key_format;
    Int32       sm9_hash_algorithm;
    Int32       sm9_sign_input_mode;
    Int32       sm9_sm9_mode;
    Int32       sm9_public_key_format;

    /* USER_ID + 各算法 IV */
    UInt8       user_id[BSC_HSE_USER_ID_MAX_LEN];
    UInt32      user_id_len;
    UInt8       aes128_iv[BSC_HSE_AES128_IV_LEN];
    Bool        aes128_iv_set;
    UInt8       tdes128_iv[BSC_HSE_TDES128_IV_LEN];
    Bool        tdes128_iv_set;
    UInt8       sm1_iv[BSC_HSE_SM1_IV_LEN];
    Bool        sm1_iv_set;
    UInt8       sm4_iv[BSC_HSE_SM4_IV_LEN];
    Bool        sm4_iv_set;
};


#ifdef __cplusplus
}
#endif

#endif  ///< _BSC_HSE_TYPES_H_
