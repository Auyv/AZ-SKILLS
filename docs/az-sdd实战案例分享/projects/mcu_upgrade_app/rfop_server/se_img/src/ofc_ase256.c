/**
 * @file        ofc_ase256.c
 * @brief       AES256加解密实现
 * @note        支持CBC/CFB模式，可递归调用的流式加解密接口
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/06
 * @version     1.0
 *
 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/06   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#include "ofc_ase256.h"
#include "alog/alog.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/opensslv.h>    // 添加版本检查支持

// 如果需要在代码中使用这些函数，可以这样处理：
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new()     EVP_MD_CTX_create()
#define EVP_MD_CTX_free(ctx) EVP_MD_CTX_destroy(ctx)
#endif


#define AES256_KEY_LENGTH 32
#define AES256_IV_LENGTH 16

int OfAes256Init(Aes256Ctx *ctx, OfAesMode mode, int is_encrypt,
                const UInt8 *key, Int32 key_len,
                const UInt8 *iv, Int32 iv_len,
                int padding)
{
    const EVP_CIPHER *cipher = NULL;
    
    if (!ctx || !key || !iv) {
        ALOG_E("Invalid parameters");
        return -1;
    }

    if (key_len != AES256_KEY_LENGTH || iv_len != AES256_IV_LENGTH) {
        ALOG_E("Key/IV length error (key:%zu/iv:%zu)", key_len, iv_len);
        return -2;
    }

    switch (mode) {
        case kOfAesModeCbc: cipher = EVP_aes_256_cbc(); break;
        case kOfAesModeCfb: cipher = EVP_aes_256_cfb128(); break;
        default: return -3;
    }

    ctx->ctx = EVP_CIPHER_CTX_new();
    if (!ctx->ctx) {
        ALOG_E("EVP_CIPHER_CTX_new failed");
        return -4;
    }

    memcpy(ctx->key, key, AES256_KEY_LENGTH);
    memcpy(ctx->iv, iv, AES256_IV_LENGTH);
    ctx->is_encrypt = is_encrypt;
    ctx->padding = padding;

    if (EVP_CipherInit_ex(ctx->ctx, cipher, NULL, 
                        ctx->key, ctx->iv, ctx->is_encrypt) != 1) {
        EVP_CIPHER_CTX_free(ctx->ctx);
        ctx->ctx = NULL;
        return -5;
    }

    EVP_CIPHER_CTX_set_padding(ctx->ctx, ctx->padding);
    return 0;
}

int OfAes256Update(Aes256Ctx *ctx,
                  const UInt8 *in, Int32 in_len,
                  UInt8 *out, Int32 *out_len)
{
    int tmp_len = 0;
    
    if (!ctx || !ctx->ctx || !out_len || *out_len < in_len + 16) {
        ALOG_E("Invalid context");
        return -1;
    }

    if (EVP_CipherUpdate(ctx->ctx, out, &tmp_len, in, (int)in_len) != 1) {
        ALOG_E("EVP_CipherUpdate failed: %s",
             ERR_error_string(ERR_get_error(), NULL));
        return -2;
    }

    *out_len = tmp_len;
    return 0;
}

int OfAes256Final(Aes256Ctx *ctx, UInt8 *out, Int32 *out_len)
{
    int tmp_len = 0;
    
    if (!ctx || !ctx->ctx) {
        ALOG_E("Invalid context");
        return -1;
    }

    if (EVP_CipherFinal_ex(ctx->ctx, out, &tmp_len) != 1) {
        ALOG_E("EVP_CipherFinal_ex failed: %s",
             ERR_error_string(ERR_get_error(), NULL));
        return -2;
    }

    *out_len = tmp_len;
    EVP_CIPHER_CTX_free(ctx->ctx);
    ctx->ctx = NULL;
    return 0;
}










