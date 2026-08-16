/**
 * @file        ciu98_za_adapter_test.c
 * @brief       CIU98 SM2 ZA 签名适配单元测试
 * @author      HSED
 * @date        2026/07/26
 * @version     1.0
 */

#include "bsc_hse/bsc_hse_vendor.h"

#include "comm.h"
#include "crypto.h"
#include "error.h"
#include "info.h"
#include "key.h"
#include "alog/a_log.h"

#include <stdio.h>
#include <string.h>

#define TEST_PUBLIC_SLOT_ID  11U
#define TEST_PRIVATE_SLOT_ID 12U
#define TEST_USER_ID_LEN     16U
#define TEST_ZA_LEN          32U
#define TEST_PUBLIC_KEY_LEN  64U
#define TEST_SIGNATURE_LEN   64U

extern Int32 BscHseCiu98TestSign(const TbscHseVendorSignParam *param);
extern Int32 BscHseCiu98TestMapError(se_error_t err);
extern Int32 BscHseCiu98TestGenerateKeyPair(
    const TbscHseVendorGenerateKeyPairParam *param);

static Int32 g_failures_ = 0;
static Int32 g_export_calls_ = 0;
static Int32 g_za_calls_ = 0;
static Int32 g_sign_calls_ = 0;
static se_error_t g_export_result_ = SE_SUCCESS;
static se_error_t g_za_result_ = SE_SUCCESS;
static se_error_t g_sign_result_ = SE_SUCCESS;
static UInt32 g_exported_public_key_len_ = TEST_PUBLIC_KEY_LEN;
static UInt32 g_last_public_slot_ = 0;
static UInt32 g_last_private_slot_ = 0;
static UInt32 g_last_public_alg_ = 0;
static UInt32 g_last_hash_alg_ = 0;
static UInt8 g_last_user_id_[TEST_USER_ID_LEN] = {0};
static UInt8 g_last_sign_input_[BSC_HSE_MAX_INPUT_LEN + TEST_ZA_LEN] = {0};
static UInt32 g_last_sign_input_len_ = 0;
static UInt32 g_log_calls_ = 0;
static UInt32 g_store_calls_ = 0;
static UInt8 g_stored_record_[2][80] = {{0}};
static UInt32 g_stored_record_len_[2] = {0};
static UInt32 g_stored_package_num_[2] = {0};
static se_error_t g_store_result_ = SE_SUCCESS;

UInt32 ALogGetFilter(void)
{
    return kALogLevelError;
}

void ALog(UInt32 level, const Int8 *tag, const Int8 *file_name,
          Int32 line_num, const Int8 *format, ...)
{
    (void)level;
    (void)tag;
    (void)file_name;
    (void)line_num;
    (void)format;
    g_log_calls_++;
}

peripheral_bus_driver *g_peripheral_bus_driver_PERIPHERAL_SPI = NULL;
struct peripheral_PERIPHERAL_SPI *p_peripheral_device_PERIPHERAL_SPI0 = NULL;

#define TEST_CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            (void)printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            g_failures_++; \
        } \
    } while (0)

static void L_ResetStubs(void)
{
    g_export_calls_ = 0;
    g_za_calls_ = 0;
    g_sign_calls_ = 0;
    g_export_result_ = SE_SUCCESS;
    g_za_result_ = SE_SUCCESS;
    g_sign_result_ = SE_SUCCESS;
    g_exported_public_key_len_ = TEST_PUBLIC_KEY_LEN;
    g_last_public_slot_ = 0;
    g_last_private_slot_ = 0;
    g_last_public_alg_ = 0;
    g_last_hash_alg_ = 0;
    g_last_sign_input_len_ = 0;
    g_log_calls_ = 0;
    g_store_calls_ = 0;
    g_store_result_ = SE_SUCCESS;
    memset(g_stored_record_, 0, sizeof(g_stored_record_));
    memset(g_stored_record_len_, 0, sizeof(g_stored_record_len_));
    memset(g_stored_package_num_, 0, sizeof(g_stored_package_num_));
    memset(g_last_user_id_, 0, sizeof(g_last_user_id_));
    memset(g_last_sign_input_, 0, sizeof(g_last_sign_input_));
}

se_error_t api_export_public_key(pub_key_t *pub_key)
{
    UInt32 index;

    g_export_calls_++;
    g_last_public_slot_ = pub_key->id;
    g_last_public_alg_ = pub_key->alg;
    if (g_export_result_ == SE_SUCCESS)
    {
        pub_key->val_len = g_exported_public_key_len_;
        for (index = 0; index < pub_key->val_len; index++)
        {
            pub_key->val[index] = (UInt8)(0x40U + index);
        }
    }
    return g_export_result_;
}

se_error_t api_generate_keypair(pub_key_t *pub_key)
{
    (void)pub_key;
    return SE_SUCCESS;
}

se_error_t api_store_key(privilege_level level, const uint8_t *in_buf,
                         uint32_t in_buf_len, uint32_t package_num)
{
    UInt32 index = g_store_calls_;

    TEST_CHECK(level == PRIVILEGE_LEVEL_0);
    TEST_CHECK(in_buf != NULL);
    if (index < 2U && in_buf != NULL &&
        in_buf_len <= sizeof(g_stored_record_[index]))
    {
        memcpy(g_stored_record_[index], in_buf, in_buf_len);
        g_stored_record_len_[index] = in_buf_len;
        g_stored_package_num_[index] = package_num;
    }
    g_store_calls_++;
    return g_store_result_;
}

se_error_t _api_register(peripheral_bus_driver *driver, peripheral *periph)
{
    (void)driver;
    (void)periph;
    return SE_SUCCESS;
}

se_error_t api_select(peripheral_type periph_type, uint32_t periph_id)
{
    (void)periph_type;
    (void)periph_id;
    return SE_SUCCESS;
}

se_error_t api_connect(uint8_t *out_buf, uint32_t *out_buf_len)
{
    (void)out_buf;
    (void)out_buf_len;
    return SE_SUCCESS;
}

se_error_t api_disconnect(void)
{
    return SE_SUCCESS;
}

se_error_t api_sym_encrypt(keyinfo_t *key,
                           alg_sym_param_t *sym_param,
                           const uint8_t *in_buf,
                           uint32_t in_buf_len,
                           uint8_t *out_buf,
                           uint32_t *out_buf_len)
{
    (void)key;
    (void)sym_param;
    (void)in_buf;
    (void)in_buf_len;
    (void)out_buf;
    (void)out_buf_len;
    return SE_ERR_FUNCTION_UNSUPPORTED;
}

se_error_t api_sym_decrypt(keyinfo_t *key,
                           alg_sym_param_t *sym_param,
                           const uint8_t *in_buf,
                           uint32_t in_buf_len,
                           uint8_t *out_buf,
                           uint32_t *out_buf_len)
{
    return api_sym_encrypt(key, sym_param, in_buf, in_buf_len,
                           out_buf, out_buf_len);
}

se_error_t api_asym_encrypt(keyinfo_t *key,
                            alg_asym_param_t *asym_param,
                            const uint8_t *in_buf,
                            uint32_t in_buf_len,
                            uint8_t *out_buf,
                            uint32_t *out_buf_len)
{
    (void)key;
    (void)asym_param;
    (void)in_buf;
    (void)in_buf_len;
    (void)out_buf;
    (void)out_buf_len;
    return SE_ERR_FUNCTION_UNSUPPORTED;
}

se_error_t api_asym_decrypt(keyinfo_t *key,
                            alg_asym_param_t *asym_param,
                            const uint8_t *in_buf,
                            uint32_t in_buf_len,
                            uint8_t *out_buf,
                            uint32_t *out_buf_len)
{
    return api_asym_encrypt(key, asym_param, in_buf, in_buf_len,
                            out_buf, out_buf_len);
}

se_error_t api_get_info(info_type type, se_info_t *info)
{
    (void)type;
    (void)info;
    return SE_SUCCESS;
}

se_error_t apdu_sm2_get_za(user_id_t *uid, pub_key_t *pub_key, uint8_t *za)
{
    UInt32 index;

    g_za_calls_++;
    TEST_CHECK(uid->val_len == TEST_USER_ID_LEN);
    TEST_CHECK(pub_key->val_len == TEST_PUBLIC_KEY_LEN);
    memcpy(g_last_user_id_, uid->val, TEST_USER_ID_LEN);
    if (g_za_result_ == SE_SUCCESS)
    {
        for (index = 0; index < TEST_ZA_LEN; index++)
        {
            za[index] = (UInt8)((0xA0U + index) ^
                                uid->val[index % TEST_USER_ID_LEN]);
        }
    }
    return g_za_result_;
}

se_error_t api_asym_sign(keyinfo_t *key,
                         alg_asym_param_t *asym_param,
                         const uint8_t *in_buf,
                         uint32_t in_buf_len,
                         uint8_t *sign_buf,
                         uint32_t *sign_buf_len)
{
    UInt32 index;

    g_sign_calls_++;
    g_last_private_slot_ = key->id;
    g_last_hash_alg_ = asym_param->hash_type;
    g_last_sign_input_len_ = in_buf_len;
    memcpy(g_last_sign_input_, in_buf, in_buf_len);
    for (index = 0; index < TEST_SIGNATURE_LEN; index++)
    {
        sign_buf[index] = (UInt8)(0xC0U + index);
    }
    *sign_buf_len = TEST_SIGNATURE_LEN;
    return g_sign_result_;
}

static void L_FillParam(TbscHseVendorSignParam *param,
                        const UInt8 *user_id,
                        const UInt8 *message,
                        UInt32 message_len,
                        UInt8 *signature,
                        UInt32 *signature_len,
                        UInt32 *r_size,
                        UInt32 *s_size)
{
    memset(param, 0, sizeof(*param));
    param->slot_id = TEST_PRIVATE_SLOT_ID;
    param->public_slot_id = TEST_PUBLIC_SLOT_ID;
    param->algorithm = kBscHseAlgorithmSm2;
    param->hash_algorithm = kBscHseAlgorithmSm3;
    param->padding = kBscHsePaddingPkcs1;
    param->input_mode = kBscHseSignInputModeMessage;
    param->user_id = user_id;
    param->user_id_len = TEST_USER_ID_LEN;
    param->input_data = message;
    param->input_len = message_len;
    param->signature = signature;
    param->signature_len = signature_len;
    param->r_size = r_size;
    param->s_size = s_size;
}

static void L_TestSuccess(void)
{
    static const UInt8 user_id[TEST_USER_ID_LEN] = {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBB, 0xCC, 0xC0,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xBC, 0xCC
    };
    static const UInt8 message[] = {0x11, 0x22, 0x33, 0x44};
    TbscHseVendorSignParam param;
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 signature_len = sizeof(signature);
    UInt32 r_size = 0;
    UInt32 s_size = 0;
    UInt32 index;
    Int32 result;

    L_ResetStubs();
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    result = BscHseCiu98TestSign(&param);

    TEST_CHECK(result == kBscHseResultOk);
    TEST_CHECK(g_export_calls_ == 1);
    TEST_CHECK(g_za_calls_ == 1);
    TEST_CHECK(g_sign_calls_ == 1);
    TEST_CHECK(g_last_public_slot_ == TEST_PUBLIC_SLOT_ID);
    TEST_CHECK(g_last_public_alg_ == ALG_SM2);
    TEST_CHECK(g_last_private_slot_ == TEST_PRIVATE_SLOT_ID);
    TEST_CHECK(g_last_hash_alg_ == ALG_SM3);
    TEST_CHECK(memcmp(g_last_user_id_, user_id, sizeof(user_id)) == 0);
    TEST_CHECK(g_last_sign_input_len_ == TEST_ZA_LEN + sizeof(message));
    for (index = 0; index < TEST_ZA_LEN; index++)
    {
        TEST_CHECK(g_last_sign_input_[index] ==
                   (UInt8)((0xA0U + index) ^
                           user_id[index % TEST_USER_ID_LEN]));
    }
    TEST_CHECK(memcmp(g_last_sign_input_ + TEST_ZA_LEN,
                      message, sizeof(message)) == 0);
    TEST_CHECK(signature_len == TEST_SIGNATURE_LEN);
    TEST_CHECK(r_size == TEST_SIGNATURE_LEN / 2U);
    TEST_CHECK(s_size == TEST_SIGNATURE_LEN / 2U);
}

static void L_TestDigestUnsupported(void)
{
    UInt8 user_id[TEST_USER_ID_LEN] = {0};
    UInt8 message[32] = {0};
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 signature_len = sizeof(signature);
    UInt32 r_size = 0;
    UInt32 s_size = 0;
    TbscHseVendorSignParam param;
    Int32 result;

    L_ResetStubs();
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    param.input_mode = kBscHseSignInputModeDigest;
    result = BscHseCiu98TestSign(&param);

    TEST_CHECK(result == kBscHseResultUnsupported);
    TEST_CHECK(g_export_calls_ == 0);
    TEST_CHECK(g_za_calls_ == 0);
    TEST_CHECK(g_sign_calls_ == 0);
}

static void L_TestZaPrerequisiteFailures(void)
{
    UInt8 user_id[TEST_USER_ID_LEN] = {0};
    UInt8 message[4] = {0};
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 signature_len;
    UInt32 r_size;
    UInt32 s_size;
    TbscHseVendorSignParam param;
    Int32 result;

    L_ResetStubs();
    signature_len = sizeof(signature);
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    g_export_result_ = SE_ERR_COMM;
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultDeviceUnavailable);
    TEST_CHECK(g_za_calls_ == 0);
    TEST_CHECK(g_sign_calls_ == 0);

    L_ResetStubs();
    signature_len = sizeof(signature);
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    g_exported_public_key_len_ = TEST_PUBLIC_KEY_LEN - 1U;
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultDeviceError);
    TEST_CHECK(g_za_calls_ == 0);
    TEST_CHECK(g_sign_calls_ == 0);

    L_ResetStubs();
    signature_len = sizeof(signature);
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    g_za_result_ = SE_ERR_COMM;
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultDeviceUnavailable);
    TEST_CHECK(g_za_calls_ == 1);
    TEST_CHECK(g_sign_calls_ == 0);
}

static void L_TestMessage1024AndDifferentUserIds(void)
{
    UInt8 user_id_a[TEST_USER_ID_LEN] = {0};
    UInt8 user_id_b[TEST_USER_ID_LEN] = {0};
    UInt8 message[1024] = {0};
    UInt8 first_za_input[TEST_ZA_LEN] = {0};
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 signature_len;
    UInt32 r_size;
    UInt32 s_size;
    UInt32 index;
    TbscHseVendorSignParam param;
    Int32 result;

    for (index = 0; index < TEST_USER_ID_LEN; index++)
    {
        user_id_a[index] = (UInt8)(0x10U + index);
        user_id_b[index] = (UInt8)(0x80U + index);
    }
    for (index = 0; index < (UInt32)sizeof(message); index++)
    {
        message[index] = (UInt8)index;
    }

    L_ResetStubs();
    signature_len = sizeof(signature);
    L_FillParam(&param, user_id_a, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultOk);
    TEST_CHECK(g_last_sign_input_len_ == TEST_ZA_LEN + sizeof(message));
    TEST_CHECK(memcmp(g_last_sign_input_ + TEST_ZA_LEN, message,
                      sizeof(message)) == 0);
    memcpy(first_za_input, g_last_sign_input_, sizeof(first_za_input));

    L_ResetStubs();
    signature_len = sizeof(signature);
    L_FillParam(&param, user_id_b, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultOk);
    TEST_CHECK(memcmp(g_last_user_id_, user_id_b, sizeof(user_id_b)) == 0);
    TEST_CHECK(g_last_sign_input_len_ == TEST_ZA_LEN + sizeof(message));
    TEST_CHECK(memcmp(g_last_sign_input_ + TEST_ZA_LEN, message,
                      sizeof(message)) == 0);
    TEST_CHECK(memcmp(first_za_input, g_last_sign_input_,
                      sizeof(first_za_input)) != 0);
}

static void L_TestRawDeviceFailureMapping(void)
{
    UInt8 user_id[TEST_USER_ID_LEN] = {0};
    UInt8 message[4] = {0};
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 signature_len = sizeof(signature);
    UInt32 r_size = 0;
    UInt32 s_size = 0;
    TbscHseVendorSignParam param;
    Int32 result;

    L_ResetStubs();
    L_FillParam(&param, user_id, message, sizeof(message),
                signature, &signature_len, &r_size, &s_size);
    g_sign_result_ = SE_ERR_OPERATION_VIOLATION;
    result = BscHseCiu98TestSign(&param);
    TEST_CHECK(result == kBscHseResultDeviceError);
    TEST_CHECK(g_sign_calls_ == 1);
    TEST_CHECK(g_log_calls_ >= 2U);
    TEST_CHECK(BscHseCiu98TestMapError(SE_ERR_OPERATION_VIOLATION) ==
               kBscHseResultDeviceError);
}

static void L_TestForceOverwriteSm2KeyPair(void)
{
    TbscHseVendorGenerateKeyPairParam param;
    Int32 result;

    L_ResetStubs();
    memset(&param, 0, sizeof(param));
    param.public_slot_id = TEST_PUBLIC_SLOT_ID;
    param.private_slot_id = TEST_PRIVATE_SLOT_ID;
    param.algorithm = kBscHseAlgorithmSm2;
    param.force_overwrite = True;

    result = BscHseCiu98TestGenerateKeyPair(&param);

    TEST_CHECK(result == kBscHseResultOk);
    TEST_CHECK(g_store_calls_ == 2U);
    TEST_CHECK(g_stored_record_len_[0] == 38U);
    TEST_CHECK(g_stored_package_num_[0] == 0x80U);
    TEST_CHECK(g_stored_record_[0][0] == 0x00U);
    TEST_CHECK(g_stored_record_[0][1] == 0x04U);
    TEST_CHECK(g_stored_record_[0][2] == TEST_PRIVATE_SLOT_ID);
    TEST_CHECK(g_stored_record_[0][3] == 0x62U);
    TEST_CHECK(g_stored_record_[0][4] == 0x00U);
    TEST_CHECK(g_stored_record_[0][5] == 0x20U);
    TEST_CHECK(g_stored_record_len_[1] == 73U);
    TEST_CHECK(g_stored_package_num_[1] == 0x80U);
    TEST_CHECK(g_stored_record_[1][0] == 0x00U);
    TEST_CHECK(g_stored_record_[1][1] == 0x04U);
    TEST_CHECK(g_stored_record_[1][2] == TEST_PUBLIC_SLOT_ID);
    TEST_CHECK(g_stored_record_[1][3] == 0x60U);
    TEST_CHECK(g_stored_record_[1][4] == 0x00U);
    TEST_CHECK(g_stored_record_[1][5] == 0x20U);
    TEST_CHECK(g_stored_record_[1][38] == 0x61U);
    TEST_CHECK(g_stored_record_[1][39] == 0x00U);
    TEST_CHECK(g_stored_record_[1][40] == 0x20U);
}

static void L_TestForceOverwriteStoreFailure(void)
{
    TbscHseVendorGenerateKeyPairParam param;
    Int32 result;

    L_ResetStubs();
    memset(&param, 0, sizeof(param));
    param.public_slot_id = TEST_PUBLIC_SLOT_ID;
    param.private_slot_id = TEST_PRIVATE_SLOT_ID;
    param.algorithm = kBscHseAlgorithmSm2;
    param.force_overwrite = True;
    g_store_result_ = SE_ERR_SECURITY_VIOLATION;

    result = BscHseCiu98TestGenerateKeyPair(&param);

    TEST_CHECK(result == kBscHseResultPermissionDenied);
    TEST_CHECK(g_store_calls_ == 1U);
    TEST_CHECK(g_log_calls_ >= 2U);
}

int main(void)
{
    L_TestSuccess();
    L_TestDigestUnsupported();
    L_TestZaPrerequisiteFailures();
    L_TestMessage1024AndDifferentUserIds();
    L_TestRawDeviceFailureMapping();
    L_TestForceOverwriteSm2KeyPair();
    L_TestForceOverwriteStoreFailure();

    if (g_failures_ == 0)
    {
        (void)printf("PASS: CIU98 ZA adapter tests\n");
    }
    return (g_failures_ == 0) ? 0 : 1;
}
