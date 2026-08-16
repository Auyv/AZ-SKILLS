/**
 * @file        bsc_hse_vendor.h
 * @brief       BSC HSE 加密芯片接口定义
 * @note        仅定义芯片能力，不负责线程安全、对象管理或动态库加载
 * @author      HSED
 * @date        2026/07/25
 * @version     4.0
 */

#ifndef _BSC_HSE_VENDOR_PUBLIC_H_
#define _BSC_HSE_VENDOR_PUBLIC_H_

#include "bsc_hse/bsc_hse_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BSC_HSE_VENDOR_ABI_VERSION 3U

typedef struct TbscHseVendorCryptParamTag TbscHseVendorCryptParam;
struct TbscHseVendorCryptParamTag
{
    UInt32              slot_id;
    TbscHseAlgorithm    algorithm;
    TbscHseCipherMode   mode;
    TbscHsePadding      padding;
    const UInt8        *iv;
    UInt32              iv_len;
    const UInt8        *input_data;
    UInt32              input_len;
    UInt8              *output_data;
    UInt32             *output_len;
};

typedef struct TbscHseVendorSignParamTag TbscHseVendorSignParam;
struct TbscHseVendorSignParamTag
{
    UInt32               slot_id;
    UInt32               public_slot_id;
    TbscHseAlgorithm     algorithm;
    TbscHseAlgorithm     hash_algorithm;
    TbscHsePadding       padding;
    TbscHseSignInputMode input_mode;
    const UInt8         *user_id;
    UInt32               user_id_len;
    const UInt8         *input_data;
    UInt32               input_len;
    UInt8               *signature;
    UInt32              *signature_len;
    UInt32              *r_size;
    UInt32              *s_size;
};

typedef struct TbscHseVendorKeyParamTag TbscHseVendorKeyParam;
struct TbscHseVendorKeyParamTag
{
    UInt32              slot_id;
    TbscHseAlgorithm    algorithm;
    const UInt8        *in_key;
    UInt32              in_key_len;
    Bool                replace;
    UInt8              *out_key;
    UInt32             *out_key_len;
};

typedef struct TbscHseVendorGenerateKeyPairParamTag
    TbscHseVendorGenerateKeyPairParam;
struct TbscHseVendorGenerateKeyPairParamTag
{
    UInt32          public_slot_id;
    UInt32          private_slot_id;
    TbscHseAlgorithm algorithm;
    Bool            force_overwrite;
};

typedef struct TbscHseVendorOpsTag TbscHseVendorOps;
struct TbscHseVendorOpsTag
{
    UInt32 abi_version;
    UInt32 struct_size;
    Int32 (*Init)(void *context);
    Int32 (*Done)(void *context);
    Int32 (*Encrypt)(void *context, const TbscHseVendorCryptParam *param);
    Int32 (*Decrypt)(void *context, const TbscHseVendorCryptParam *param);
    Int32 (*Sign)(void *context, const TbscHseVendorSignParam *param);
    Int32 (*ExportPublicKey)(void *context, TbscHseVendorKeyParam *param);
    Int32 (*ImportPublicKey)(void *context,
                             const TbscHseVendorKeyParam *param);
    Int32 (*GenerateKeyPair)(
        void *context,
        const TbscHseVendorGenerateKeyPairParam *param);
};

typedef struct TbscHseVendorBindingTag TbscHseVendorBinding;
struct TbscHseVendorBindingTag
{
    const TbscHseVendorOps  *ops;
    const TbscHseVendorInfo *info;
    void                    *context;
};

/**
 * @brief 获取加密芯片 SDK 接口绑定
 * @return 静态接口绑定；动态库卸载前始终有效
 * @note 每款芯片动态库均按此函数定义实现，RTE 加载后传给 BscHseInit
 */
const TbscHseVendorBinding *BscHseVendorSdkApi(void);

#ifdef __cplusplus
}
#endif

#endif  ///< _BSC_HSE_VENDOR_PUBLIC_H_
