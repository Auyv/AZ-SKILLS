/**
 * @file        bsc_hse.c
 * @brief       BSC HSE 公共组件实现
 *              - 组件级 SFC 临界区保护所有公共入口
 *              - 64 位代际句柄 + 固定对象池
 *              - 11 项密码能力 = 参数校验 + 属性读取 + 调用 vendor ops
 *              - 所有函数遵循单 return 出口
 * @note        不包含任何供应商 SDK 头文件
 * @author      HSED
 * @date        2026/07/21
 * @version     2.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2026/07/20   <td>1.0         <td>靳普诏       <td>初始骨架
 * <tr><td>2026/07/21   <td>2.0         <td>HSED        <td>组件核心逻辑实现
 * <tr><td>2026/07/24   <td>3.0         <td>HSED        <td>平台加载迁移至 RTE
 * <tr><td>2026/07/24   <td>4.0         <td>HSED        <td>互斥迁移至 SFC
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#include "bsc_hse_internal.h"
#include "bsc_hse/bsc_hse_vendor.h"

#include "ofc/ofc_object.h"
#include "sfc/sfc_mutex.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ============ 字符串格式化 ============ */

static const char *g_result_strings_[] = {
    "OK",
    "INVALID_PARAM",
    "UNSUPPORTED",
    "PERMISSION_DENIED",
    "KEY_NOT_FOUND",
    "KEY_OCCUPIED",
    "BUSY",
    "OBJECT_NOT_FOUND",
    "OBJECT_STATE_ERROR",
    "DATA_TOO_LARGE",
    "NO_RESOURCE",
    "TIMEOUT",
    "DEVICE_UNAVAILABLE",
    "AUTH_LOCKED",
    "DEVICE_ERROR",
};

const char *BscHseResultString(Int32 result)
{
    const char *str = "UNKNOWN_RESULT";

    if (result == kBscHseResultOk)
    {
        str = "OK";
    }
    else
    {
        UInt32 idx = (UInt32)(-result);
        if (idx > 0 && idx <= 14)
        {
            str = g_result_strings_[idx];
        }
    }
    return str;
}

const char *BscHseAlgorithmString(TbscHseAlgorithm algorithm)
{
    const char *str = "UNKNOWN_ALG";
    switch (algorithm)
    {
        case kBscHseAlgorithmNone:    str = "NONE";    break;
        case kBscHseAlgorithmAes128:  str = "AES128";  break;
        case kBscHseAlgorithmTdes128: str = "TDES128"; break;
        case kBscHseAlgorithmSm1:     str = "SM1";     break;
        case kBscHseAlgorithmSm4:     str = "SM4";     break;
        case kBscHseAlgorithmRsa1024: str = "RSA1024"; break;
        case kBscHseAlgorithmRsa2048: str = "RSA2048"; break;
        case kBscHseAlgorithmEcc256:  str = "ECC256";  break;
        case kBscHseAlgorithmSm2:     str = "SM2";     break;
        case kBscHseAlgorithmSm9:     str = "SM9";     break;
        case kBscHseAlgorithmSha1:    str = "SHA1";    break;
        case kBscHseAlgorithmSha256:  str = "SHA256";  break;
        case kBscHseAlgorithmSha384:  str = "SHA384";  break;
        case kBscHseAlgorithmSha512:  str = "SHA512";  break;
        case kBscHseAlgorithmSm3:     str = "SM3";     break;
        default:                                      break;
    }
    return str;
}

const char *BscHsePropertyIdString(TbscHsePropertyId property_id)
{
    const char *str = "UNKNOWN";
    switch (property_id)
    {
        case kBscHsePropertyIdUserId:                  str = "USER_ID";                  break;
        case kBscHsePropertyIdChipId:                  str = "CHIP_ID";                  break;
        case kBscHsePropertyIdAes128KeyId:             str = "AES128_KEY_ID";            break;
        case kBscHsePropertyIdAes128CipherMode:        str = "AES128_CIPHER_MODE";       break;
        case kBscHsePropertyIdAes128Padding:           str = "AES128_PADDING";           break;
        case kBscHsePropertyIdAes128Iv:               str = "AES128_IV";                break;
        case kBscHsePropertyIdTdes128KeyId:           str = "TDES128_KEY_ID";           break;
        case kBscHsePropertyIdTdes128CipherMode:      str = "TDES128_CIPHER_MODE";      break;
        case kBscHsePropertyIdTdes128Padding:         str = "TDES128_PADDING";          break;
        case kBscHsePropertyIdTdes128Iv:             str = "TDES128_IV";               break;
        case kBscHsePropertyIdSm1KeyId:               str = "SM1_KEY_ID";                break;
        case kBscHsePropertyIdSm1CipherMode:          str = "SM1_CIPHER_MODE";           break;
        case kBscHsePropertyIdSm1Padding:             str = "SM1_PADDING";               break;
        case kBscHsePropertyIdSm1Iv:                 str = "SM1_IV";                   break;
        case kBscHsePropertyIdSm4KeyId:               str = "SM4_KEY_ID";                break;
        case kBscHsePropertyIdSm4CipherMode:          str = "SM4_CIPHER_MODE";           break;
        case kBscHsePropertyIdSm4Padding:             str = "SM4_PADDING";               break;
        case kBscHsePropertyIdSm4Iv:                 str = "SM4_IV";                   break;
        case kBscHsePropertyIdRsa1024PublicKeyId:     str = "RSA1024_PUBLIC_KEY_ID";     break;
        case kBscHsePropertyIdRsa1024PrivateKeyId:    str = "RSA1024_PRIVATE_KEY_ID";    break;
        case kBscHsePropertyIdRsa1024Padding:         str = "RSA1024_PADDING";           break;
        case kBscHsePropertyIdRsa1024HashAlgorithm:   str = "RSA1024_HASH_ALGORITHM";    break;
        case kBscHsePropertyIdRsa1024SignInputMode:   str = "RSA1024_SIGN_INPUT_MODE";   break;
        case kBscHsePropertyIdRsa1024PublicKeyFormat: str = "RSA1024_PUBLIC_KEY_FORMAT"; break;
        case kBscHsePropertyIdRsa2048PublicKeyId:     str = "RSA2048_PUBLIC_KEY_ID";     break;
        case kBscHsePropertyIdRsa2048PrivateKeyId:    str = "RSA2048_PRIVATE_KEY_ID";    break;
        case kBscHsePropertyIdRsa2048Padding:         str = "RSA2048_PADDING";           break;
        case kBscHsePropertyIdRsa2048HashAlgorithm:   str = "RSA2048_HASH_ALGORITHM";    break;
        case kBscHsePropertyIdRsa2048SignInputMode:   str = "RSA2048_SIGN_INPUT_MODE";   break;
        case kBscHsePropertyIdRsa2048PublicKeyFormat: str = "RSA2048_PUBLIC_KEY_FORMAT"; break;
        case kBscHsePropertyIdEcc256PublicKeyId:      str = "ECC256_PUBLIC_KEY_ID";      break;
        case kBscHsePropertyIdEcc256PrivateKeyId:     str = "ECC256_PRIVATE_KEY_ID";     break;
        case kBscHsePropertyIdEcc256HashAlgorithm:    str = "ECC256_HASH_ALGORITHM";     break;
        case kBscHsePropertyIdEcc256SignInputMode:    str = "ECC256_SIGN_INPUT_MODE";    break;
        case kBscHsePropertyIdEcc256PublicKeyFormat:  str = "ECC256_PUBLIC_KEY_FORMAT";  break;
        case kBscHsePropertyIdSm2PublicKeyId:         str = "SM2_PUBLIC_KEY_ID";         break;
        case kBscHsePropertyIdSm2PrivateKeyId:        str = "SM2_PRIVATE_KEY_ID";        break;
        case kBscHsePropertyIdSm2HashAlgorithm:       str = "SM2_HASH_ALGORITHM";        break;
        case kBscHsePropertyIdSm2SignInputMode:       str = "SM2_SIGN_INPUT_MODE";       break;
        case kBscHsePropertyIdSm2PublicKeyFormat:     str = "SM2_PUBLIC_KEY_FORMAT";     break;
        case kBscHsePropertyIdSm9PublicKeyId:         str = "SM9_PUBLIC_KEY_ID";         break;
        case kBscHsePropertyIdSm9PrivateKeyId:        str = "SM9_PRIVATE_KEY_ID";        break;
        case kBscHsePropertyIdSm9HashAlgorithm:       str = "SM9_HASH_ALGORITHM";        break;
        case kBscHsePropertyIdSm9SignInputMode:       str = "SM9_SIGN_INPUT_MODE";       break;
        case kBscHsePropertyIdSm9Sm9Mode:            str = "SM9_SM9_MODE";              break;
        case kBscHsePropertyIdSm9PublicKeyFormat:     str = "SM9_PUBLIC_KEY_FORMAT";     break;
        default:                                                                         break;
    }
    return str;
}

/* ============ 属性/算法辅助 ============ */

TbscHseAlgorithm BscHsePropertyAlgorithmOf(TbscHsePropertyId property_id)
{
    TbscHseAlgorithm result = kBscHseAlgorithmNone;
    UInt32 alg16 = ((UInt32)property_id >> 8) & 0xFFFFu;

    switch (alg16)
    {
        case 0x1101: result = kBscHseAlgorithmAes128;  break;
        case 0x1231: result = kBscHseAlgorithmTdes128; break;
        case 0x1310: result = kBscHseAlgorithmSm1;     break;
        case 0x1340: result = kBscHseAlgorithmSm4;     break;
        case 0x2401: result = kBscHseAlgorithmRsa1024; break;
        case 0x2402: result = kBscHseAlgorithmRsa2048; break;
        case 0x2501: result = kBscHseAlgorithmEcc256;  break;
        case 0x2320: result = kBscHseAlgorithmSm2;     break;
        case 0x2390: result = kBscHseAlgorithmSm9;     break;
        default:                                       break;
    }
    return result;
}

static Bool L_IsHashAlgorithmValue(Int32 value)
{
    Bool result = False;

    switch ((TbscHseAlgorithm)value)
    {
        case kBscHseAlgorithmNone:
        case kBscHseAlgorithmSha1:
        case kBscHseAlgorithmSha256:
        case kBscHseAlgorithmSha384:
        case kBscHseAlgorithmSha512:
        case kBscHseAlgorithmSm3:
            result = True;
            break;
        default:
            break;
    }
    return result;
}

static Int32 L_ValidatePropertyIntValue(TbscHsePropertyId property_id,
                                        Int32 value)
{
    Int32 result = kBscHseResultOk;
    TbscHseAlgorithm algorithm = BscHsePropertyAlgorithmOf(property_id);
    UInt8 property = (UInt8)((UInt32)property_id & 0xFFu);

    if (BscHsePropertyKeyRole(property_id) != kBscHseKeyRoleInvalid)
    {
        if (value < 0)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x04U)
    {
        if (value != kBscHseCipherModeNone &&
            value != kBscHseCipherModeEcb &&
            value != kBscHseCipherModeCbc)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x05U)
    {
        if ((BscHseAlgorithmIsSymmetric(algorithm) == True &&
             value != kBscHsePaddingNone && value != kBscHsePaddingPkcs7) ||
            ((algorithm == kBscHseAlgorithmRsa1024 ||
              algorithm == kBscHseAlgorithmRsa2048) &&
             value != kBscHsePaddingNone && value != kBscHsePaddingPkcs1) ||
            (BscHseAlgorithmIsSymmetric(algorithm) == False &&
             algorithm != kBscHseAlgorithmRsa1024 &&
             algorithm != kBscHseAlgorithmRsa2048))
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x06U)
    {
        if (L_IsHashAlgorithmValue(value) == False)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x07U)
    {
        if (value != kBscHseSignInputModeMessage &&
            value != kBscHseSignInputModeDigest)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x08U)
    {
        if (value != kBscHseSm9ModeNone && value != kBscHseSm9ModeStream &&
            value != kBscHseSm9ModeSymmetric)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    else if (property == 0x0AU)
    {
        /* CIU98 adapter currently supports only the generated BIN format. */
        if (value != 0)
        {
            result = kBscHseResultInvalidParam;
        }
    }
    return result;
}

TbscHseKeyRole BscHseAlgorithmDefaultRole(TbscHseAlgorithm algorithm)
{
    TbscHseKeyRole result = kBscHseKeyRoleInvalid;

    if (BscHseAlgorithmIsSymmetric(algorithm) == True)
    {
        result = kBscHseKeyRoleSymmetric;
    }
    return result;
}

Bool BscHseAlgorithmIsSymmetric(TbscHseAlgorithm algorithm)
{
    Bool result = False;

    if (algorithm == kBscHseAlgorithmAes128  ||
        algorithm == kBscHseAlgorithmTdes128 ||
        algorithm == kBscHseAlgorithmSm1     ||
        algorithm == kBscHseAlgorithmSm4)
    {
        result = True;
    }
    return result;
}

Bool BscHseAlgorithmIsAsymmetric(TbscHseAlgorithm algorithm)
{
    Bool result = False;

    if (algorithm == kBscHseAlgorithmRsa1024 ||
        algorithm == kBscHseAlgorithmRsa2048 ||
        algorithm == kBscHseAlgorithmEcc256  ||
        algorithm == kBscHseAlgorithmSm2     ||
        algorithm == kBscHseAlgorithmSm9)
    {
        result = True;
    }
    return result;
}

Bool BscHseAlgorithmIsHash(TbscHseAlgorithm algorithm)
{
    Bool result = False;

    if (algorithm == kBscHseAlgorithmSha1   ||
        algorithm == kBscHseAlgorithmSha256 ||
        algorithm == kBscHseAlgorithmSha384 ||
        algorithm == kBscHseAlgorithmSha512 ||
        algorithm == kBscHseAlgorithmSm3)
    {
        result = True;
    }
    return result;
}

UInt32 BscHseAlgorithmSm2DigestLen(void)
{
    return 32;
}

static UInt8 L_PropertyLowByte(TbscHsePropertyId property_id)
{
    return (UInt8)((UInt32)property_id & 0xFFu);
}

TbscHseKeyRole BscHsePropertyKeyRole(TbscHsePropertyId property_id)
{
    TbscHseKeyRole result = kBscHseKeyRoleInvalid;
    TbscHseAlgorithm alg = BscHsePropertyAlgorithmOf(property_id);
    UInt8 pp = L_PropertyLowByte(property_id);

    if (alg != kBscHseAlgorithmNone)
    {
        if (BscHseAlgorithmIsSymmetric(alg) == True)
        {
            if (pp == 0x03U)
            {
                result = kBscHseKeyRoleSymmetric;
            }
        }
        else if (BscHseAlgorithmIsAsymmetric(alg) == True)
        {
            if (pp == 0x0C)
            {
                result = kBscHseKeyRolePublic;
            }
            else if (pp == 0x0D)
            {
                result = kBscHseKeyRolePrivate;
            }
        }
    }
    return result;
}

/* ============ 单例运行态 ============ */

typedef struct TbscHseRuntimeTag TbscHseRuntime;
struct TbscHseRuntimeTag
{
    Bool                 runtime_initialized;
    Bool                 initialized;
    TSfCriticalSection   lock;
    TbscHseObject       *object_pool;
    UInt32               object_pool_count;
};

static TbscHseRuntime       g_runtime_ = {0};
static TbscHseVendorBinding g_vendor_binding_ = { NULL, NULL, NULL };

static Bool L_RuntimeEnter(void)
{
    Bool result = False;

    if (g_runtime_.runtime_initialized == True)
    {
        result = TSfCriticalSectionEnter(&g_runtime_.lock);
    }
    return result;
}

static void L_RuntimeLeave(void)
{
    TSfCriticalSectionLeave(&g_runtime_.lock);
}

/* ============ Vendor 虚接口注入 ============ */

static Bool L_VendorOpsComplete(const TbscHseVendorOps *ops)
{
    Bool result = False;

    if (ops != NULL &&
        ops->Init != NULL &&
        ops->Done != NULL &&
        ops->Encrypt != NULL &&
        ops->Decrypt != NULL &&
        ops->Sign != NULL &&
        ops->ExportPublicKey != NULL &&
        ops->ImportPublicKey != NULL)
    {
        result = True;
    }
    return result;
}

/* ============ 内部：对象池查找 ============ */

static Int32 L_ResolveHandleLocked(TbscHseHandle handle, UInt32 *out_slot)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot;
    UInt32 gen;

    if (BSC_HSE_HANDLE_IS_VALID(handle) == False)
    {
        result = kBscHseResultObjectNotFound;
    }
    else
    {
        slot = BSC_HSE_HANDLE_SLOT(handle);
        gen  = BSC_HSE_HANDLE_GEN(handle);
        if (slot >= g_runtime_.object_pool_count)
        {
            result = kBscHseResultObjectNotFound;
        }
        else if (g_runtime_.object_pool[slot].used == False)
        {
            result = kBscHseResultObjectNotFound;
        }
        else if (g_runtime_.object_pool[slot].generation != gen)
        {
            result = kBscHseResultObjectNotFound;
        }
        else
        {
            *out_slot = slot;
        }
    }
    return result;
}

static void L_InitObjectDefaults(TbscHseObject *obj)
{
    UInt32 generation = obj->generation;

    memset(obj, 0, sizeof(*obj));
    obj->generation = (generation == 0U) ? 1U : generation;
    obj->used = True;

    obj->aes128_key_id = BSC_HSE_AES128_DEFAULT_KEY_ID;
    obj->tdes128_key_id = BSC_HSE_TDES128_DEFAULT_KEY_ID;
    obj->sm1_key_id = BSC_HSE_SM1_DEFAULT_KEY_ID;
    obj->sm4_key_id = BSC_HSE_SM4_DEFAULT_KEY_ID;
    obj->rsa1024_public_key_id =
        BSC_HSE_RSA1024_DEFAULT_PUBLIC_KEY_ID;
    obj->rsa1024_private_key_id =
        BSC_HSE_RSA1024_DEFAULT_PRIVATE_KEY_ID;
    obj->rsa2048_public_key_id =
        BSC_HSE_RSA2048_DEFAULT_PUBLIC_KEY_ID;
    obj->rsa2048_private_key_id =
        BSC_HSE_RSA2048_DEFAULT_PRIVATE_KEY_ID;
    obj->ecc256_public_key_id =
        BSC_HSE_ECC256_DEFAULT_PUBLIC_KEY_ID;
    obj->ecc256_private_key_id =
        BSC_HSE_ECC256_DEFAULT_PRIVATE_KEY_ID;
    obj->sm2_public_key_id = BSC_HSE_SM2_DEFAULT_PUBLIC_KEY_ID;
    obj->sm2_private_key_id = BSC_HSE_SM2_DEFAULT_PRIVATE_KEY_ID;
    obj->sm9_public_key_id = BSC_HSE_SM9_DEFAULT_PUBLIC_KEY_ID;
    obj->sm9_private_key_id = BSC_HSE_SM9_DEFAULT_PRIVATE_KEY_ID;

    obj->aes128_cipher_mode = kBscHseCipherModeEcb;
    obj->tdes128_cipher_mode = kBscHseCipherModeEcb;
    obj->sm1_cipher_mode = kBscHseCipherModeEcb;
    obj->sm4_cipher_mode = kBscHseCipherModeEcb;

    obj->rsa1024_padding = kBscHsePaddingPkcs1;
    obj->rsa1024_hash_algorithm = kBscHseAlgorithmSha256;
    obj->rsa1024_sign_input_mode = kBscHseSignInputModeMessage;
    obj->rsa2048_padding = kBscHsePaddingPkcs1;
    obj->rsa2048_hash_algorithm = kBscHseAlgorithmSha256;
    obj->rsa2048_sign_input_mode = kBscHseSignInputModeMessage;
    obj->ecc256_hash_algorithm = kBscHseAlgorithmSha256;
    obj->ecc256_sign_input_mode = kBscHseSignInputModeMessage;
    obj->sm2_hash_algorithm = kBscHseAlgorithmSm3;
    obj->sm2_sign_input_mode = kBscHseSignInputModeMessage;
    obj->sm9_hash_algorithm = kBscHseAlgorithmSm3;
    obj->sm9_sign_input_mode = kBscHseSignInputModeMessage;
    obj->sm9_sm9_mode = kBscHseSm9ModeStream;
}

/* ============ 内部：属性读写 ============ */

Int32 BscHseObjectReadInt(const TbscHseObject *obj,
                           TbscHsePropertyId property_id,
                           Int32 *value)
{
    Int32 result = kBscHseResultUnsupported;

    if (obj != NULL && value != NULL)
    {
        switch (property_id)
        {
            case kBscHsePropertyIdAes128KeyId:             *value = obj->aes128_key_id;             result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128KeyId:            *value = obj->tdes128_key_id;            result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1KeyId:                *value = obj->sm1_key_id;                result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4KeyId:                *value = obj->sm4_key_id;                result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PublicKeyId:      *value = obj->rsa1024_public_key_id;     result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PrivateKeyId:     *value = obj->rsa1024_private_key_id;    result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PublicKeyId:      *value = obj->rsa2048_public_key_id;     result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PrivateKeyId:     *value = obj->rsa2048_private_key_id;    result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PublicKeyId:       *value = obj->ecc256_public_key_id;      result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PrivateKeyId:      *value = obj->ecc256_private_key_id;     result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PublicKeyId:          *value = obj->sm2_public_key_id;         result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PrivateKeyId:         *value = obj->sm2_private_key_id;        result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PublicKeyId:          *value = obj->sm9_public_key_id;         result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PrivateKeyId:         *value = obj->sm9_private_key_id;        result = kBscHseResultOk; break;
            case kBscHsePropertyIdAes128CipherMode:        *value = obj->aes128_cipher_mode;        result = kBscHseResultOk; break;
            case kBscHsePropertyIdAes128Padding:           *value = obj->aes128_padding;            result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128CipherMode:       *value = obj->tdes128_cipher_mode;       result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128Padding:          *value = obj->tdes128_padding;           result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1CipherMode:           *value = obj->sm1_cipher_mode;           result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1Padding:              *value = obj->sm1_padding;               result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4CipherMode:           *value = obj->sm4_cipher_mode;           result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4Padding:              *value = obj->sm4_padding;               result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024Padding:          *value = obj->rsa1024_padding;           result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024HashAlgorithm:    *value = obj->rsa1024_hash_algorithm;    result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024SignInputMode:    *value = obj->rsa1024_sign_input_mode;   result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PublicKeyFormat:  *value = obj->rsa1024_public_key_format; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048Padding:          *value = obj->rsa2048_padding;           result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048HashAlgorithm:    *value = obj->rsa2048_hash_algorithm;    result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048SignInputMode:    *value = obj->rsa2048_sign_input_mode;   result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PublicKeyFormat:  *value = obj->rsa2048_public_key_format; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256HashAlgorithm:     *value = obj->ecc256_hash_algorithm;     result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256SignInputMode:     *value = obj->ecc256_sign_input_mode;    result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PublicKeyFormat:   *value = obj->ecc256_public_key_format;  result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2HashAlgorithm:        *value = obj->sm2_hash_algorithm;        result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2SignInputMode:        *value = obj->sm2_sign_input_mode;       result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PublicKeyFormat:      *value = obj->sm2_public_key_format;     result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9HashAlgorithm:        *value = obj->sm9_hash_algorithm;        result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9SignInputMode:        *value = obj->sm9_sign_input_mode;       result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9Sm9Mode:             *value = obj->sm9_sm9_mode;              result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PublicKeyFormat:      *value = obj->sm9_public_key_format;     result = kBscHseResultOk; break;
            default:                                       result = kBscHseResultUnsupported;       break;
        }
    }
    else
    {
        result = kBscHseResultInvalidParam;
    }
    return result;
}

Int32 BscHseObjectWriteInt(TbscHseObject *obj,
                            TbscHsePropertyId property_id,
                            Int32 value)
{
    Int32 result = kBscHseResultUnsupported;

    if (obj != NULL)
    {
        switch (property_id)
        {
            case kBscHsePropertyIdAes128KeyId:             obj->aes128_key_id             = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128KeyId:            obj->tdes128_key_id            = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1KeyId:                obj->sm1_key_id                = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4KeyId:                obj->sm4_key_id                = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PublicKeyId:      obj->rsa1024_public_key_id     = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PrivateKeyId:     obj->rsa1024_private_key_id    = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PublicKeyId:      obj->rsa2048_public_key_id     = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PrivateKeyId:     obj->rsa2048_private_key_id    = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PublicKeyId:       obj->ecc256_public_key_id      = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PrivateKeyId:      obj->ecc256_private_key_id     = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PublicKeyId:          obj->sm2_public_key_id         = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PrivateKeyId:         obj->sm2_private_key_id        = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PublicKeyId:          obj->sm9_public_key_id         = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PrivateKeyId:         obj->sm9_private_key_id        = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdAes128CipherMode:        obj->aes128_cipher_mode        = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdAes128Padding:           obj->aes128_padding            = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128CipherMode:       obj->tdes128_cipher_mode       = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdTdes128Padding:          obj->tdes128_padding           = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1CipherMode:           obj->sm1_cipher_mode           = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm1Padding:              obj->sm1_padding               = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4CipherMode:           obj->sm4_cipher_mode           = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm4Padding:              obj->sm4_padding               = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024Padding:          obj->rsa1024_padding           = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024HashAlgorithm:    obj->rsa1024_hash_algorithm    = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024SignInputMode:    obj->rsa1024_sign_input_mode   = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa1024PublicKeyFormat:  obj->rsa1024_public_key_format = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048Padding:          obj->rsa2048_padding           = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048HashAlgorithm:    obj->rsa2048_hash_algorithm    = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048SignInputMode:    obj->rsa2048_sign_input_mode   = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdRsa2048PublicKeyFormat:  obj->rsa2048_public_key_format = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256HashAlgorithm:     obj->ecc256_hash_algorithm     = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256SignInputMode:     obj->ecc256_sign_input_mode    = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdEcc256PublicKeyFormat:   obj->ecc256_public_key_format  = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2HashAlgorithm:        obj->sm2_hash_algorithm        = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2SignInputMode:        obj->sm2_sign_input_mode       = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm2PublicKeyFormat:      obj->sm2_public_key_format     = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9HashAlgorithm:        obj->sm9_hash_algorithm        = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9SignInputMode:        obj->sm9_sign_input_mode       = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9Sm9Mode:             obj->sm9_sm9_mode              = value; result = kBscHseResultOk; break;
            case kBscHsePropertyIdSm9PublicKeyFormat:      obj->sm9_public_key_format     = value; result = kBscHseResultOk; break;
            default:                                       result = kBscHseResultUnsupported;         break;
        }
    }
    else
    {
        result = kBscHseResultInvalidParam;
    }
    return result;
}

/* ============ Init / Done ============ */

Int32 BscHseInit(const TbscHseVendorBinding *binding,
                 TbscHseObject *object_pool,
                 UInt32 object_pool_count)
{
    Int32 result = kBscHseResultInvalidParam;
    Bool entered = False;
    UInt32 i;

    if (g_runtime_.runtime_initialized == True)
    {
        result = kBscHseResultBusy;
    }
    else if (binding == NULL || binding->ops == NULL ||
             object_pool == NULL || object_pool_count == 0U)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (binding->ops->abi_version != BSC_HSE_VENDOR_ABI_VERSION ||
             binding->ops->struct_size != (UInt32)sizeof(TbscHseVendorOps))
    {
        result = kBscHseResultUnsupported;
    }
    else if (L_VendorOpsComplete(binding->ops) == False)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (TSfCriticalSectionCreate(&g_runtime_.lock) == False)
    {
        memset(&g_runtime_, 0, sizeof(g_runtime_));
        result = kBscHseResultNoResource;
    }
    else
    {
        g_runtime_.runtime_initialized = True;
        g_runtime_.object_pool = object_pool;
        g_runtime_.object_pool_count = object_pool_count;
        g_vendor_binding_ = *binding;
        entered = L_RuntimeEnter();
        if (entered == True)
        {
            result = g_vendor_binding_.ops->Init(g_vendor_binding_.context);
            if (result == kBscHseResultOk)
            {
                memset(g_runtime_.object_pool, 0,
                       sizeof(TbscHseObject) *
                       g_runtime_.object_pool_count);
                for (i = 0; i < g_runtime_.object_pool_count; i++)
                {
                    g_runtime_.object_pool[i].generation = 1;
                }
                g_runtime_.initialized = True;
            }
            L_RuntimeLeave();
        }
        else
        {
            result = kBscHseResultDeviceUnavailable;
        }
        if (result != kBscHseResultOk)
        {
            OfDestroyObject((TOfObject *)&g_runtime_.lock);
            memset(&g_runtime_, 0, sizeof(g_runtime_));
            memset(&g_vendor_binding_, 0, sizeof(g_vendor_binding_));
        }
    }
    return result;
}

void BscHseDone(void)
{
    if (L_RuntimeEnter() == True)
    {
        if (g_runtime_.initialized == True)
        {
            if (g_vendor_binding_.ops != NULL)
            {
                (void)g_vendor_binding_.ops->Done(g_vendor_binding_.context);
            }
            memset(g_runtime_.object_pool, 0,
                   sizeof(TbscHseObject) * g_runtime_.object_pool_count);
            g_runtime_.initialized = False;
        }
        L_RuntimeLeave();
        OfDestroyObject((TOfObject *)&g_runtime_.lock);
        memset(&g_runtime_, 0, sizeof(g_runtime_));
        memset(&g_vendor_binding_, 0, sizeof(g_vendor_binding_));
    }
}

/* ============ Create / Destroy ============ */

Int32 BscHseCreate(TbscHseHandle *handle)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;
    UInt32 i;
    TbscHseObject *obj = NULL;

    if (handle == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        *handle = 0;
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            if (g_runtime_.initialized == False)
            {
                result = kBscHseResultDeviceUnavailable;
            }
            else
            {
                for (i = 0; i < g_runtime_.object_pool_count; i++)
                {
                    if (g_runtime_.object_pool[i].used == False)
                    {
                        slot = i;
                        break;
                    }
                }
                if (slot == BSC_HSE_INVALID_SLOT_INDEX)
                {
                    result = kBscHseResultNoResource;
                }
                else
                {
                    obj = &g_runtime_.object_pool[slot];
                    L_InitObjectDefaults(obj);
                    *handle = BSC_HSE_HANDLE_BUILD(obj->generation, slot);
                }
            }
            L_RuntimeLeave();
        }
    }
    return result;
}

Int32 BscHseDestroy(TbscHseHandle handle)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (L_RuntimeEnter() == False)
    {
        result = kBscHseResultDeviceUnavailable;
    }
    else
    {
        if (g_runtime_.initialized == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                UInt32 new_gen = obj->generation + 1u;
                if (new_gen == 0)
                {
                    new_gen = 1;
                }
                memset(obj, 0, sizeof(*obj));
                obj->generation = new_gen;
            }
        }
        L_RuntimeLeave();
    }
    return result;
}

/* ============ Set/Get 属性 ============ */

Int32 BscHseSetPropertyInt(TbscHseHandle handle,
                            TbscHsePropertyId property_id,
                            Int32 value)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (BscHsePropertyKeyRole(property_id) != kBscHseKeyRoleInvalid)
    {
        result = kBscHseResultPermissionDenied;
    }
    else if (property_id == kBscHsePropertyIdUserId ||
        property_id == kBscHsePropertyIdChipId ||
        property_id == kBscHsePropertyIdAes128Iv ||
        property_id == kBscHsePropertyIdTdes128Iv ||
        property_id == kBscHsePropertyIdSm1Iv ||
        property_id == kBscHsePropertyIdSm4Iv)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (L_ValidatePropertyIntValue(property_id, value) !=
             kBscHseResultOk)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            if (g_runtime_.initialized == False)
            {
                result = kBscHseResultDeviceUnavailable;
            }
            else
            {
                result = L_ResolveHandleLocked(handle, &slot);
                if (result == kBscHseResultOk)
                {
                    result = BscHseObjectWriteInt(&g_runtime_.object_pool[slot],
                                                   property_id, value);
                }
            }
            L_RuntimeLeave();
        }
    }
    return result;
}

Int32 BscHseGetPropertyInt(TbscHseHandle handle,
                            TbscHsePropertyId property_id,
                            Int32 *value)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (value == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (property_id == kBscHsePropertyIdUserId ||
             property_id == kBscHsePropertyIdChipId ||
             property_id == kBscHsePropertyIdAes128Iv ||
             property_id == kBscHsePropertyIdTdes128Iv ||
             property_id == kBscHsePropertyIdSm1Iv ||
             property_id == kBscHsePropertyIdSm4Iv)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            if (g_runtime_.initialized == False)
            {
                result = kBscHseResultDeviceUnavailable;
            }
            else
            {
                result = L_ResolveHandleLocked(handle, &slot);
                if (result == kBscHseResultOk)
                {
                    result = BscHseObjectReadInt(&g_runtime_.object_pool[slot],
                                                  property_id, value);
                }
            }
            L_RuntimeLeave();
        }
    }
    return result;
}

Int32 BscHseSetPropertyBytes(TbscHseHandle handle,
                              TbscHsePropertyId property_id,
                              const UInt8 *value,
                              UInt32 value_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (value == NULL && value_size != 0)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                switch (property_id)
                {
                    case kBscHsePropertyIdUserId:
                        if (value_size > BSC_HSE_USER_ID_MAX_LEN)
                        {
                            result = kBscHseResultDataTooLarge;
                        }
                        else
                        {
                            memset(obj->user_id, 0, sizeof(obj->user_id));
                            if (value_size > 0)
                            {
                                memcpy(obj->user_id, value, value_size);
                            }
                            obj->user_id_len = value_size;
                        }
                        break;
                    case kBscHsePropertyIdChipId:
                        result = kBscHseResultUnsupported;
                        break;
                    case kBscHsePropertyIdAes128Iv:
                        if (value_size != BSC_HSE_AES128_IV_LEN)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            memcpy(obj->aes128_iv, value, BSC_HSE_AES128_IV_LEN);
                            obj->aes128_iv_set = True;
                        }
                        break;
                    case kBscHsePropertyIdTdes128Iv:
                        if (value_size != BSC_HSE_TDES128_IV_LEN)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            memcpy(obj->tdes128_iv, value, BSC_HSE_TDES128_IV_LEN);
                            obj->tdes128_iv_set = True;
                        }
                        break;
                    case kBscHsePropertyIdSm1Iv:
                        if (value_size != BSC_HSE_SM1_IV_LEN)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            memcpy(obj->sm1_iv, value, BSC_HSE_SM1_IV_LEN);
                            obj->sm1_iv_set = True;
                        }
                        break;
                    case kBscHsePropertyIdSm4Iv:
                        if (value_size != BSC_HSE_SM4_IV_LEN)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            memcpy(obj->sm4_iv, value, BSC_HSE_SM4_IV_LEN);
                            obj->sm4_iv_set = True;
                        }
                        break;
                    default:
                        if (BscHseObjectReadInt(obj, property_id,
                                                 &(Int32){0}) == kBscHseResultOk)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            result = kBscHseResultUnsupported;
                        }
                        break;
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

Int32 BscHseGetPropertyBytes(TbscHseHandle handle,
                              TbscHsePropertyId property_id,
                              UInt8 *value,
                              UInt32 *value_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;
    UInt32 actual_len = 0;
    const UInt8 *src = NULL;

    if (value == NULL || value_size == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                switch (property_id)
                {
                    case kBscHsePropertyIdUserId:
                        actual_len = obj->user_id_len;
                        src = obj->user_id;
                        break;
                    case kBscHsePropertyIdAes128Iv:
                        if (obj->aes128_iv_set == False)
                        {
                            result = kBscHseResultObjectStateError;
                        }
                        else
                        {
                            actual_len = BSC_HSE_AES128_IV_LEN;
                            src = obj->aes128_iv;
                        }
                        break;
                    case kBscHsePropertyIdTdes128Iv:
                        if (obj->tdes128_iv_set == False)
                        {
                            result = kBscHseResultObjectStateError;
                        }
                        else
                        {
                            actual_len = BSC_HSE_TDES128_IV_LEN;
                            src = obj->tdes128_iv;
                        }
                        break;
                    case kBscHsePropertyIdSm1Iv:
                        if (obj->sm1_iv_set == False)
                        {
                            result = kBscHseResultObjectStateError;
                        }
                        else
                        {
                            actual_len = BSC_HSE_SM1_IV_LEN;
                            src = obj->sm1_iv;
                        }
                        break;
                    case kBscHsePropertyIdSm4Iv:
                        if (obj->sm4_iv_set == False)
                        {
                            result = kBscHseResultObjectStateError;
                        }
                        else
                        {
                            actual_len = BSC_HSE_SM4_IV_LEN;
                            src = obj->sm4_iv;
                        }
                        break;
                    case kBscHsePropertyIdChipId:
                        result = kBscHseResultUnsupported;
                        break;
                    default:
                        result = kBscHseResultUnsupported;
                        break;
                }
            }
            if (result == kBscHseResultOk && src != NULL)
            {
                if (*value_size < actual_len)
                {
                    *value_size = actual_len;
                    result = kBscHseResultDataTooLarge;
                }
                else
                {
                    memcpy(value, src, actual_len);
                    *value_size = actual_len;
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

/* ============ 辅助：属性读取简写（内部使用） ============ */

static Int32 L_GetPropChecked(TbscHseObject *obj,
                               TbscHsePropertyId pid,
                               Int32 *out)
{
    Int32 r = BscHseObjectReadInt(obj, pid, out);

    if (r == kBscHseResultUnsupported)
    {
        *out = 0;
        r = kBscHseResultOk;
    }
    return r;
}

static void L_GetIvByAlgorithm(TbscHseObject *obj,
                                TbscHseAlgorithm algorithm,
                                Bool *out_has_iv,
                                UInt8 *out_iv,
                                UInt32 expected_len)
{
    *out_has_iv = False;
    switch (algorithm)
    {
        case kBscHseAlgorithmAes128:
            *out_has_iv = obj->aes128_iv_set;
            memcpy(out_iv, obj->aes128_iv, BSC_HSE_AES128_IV_LEN);
            break;
        case kBscHseAlgorithmTdes128:
            *out_has_iv = obj->tdes128_iv_set;
            memcpy(out_iv, obj->tdes128_iv, BSC_HSE_TDES128_IV_LEN);
            break;
        case kBscHseAlgorithmSm1:
            *out_has_iv = obj->sm1_iv_set;
            memcpy(out_iv, obj->sm1_iv, BSC_HSE_SM1_IV_LEN);
            break;
        case kBscHseAlgorithmSm4:
            *out_has_iv = obj->sm4_iv_set;
            memcpy(out_iv, obj->sm4_iv, BSC_HSE_SM4_IV_LEN);
            break;
        default:
            break;
    }
    (void)expected_len;
}

/* ============ Encrypt ============ */

Int32 BscHseEncrypt(TbscHseHandle handle,
                     TbscHseAlgorithm algorithm,
                     const UInt8 *input_data,
                     UInt32 input_size,
                     UInt8 *output_data,
                     UInt32 *output_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;
    Bool is_sym = BscHseAlgorithmIsSymmetric(algorithm);

    if (input_data == NULL || output_data == NULL || output_size == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (BscHseAlgorithmIsHash(algorithm) == True ||
             algorithm == kBscHseAlgorithmNone)
    {
        result = kBscHseResultUnsupported;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False || g_vendor_binding_.ops == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                TbscHseVendorCryptParam param;
                TbscHsePropertyId kid_pid = kBscHsePropertyIdUserId;
                TbscHsePropertyId mode_pid = kBscHsePropertyIdUserId;
                TbscHsePropertyId pad_pid = kBscHsePropertyIdUserId;
                UInt32 expected_iv_len = 0;

                memset(&param, 0, sizeof(param));
                if (is_sym == True)
                {
                    switch (algorithm)
                    {
                        case kBscHseAlgorithmAes128:
                            kid_pid = kBscHsePropertyIdAes128KeyId;
                            mode_pid = kBscHsePropertyIdAes128CipherMode;
                            pad_pid = kBscHsePropertyIdAes128Padding;
                            expected_iv_len = BSC_HSE_AES128_IV_LEN;
                            break;
                        case kBscHseAlgorithmTdes128:
                            kid_pid = kBscHsePropertyIdTdes128KeyId;
                            mode_pid = kBscHsePropertyIdTdes128CipherMode;
                            pad_pid = kBscHsePropertyIdTdes128Padding;
                            expected_iv_len = BSC_HSE_TDES128_IV_LEN;
                            break;
                        case kBscHseAlgorithmSm1:
                            kid_pid = kBscHsePropertyIdSm1KeyId;
                            mode_pid = kBscHsePropertyIdSm1CipherMode;
                            pad_pid = kBscHsePropertyIdSm1Padding;
                            expected_iv_len = BSC_HSE_SM1_IV_LEN;
                            break;
                        case kBscHseAlgorithmSm4:
                            kid_pid = kBscHsePropertyIdSm4KeyId;
                            mode_pid = kBscHsePropertyIdSm4CipherMode;
                            pad_pid = kBscHsePropertyIdSm4Padding;
                            expected_iv_len = BSC_HSE_SM4_IV_LEN;
                            break;
                        default:
                            result = kBscHseResultUnsupported;
                            break;
                    }
                }
                else
                {
                    switch (algorithm)
                    {
                        case kBscHseAlgorithmRsa1024:
                            kid_pid = kBscHsePropertyIdRsa1024PublicKeyId;
                            pad_pid = kBscHsePropertyIdRsa1024Padding;
                            break;
                        case kBscHseAlgorithmRsa2048:
                            kid_pid = kBscHsePropertyIdRsa2048PublicKeyId;
                            pad_pid = kBscHsePropertyIdRsa2048Padding;
                            break;
                        case kBscHseAlgorithmEcc256:
                            kid_pid = kBscHsePropertyIdEcc256PublicKeyId;
                            break;
                        case kBscHseAlgorithmSm2:
                            kid_pid = kBscHsePropertyIdSm2PublicKeyId;
                            break;
                        case kBscHseAlgorithmSm9:
                            kid_pid = kBscHsePropertyIdSm9PublicKeyId;
                            break;
                        default:
                            result = kBscHseResultUnsupported;
                            break;
                    }
                }
                if (result == kBscHseResultOk)
                {
                    Int32 kid = 0;
                    Int32 mode_val = kBscHseCipherModeEcb;
                    Int32 pad_val = kBscHsePaddingNone;
                    Bool has_iv = False;
                    UInt8 iv_buf[BSC_HSE_SM4_IV_LEN];

                    L_GetPropChecked(obj, kid_pid, &kid);
                    L_GetPropChecked(obj, mode_pid, &mode_val);
                    L_GetPropChecked(obj, pad_pid, &pad_val);
                    if (is_sym == True &&
                        (TbscHseCipherMode)mode_val == kBscHseCipherModeCbc)
                    {
                        L_GetIvByAlgorithm(obj, algorithm, &has_iv, iv_buf,
                                          expected_iv_len);
                        if (has_iv == False)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                    }
                    if (result == kBscHseResultOk)
                    {
                        param.slot_id    = (UInt32)kid;
                        param.algorithm  = algorithm;
                        param.mode       = (TbscHseCipherMode)mode_val;
                        param.padding    = (TbscHsePadding)pad_val;
                        param.iv         = has_iv ? iv_buf : NULL;
                        param.iv_len     = has_iv ? expected_iv_len : 0;
                        param.input_data = input_data;
                        param.input_len  = input_size;
                        param.output_data = output_data;
                        param.output_len  = output_size;
                        result = g_vendor_binding_.ops->Encrypt(
                            g_vendor_binding_.context, &param);
                    }
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

/* ============ Decrypt ============ */

Int32 BscHseDecrypt(TbscHseHandle handle,
                     TbscHseAlgorithm algorithm,
                     const UInt8 *input_data,
                     UInt32 input_size,
                     UInt8 *output_data,
                     UInt32 *output_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;
    Bool is_sym = BscHseAlgorithmIsSymmetric(algorithm);

    if (input_data == NULL || output_data == NULL || output_size == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (BscHseAlgorithmIsHash(algorithm) == True ||
             algorithm == kBscHseAlgorithmNone)
    {
        result = kBscHseResultUnsupported;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False || g_vendor_binding_.ops == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                TbscHseVendorCryptParam param;
                TbscHsePropertyId kid_pid = kBscHsePropertyIdUserId;
                TbscHsePropertyId mode_pid = kBscHsePropertyIdUserId;
                TbscHsePropertyId pad_pid = kBscHsePropertyIdUserId;
                UInt32 expected_iv_len = 0;

                memset(&param, 0, sizeof(param));
                if (is_sym == True)
                {
                    switch (algorithm)
                    {
                        case kBscHseAlgorithmAes128:
                            kid_pid = kBscHsePropertyIdAes128KeyId;
                            mode_pid = kBscHsePropertyIdAes128CipherMode;
                            pad_pid = kBscHsePropertyIdAes128Padding;
                            expected_iv_len = BSC_HSE_AES128_IV_LEN;
                            break;
                        case kBscHseAlgorithmTdes128:
                            kid_pid = kBscHsePropertyIdTdes128KeyId;
                            mode_pid = kBscHsePropertyIdTdes128CipherMode;
                            pad_pid = kBscHsePropertyIdTdes128Padding;
                            expected_iv_len = BSC_HSE_TDES128_IV_LEN;
                            break;
                        case kBscHseAlgorithmSm1:
                            kid_pid = kBscHsePropertyIdSm1KeyId;
                            mode_pid = kBscHsePropertyIdSm1CipherMode;
                            pad_pid = kBscHsePropertyIdSm1Padding;
                            expected_iv_len = BSC_HSE_SM1_IV_LEN;
                            break;
                        case kBscHseAlgorithmSm4:
                            kid_pid = kBscHsePropertyIdSm4KeyId;
                            mode_pid = kBscHsePropertyIdSm4CipherMode;
                            pad_pid = kBscHsePropertyIdSm4Padding;
                            expected_iv_len = BSC_HSE_SM4_IV_LEN;
                            break;
                        default:
                            result = kBscHseResultUnsupported;
                            break;
                    }
                }
                else
                {
                    switch (algorithm)
                    {
                        case kBscHseAlgorithmRsa1024:
                            kid_pid = kBscHsePropertyIdRsa1024PrivateKeyId;
                            pad_pid = kBscHsePropertyIdRsa1024Padding;
                            break;
                        case kBscHseAlgorithmRsa2048:
                            kid_pid = kBscHsePropertyIdRsa2048PrivateKeyId;
                            pad_pid = kBscHsePropertyIdRsa2048Padding;
                            break;
                        case kBscHseAlgorithmEcc256:
                            kid_pid = kBscHsePropertyIdEcc256PrivateKeyId;
                            break;
                        case kBscHseAlgorithmSm2:
                            kid_pid = kBscHsePropertyIdSm2PrivateKeyId;
                            break;
                        case kBscHseAlgorithmSm9:
                            kid_pid = kBscHsePropertyIdSm9PrivateKeyId;
                            break;
                        default:
                            result = kBscHseResultUnsupported;
                            break;
                    }
                }
                if (result == kBscHseResultOk)
                {
                    Int32 kid = 0;
                    Int32 mode_val = kBscHseCipherModeEcb;
                    Int32 pad_val = kBscHsePaddingNone;
                    Bool has_iv = False;
                    UInt8 iv_buf[BSC_HSE_SM4_IV_LEN];

                    L_GetPropChecked(obj, kid_pid, &kid);
                    L_GetPropChecked(obj, mode_pid, &mode_val);
                    L_GetPropChecked(obj, pad_pid, &pad_val);
                    if (is_sym == True &&
                        (TbscHseCipherMode)mode_val == kBscHseCipherModeCbc)
                    {
                        L_GetIvByAlgorithm(obj, algorithm, &has_iv, iv_buf,
                                          expected_iv_len);
                        if (has_iv == False)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                    }
                    if (result == kBscHseResultOk)
                    {
                        param.slot_id    = (UInt32)kid;
                        param.algorithm  = algorithm;
                        param.mode       = (TbscHseCipherMode)mode_val;
                        param.padding    = (TbscHsePadding)pad_val;
                        param.iv         = has_iv ? iv_buf : NULL;
                        param.iv_len     = has_iv ? expected_iv_len : 0;
                        param.input_data = input_data;
                        param.input_len  = input_size;
                        param.output_data = output_data;
                        param.output_len  = output_size;
                        result = g_vendor_binding_.ops->Decrypt(
                            g_vendor_binding_.context, &param);
                    }
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

/* ============ Sign ============ */

Int32 BscHseSign(TbscHseHandle handle,
                  TbscHseAlgorithm algorithm,
                  const UInt8 *input_data,
                  UInt32 input_size,
                  UInt8 *signature,
                  UInt32 *signature_size,
                  UInt32 *r_size,
                  UInt32 *s_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (BscHseAlgorithmIsAsymmetric(algorithm) == False)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (input_data == NULL || signature == NULL ||
             signature_size == NULL || r_size == NULL || s_size == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False || g_vendor_binding_.ops == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                TbscHseVendorSignParam param;
                TbscHsePropertyId kid_pid;
                TbscHsePropertyId public_kid_pid;
                TbscHsePropertyId hash_pid;
                TbscHsePropertyId mode_pid;
                Int32 kid = 0;
                Int32 public_kid = 0;
                Int32 hash_val = kBscHseAlgorithmSm3;
                Int32 mode_val = kBscHseSignInputModeMessage;
                UInt8 user_id_16[BSC_HSE_USER_ID_SM2_LEN] = {0};
                UInt32 user_id_len_16 = 0;

                memset(&param, 0, sizeof(param));
                switch (algorithm)
                {
                    case kBscHseAlgorithmRsa1024:
                        kid_pid  = kBscHsePropertyIdRsa1024PrivateKeyId;
                        hash_pid = kBscHsePropertyIdRsa1024HashAlgorithm;
                        mode_pid = kBscHsePropertyIdRsa1024SignInputMode;
                        break;
                    case kBscHseAlgorithmRsa2048:
                        kid_pid  = kBscHsePropertyIdRsa2048PrivateKeyId;
                        hash_pid = kBscHsePropertyIdRsa2048HashAlgorithm;
                        mode_pid = kBscHsePropertyIdRsa2048SignInputMode;
                        break;
                    case kBscHseAlgorithmEcc256:
                        kid_pid  = kBscHsePropertyIdEcc256PrivateKeyId;
                        hash_pid = kBscHsePropertyIdEcc256HashAlgorithm;
                        mode_pid = kBscHsePropertyIdEcc256SignInputMode;
                        break;
                    case kBscHseAlgorithmSm2:
                        kid_pid  = kBscHsePropertyIdSm2PrivateKeyId;
                        public_kid_pid = kBscHsePropertyIdSm2PublicKeyId;
                        hash_pid = kBscHsePropertyIdSm2HashAlgorithm;
                        mode_pid = kBscHsePropertyIdSm2SignInputMode;
                        break;
                    case kBscHseAlgorithmSm9:
                        kid_pid  = kBscHsePropertyIdSm9PrivateKeyId;
                        hash_pid = kBscHsePropertyIdSm9HashAlgorithm;
                        mode_pid = kBscHsePropertyIdSm9SignInputMode;
                        break;
                    default:
                        result = kBscHseResultUnsupported;
                        break;
                }
                if (result == kBscHseResultOk)
                {
                    L_GetPropChecked(obj, kid_pid, &kid);
                    L_GetPropChecked(obj, hash_pid, &hash_val);
                    L_GetPropChecked(obj, mode_pid, &mode_val);
                    if (algorithm == kBscHseAlgorithmSm2)
                    {
                        L_GetPropChecked(obj, public_kid_pid, &public_kid);
                        if (mode_val == kBscHseSignInputModeDigest)
                        {
                            result = kBscHseResultUnsupported;
                        }
                        else if (hash_val != kBscHseAlgorithmSm3)
                        {
                            result = kBscHseResultUnsupported;
                        }
                        else if (obj->user_id_len < BSC_HSE_USER_ID_SM2_LEN)
                        {
                            result = kBscHseResultInvalidParam;
                        }
                        else
                        {
                            memcpy(user_id_16, obj->user_id,
                                   BSC_HSE_USER_ID_SM2_LEN);
                            user_id_len_16 = BSC_HSE_USER_ID_SM2_LEN;
                        }
                    }
                    if (result == kBscHseResultOk)
                    {
                        param.slot_id        = (UInt32)kid;
                        param.public_slot_id = (UInt32)public_kid;
                        param.algorithm      = algorithm;
                        param.hash_algorithm = (TbscHseAlgorithm)hash_val;
                        param.padding        = kBscHsePaddingPkcs1;
                        param.input_mode     = (TbscHseSignInputMode)mode_val;
                        param.user_id        =
                            (algorithm == kBscHseAlgorithmSm2) ?
                            user_id_16 : NULL;
                        param.user_id_len    = user_id_len_16;
                        param.input_data     = input_data;
                        param.input_len      = input_size;
                        param.signature      = signature;
                        param.signature_len  = signature_size;
                        param.r_size         = r_size;
                        param.s_size         = s_size;
                        result = g_vendor_binding_.ops->Sign(
                            g_vendor_binding_.context, &param);
                    }
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

/* ============ Export/Import PublicKey ============ */

Int32 BscHseExportPublicKey(TbscHseHandle handle,
                             TbscHseAlgorithm algorithm,
                             UInt8 *public_key,
                             UInt32 *public_key_size)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    if (BscHseAlgorithmIsAsymmetric(algorithm) == False)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (public_key == NULL || public_key_size == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False || g_vendor_binding_.ops == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                TbscHseObject *obj = &g_runtime_.object_pool[slot];
                TbscHseVendorKeyParam param;
                TbscHsePropertyId kid_pid = kBscHsePropertyIdUserId;
                Int32 kid = 0;

                memset(&param, 0, sizeof(param));
                switch (algorithm)
                {
                    case kBscHseAlgorithmRsa1024: kid_pid = kBscHsePropertyIdRsa1024PublicKeyId; break;
                    case kBscHseAlgorithmRsa2048: kid_pid = kBscHsePropertyIdRsa2048PublicKeyId; break;
                    case kBscHseAlgorithmEcc256:  kid_pid = kBscHsePropertyIdEcc256PublicKeyId;  break;
                    case kBscHseAlgorithmSm2:     kid_pid = kBscHsePropertyIdSm2PublicKeyId;     break;
                    case kBscHseAlgorithmSm9:     kid_pid = kBscHsePropertyIdSm9PublicKeyId;     break;
                    default: result = kBscHseResultUnsupported; break;
                }
                if (result == kBscHseResultOk)
                {
                    L_GetPropChecked(obj, kid_pid, &kid);
                    param.slot_id      = (UInt32)kid;
                    param.algorithm    = algorithm;
                    param.out_key      = public_key;
                    param.out_key_len  = public_key_size;
                    result = g_vendor_binding_.ops->ExportPublicKey(
                        g_vendor_binding_.context, &param);
                }
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}

Int32 BscHseImportPublicKey(TbscHseHandle handle,
                             TbscHseAlgorithm algorithm,
                             const UInt8 *public_key,
                             UInt32 public_key_size,
                             Bool replace_existing_key)
{
    Int32 result = kBscHseResultOk;
    UInt32 slot = BSC_HSE_INVALID_SLOT_INDEX;

    (void)replace_existing_key;
    if (BscHseAlgorithmIsAsymmetric(algorithm) == False)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (public_key == NULL || public_key_size == 0)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (L_RuntimeEnter() == False)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
        if (g_runtime_.initialized == False || g_vendor_binding_.ops == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            result = L_ResolveHandleLocked(handle, &slot);
            if (result == kBscHseResultOk)
            {
                result = kBscHseResultPermissionDenied;
            }
        }
            L_RuntimeLeave();
        }
    }
    return result;
}
