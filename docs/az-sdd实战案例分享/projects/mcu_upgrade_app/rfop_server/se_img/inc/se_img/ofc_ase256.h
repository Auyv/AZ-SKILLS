/**
 * @addtogroup ofc_ase256
 * ofc_ase256
 * @{
 */
/**
 * @file        ofc_ase256.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/06
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/06   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _OFC_ASE256_H_
#define _OFC_ASE256_H_

#include "ofc/ofc_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

typedef enum {
    kOfAesModeCbc = 0,
    kOfAesModeCfb = 1
} OfAesMode ; 

typedef enum {
    OF_PADDING_NONE = 0,   ///< 不进行填充
    OF_PADDING_PKCS7 = 1   ///< 使用PKCS7填充标准
} OfPaddingMode;

typedef struct {
    int is_encrypt;          ///< 工作模式: 1-加密模式, 0-解密模式
    unsigned char key[32];   ///< AES256密钥，必须为32字节长度
    unsigned char iv[16];    ///< 初始化向量，必须为16字节长度
    OfPaddingMode padding;   ///< 填充模式，参见OfPaddingMode枚举
    EVP_CIPHER_CTX *ctx;     ///< OpenSSL算法上下文
} Aes256Ctx;

/**
 * @brief 初始化AES256加解密上下文
 * @param ctx        [in/out] 上下文对象指针
 * @param mode       [in] 算法模式: 0-CBC, 1-CFB
 * @param is_encrypt [in] 工作模式: 1-加密, 0-解密
 * @param key        [in] 加密密钥(32字节)
 * @param key_len    [in] 密钥长度，必须为32
 * @param iv         [in] 初始化向量(16字节)
 * @param iv_len     [in] 向量长度，必须为16
 * @param padding    [in] 填充模式枚举值
 * @return 0成功，非0为错误码:
 *         -1: 参数错误
 *         -2: 上下文创建失败
 */
int OfAes256Init(Aes256Ctx *ctx, OfAesMode mode, int is_encrypt,
                const UInt8 *key, Int32 key_len,
                const UInt8 *iv, Int32 iv_len, 
                int padding);

/**
 * @brief 处理加解密数据块
 * @param ctx      [in] 上下文对象
 * @param in       [in] 输入数据指针
 * @param in_len   [in] 输入数据长度
 * @param out      [out] 输出缓冲区指针
 * @param out_len  [out] 实际输出数据长度
 * @return 0成功，非0为错误码:
 *         -1: 无效上下文
 *         -2: 加解密操作失败
 */
int OfAes256Update(Aes256Ctx *ctx,
                  const UInt8 *in, Int32 in_len,
                  UInt8 *out, Int32 *out_len);

/**
 * @brief 结束加解密流程并清理资源
 * @param ctx      [in] 上下文对象
 * @param out      [out] 最终输出缓冲区
 * @param out_len  [out] 最终输出数据长度
 * @return 0成功，非0为错误码:
 *         -1: 无效上下文
 *         -2: 最终块处理失败
 */
int OfAes256Final(Aes256Ctx *ctx, UInt8 *out, Int32 *out_len);

#endif // _OFC_ASE256_H_


