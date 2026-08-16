/**
 * @file        bsc_hse_sfc_runtime_test.c
 * @brief       BSC HSE SFC runtime 生命周期与串行化测试
 * @note        Linux 主机自动化测试
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#define _DEFAULT_SOURCE

#include "bsc_hse/bsc_hse.h"
#include "bsc_hse/bsc_hse_vendor.h"
#include "bsc_hse_internal.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_THREAD_COUNT 8
#define TEST_LOOP_COUNT 20
#define TEST_DATA_SIZE 16

static atomic_int g_active_calls_ = 0;
static atomic_int g_max_active_calls_ = 0;
static atomic_int g_worker_failures_ = 0;
static atomic_int g_sign_calls_ = 0;
static atomic_int g_import_calls_ = 0;
static TbscHseHandle g_handle_ = 0;
static UInt32 g_sign_private_slot_ = 0;
static UInt32 g_sign_public_slot_ = 0;
static UInt8 g_sign_user_id_[16] = {0};

static Int32 L_VendorInit(void *context)
{
    (void)context;
    return kBscHseResultOk;
}

static Int32 L_VendorDone(void *context)
{
    (void)context;
    return kBscHseResultOk;
}

static Int32 L_VendorCrypt(void *context,
                           const TbscHseVendorCryptParam *param)
{
    Int32 result = kBscHseResultInvalidParam;
    Int32 active = 0;
    Int32 max_active = 0;

    (void)context;
    if (param != NULL && param->input_data != NULL &&
        param->output_data != NULL && param->output_len != NULL &&
        *(param->output_len) >= param->input_len)
    {
        active = atomic_fetch_add(&g_active_calls_, 1) + 1;
        max_active = atomic_load(&g_max_active_calls_);
        while (active > max_active &&
               atomic_compare_exchange_weak(&g_max_active_calls_,
                                             &max_active, active) == 0)
        {
        }
        (void)usleep(1000);
        memcpy(param->output_data, param->input_data, param->input_len);
        *(param->output_len) = param->input_len;
        (void)atomic_fetch_sub(&g_active_calls_, 1);
        result = kBscHseResultOk;
    }
    return result;
}

static Int32 L_VendorSign(void *context,
                          const TbscHseVendorSignParam *param)
{
    (void)context;
    if (param == NULL || param->signature == NULL ||
        param->signature_len == NULL || *(param->signature_len) < 64U ||
        param->user_id == NULL || param->user_id_len != 16U)
    {
        return kBscHseResultInvalidParam;
    }
    g_sign_private_slot_ = param->slot_id;
    g_sign_public_slot_ = param->public_slot_id;
    memcpy(g_sign_user_id_, param->user_id, sizeof(g_sign_user_id_));
    memset(param->signature, 0x5A, 64U);
    *(param->signature_len) = 64U;
    if (param->r_size != NULL)
    {
        *(param->r_size) = 32U;
    }
    if (param->s_size != NULL)
    {
        *(param->s_size) = 32U;
    }
    (void)atomic_fetch_add(&g_sign_calls_, 1);
    return kBscHseResultOk;
}

static Int32 L_VendorExport(void *context, TbscHseVendorKeyParam *param)
{
    (void)context;
    (void)param;
    return kBscHseResultUnsupported;
}

static Int32 L_VendorImport(void *context,
                            const TbscHseVendorKeyParam *param)
{
    (void)context;
    (void)param;
    (void)atomic_fetch_add(&g_import_calls_, 1);
    return kBscHseResultOk;
}

static const TbscHseVendorOps g_vendor_ops_ =
{
    BSC_HSE_VENDOR_ABI_VERSION,
    (UInt32)sizeof(TbscHseVendorOps),
    L_VendorInit,
    L_VendorDone,
    L_VendorCrypt,
    L_VendorCrypt,
    L_VendorSign,
    L_VendorExport,
    L_VendorImport
};

static const TbscHseVendorInfo g_vendor_info_ =
{
    "sfc_runtime_test",
    "1.0",
    "host"
};

static void *L_Worker(void *arg)
{
    UInt8 input[TEST_DATA_SIZE] = {0};
    UInt8 output[TEST_DATA_SIZE] = {0};
    Int32 i = 0;

    (void)arg;
    for (i = 0; i < TEST_LOOP_COUNT; i++)
    {
        UInt32 output_size = (UInt32)sizeof(output);
        Int32 result = BscHseEncrypt(g_handle_,
                                     kBscHseAlgorithmAes128,
                                     input,
                                     (UInt32)sizeof(input),
                                     output,
                                     &output_size);
        if (result != kBscHseResultOk ||
            output_size != (UInt32)sizeof(input))
        {
            (void)atomic_fetch_add(&g_worker_failures_, 1);
            break;
        }
    }
    return NULL;
}

static void L_Expect(const char *name, Int32 actual, Int32 expected,
                     Int32 *failed)
{
    if (actual != expected)
    {
        fprintf(stderr, "[FAIL] %s actual=%d expected=%d\n",
                name, (int)actual, (int)expected);
        (*failed)++;
    }
}

typedef struct TExpectedIntPropertyTag
{
    TbscHsePropertyId property_id;
    Int32 expected_value;
} TExpectedIntProperty;

static void L_CheckDefaultIntProperties(TbscHseHandle handle, Int32 *failed)
{
    static const TExpectedIntProperty expected[] =
    {
        { kBscHsePropertyIdAes128KeyId, 1 },
        { kBscHsePropertyIdAes128CipherMode, kBscHseCipherModeEcb },
        { kBscHsePropertyIdAes128Padding, kBscHsePaddingNone },
        { kBscHsePropertyIdTdes128KeyId, 2 },
        { kBscHsePropertyIdTdes128CipherMode, kBscHseCipherModeEcb },
        { kBscHsePropertyIdTdes128Padding, kBscHsePaddingNone },
        { kBscHsePropertyIdSm1KeyId, 3 },
        { kBscHsePropertyIdSm1CipherMode, kBscHseCipherModeEcb },
        { kBscHsePropertyIdSm1Padding, kBscHsePaddingNone },
        { kBscHsePropertyIdSm4KeyId, 4 },
        { kBscHsePropertyIdSm4CipherMode, kBscHseCipherModeEcb },
        { kBscHsePropertyIdSm4Padding, kBscHsePaddingNone },
        { kBscHsePropertyIdRsa1024PublicKeyId, 5 },
        { kBscHsePropertyIdRsa1024PrivateKeyId, 6 },
        { kBscHsePropertyIdRsa1024Padding, kBscHsePaddingPkcs1 },
        { kBscHsePropertyIdRsa1024HashAlgorithm, kBscHseAlgorithmSha256 },
        { kBscHsePropertyIdRsa1024SignInputMode,
          kBscHseSignInputModeMessage },
        { kBscHsePropertyIdRsa1024PublicKeyFormat, 0 },
        { kBscHsePropertyIdRsa2048PublicKeyId, 7 },
        { kBscHsePropertyIdRsa2048PrivateKeyId, 8 },
        { kBscHsePropertyIdRsa2048Padding, kBscHsePaddingPkcs1 },
        { kBscHsePropertyIdRsa2048HashAlgorithm, kBscHseAlgorithmSha256 },
        { kBscHsePropertyIdRsa2048SignInputMode,
          kBscHseSignInputModeMessage },
        { kBscHsePropertyIdRsa2048PublicKeyFormat, 0 },
        { kBscHsePropertyIdEcc256PublicKeyId, 9 },
        { kBscHsePropertyIdEcc256PrivateKeyId, 10 },
        { kBscHsePropertyIdEcc256HashAlgorithm, kBscHseAlgorithmSha256 },
        { kBscHsePropertyIdEcc256SignInputMode,
          kBscHseSignInputModeMessage },
        { kBscHsePropertyIdEcc256PublicKeyFormat, 0 },
        { kBscHsePropertyIdSm2PublicKeyId, 11 },
        { kBscHsePropertyIdSm2PrivateKeyId, 12 },
        { kBscHsePropertyIdSm2HashAlgorithm, kBscHseAlgorithmSm3 },
        { kBscHsePropertyIdSm2SignInputMode,
          kBscHseSignInputModeMessage },
        { kBscHsePropertyIdSm2PublicKeyFormat, 0 },
        { kBscHsePropertyIdSm9PublicKeyId, 13 },
        { kBscHsePropertyIdSm9PrivateKeyId, 14 },
        { kBscHsePropertyIdSm9HashAlgorithm, kBscHseAlgorithmSm3 },
        { kBscHsePropertyIdSm9SignInputMode,
          kBscHseSignInputModeMessage },
        { kBscHsePropertyIdSm9Sm9Mode, kBscHseSm9ModeStream },
        { kBscHsePropertyIdSm9PublicKeyFormat, 0 }
    };
    UInt32 i;

    for (i = 0; i < (UInt32)(sizeof(expected) / sizeof(expected[0])); i++)
    {
        Int32 actual = -1;
        Int32 result = BscHseGetPropertyInt(
            handle, expected[i].property_id, &actual);
        if (result != kBscHseResultOk ||
            actual != expected[i].expected_value)
        {
            fprintf(stderr,
                    "[FAIL] default %s result=%d actual=%d expected=%d\n",
                    BscHsePropertyIdString(expected[i].property_id),
                    (int)result, (int)actual,
                    (int)expected[i].expected_value);
            (*failed)++;
        }
    }
}

int main(void)
{
    static const UInt8 user_id[16] =
    {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBB, 0xCC, 0xC0,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xBC, 0xCC
    };
    UInt8 sign_input[TEST_DATA_SIZE] = {0};
    UInt8 signature[64] = {0};
    UInt32 signature_size = (UInt32)sizeof(signature);
    UInt32 r_size = 0;
    UInt32 s_size = 0;
    UInt8 property_bytes[16] = {0};
    UInt8 public_key[64] = {0};
    UInt32 property_bytes_size = (UInt32)sizeof(property_bytes);
    Int32 failed = 0;
    Int32 i = 0;
    pthread_t threads[TEST_THREAD_COUNT];
    TbscHseVendorBinding binding =
    {
        &g_vendor_ops_,
        &g_vendor_info_,
        NULL
    };
    TbscHseObject object_pool[BSC_HSE_OBJECT_POOL_SIZE] = {0};

    L_Expect("BSC init",
             BscHseInit(&binding, object_pool,
                        BSC_HSE_OBJECT_POOL_SIZE),
             kBscHseResultOk, &failed);
    L_Expect("BSC duplicate init",
             BscHseInit(&binding, object_pool,
                        BSC_HSE_OBJECT_POOL_SIZE),
             kBscHseResultBusy, &failed);
    L_Expect("create", BscHseCreate(&g_handle_),
             kBscHseResultOk, &failed);
    L_CheckDefaultIntProperties(g_handle_, &failed);
    L_Expect("default USER_ID empty",
             BscHseGetPropertyBytes(g_handle_,
                                    kBscHsePropertyIdUserId,
                                    property_bytes,
                                    &property_bytes_size),
             kBscHseResultOk, &failed);
    L_Expect("default USER_ID length", (Int32)property_bytes_size,
             0, &failed);
    property_bytes_size = (UInt32)sizeof(property_bytes);
    L_Expect("default AES IV unset",
             BscHseGetPropertyBytes(g_handle_,
                                    kBscHsePropertyIdAes128Iv,
                                    property_bytes,
                                    &property_bytes_size),
             kBscHseResultObjectStateError, &failed);
    L_Expect("property algorithm AES128",
             BscHsePropertyAlgorithmOf(kBscHsePropertyIdAes128KeyId),
             kBscHseAlgorithmAes128, &failed);
    L_Expect("property algorithm RSA2048",
             BscHsePropertyAlgorithmOf(
                 kBscHsePropertyIdRsa2048PrivateKeyId),
             kBscHseAlgorithmRsa2048, &failed);
    L_Expect("property algorithm SM2",
             BscHsePropertyAlgorithmOf(kBscHsePropertyIdSm2HashAlgorithm),
             kBscHseAlgorithmSm2, &failed);
    L_Expect("reject readonly AES key slot",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdAes128KeyId, -1),
             kBscHseResultPermissionDenied, &failed);
    L_Expect("reject invalid cipher mode",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdAes128CipherMode, 3),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject asymmetric PKCS7",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdRsa2048Padding,
                                  kBscHsePaddingPkcs7),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject symmetric PKCS1",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm4Padding,
                                  kBscHsePaddingPkcs1),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject non-hash algorithm",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2HashAlgorithm,
                                  kBscHseAlgorithmAes128),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject invalid sign input mode",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2SignInputMode, 2),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject invalid public key format",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2PublicKeyFormat, 1),
             kBscHseResultInvalidParam, &failed);
    L_Expect("reject SM2 public slot update",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2PublicKeyId, 0x0F),
             kBscHseResultPermissionDenied, &failed);
    L_Expect("reject SM2 private slot update",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2PrivateKeyId, 0x10),
             kBscHseResultPermissionDenied, &failed);
    L_Expect("set USER_ID",
             BscHseSetPropertyBytes(g_handle_, kBscHsePropertyIdUserId,
                                    user_id, (UInt32)sizeof(user_id)),
             kBscHseResultOk, &failed);
    L_Expect("SM2 sign",
             BscHseSign(g_handle_, kBscHseAlgorithmSm2,
                        sign_input, (UInt32)sizeof(sign_input),
                        signature, &signature_size, &r_size, &s_size),
             kBscHseResultOk, &failed);
    if (g_sign_private_slot_ != 12U ||
        g_sign_public_slot_ != 11U ||
        memcmp(g_sign_user_id_, user_id, sizeof(user_id)) != 0 ||
        signature_size != 64U || r_size != 32U || s_size != 32U)
    {
        fprintf(stderr,
                "[FAIL] SM2 sign propagation private=%u public=%u "
                "signature=%u r=%u s=%u\n",
                (unsigned int)g_sign_private_slot_,
                (unsigned int)g_sign_public_slot_,
                (unsigned int)signature_size,
                (unsigned int)r_size,
                (unsigned int)s_size);
        failed++;
    }
    L_Expect("set SM2 digest mode",
             BscHseSetPropertyInt(g_handle_,
                                  kBscHsePropertyIdSm2SignInputMode,
                                  kBscHseSignInputModeDigest),
             kBscHseResultOk, &failed);
    signature_size = (UInt32)sizeof(signature);
    L_Expect("reject SM2 digest mode",
             BscHseSign(g_handle_, kBscHseAlgorithmSm2,
                        sign_input, (UInt32)sizeof(sign_input),
                        signature, &signature_size, &r_size, &s_size),
             kBscHseResultUnsupported, &failed);
    if (atomic_load(&g_sign_calls_) != 1)
    {
        fprintf(stderr, "[FAIL] digest mode reached vendor sign\n");
        failed++;
    }
    L_Expect("deny public key import to reserved slot",
             BscHseImportPublicKey(g_handle_,
                                   kBscHseAlgorithmSm2,
                                   public_key,
                                   (UInt32)sizeof(public_key),
                                   False),
             kBscHseResultPermissionDenied, &failed);
    L_Expect("reserved import did not reach vendor",
             atomic_load(&g_import_calls_), 0, &failed);

    if (failed == 0)
    {
        for (i = 0; i < TEST_THREAD_COUNT; i++)
        {
            if (pthread_create(&threads[i], NULL, L_Worker, NULL) != 0)
            {
                failed++;
                break;
            }
        }
        while (i > 0)
        {
            i--;
            (void)pthread_join(threads[i], NULL);
        }
    }

    if (atomic_load(&g_worker_failures_) != 0 ||
        atomic_load(&g_max_active_calls_) != 1)
    {
        fprintf(stderr, "[FAIL] workers=%d max_active=%d\n",
                atomic_load(&g_worker_failures_),
                atomic_load(&g_max_active_calls_));
        failed++;
    }

    L_Expect("destroy", BscHseDestroy(g_handle_),
             kBscHseResultOk, &failed);
    BscHseDone();
    L_Expect("create after done", BscHseCreate(&g_handle_),
             kBscHseResultDeviceUnavailable, &failed);

    if (failed == 0)
    {
        printf("[PASS] runtime lifecycle and max_active_calls=1\n");
    }
    return (failed == 0) ? 0 : 1;
}
