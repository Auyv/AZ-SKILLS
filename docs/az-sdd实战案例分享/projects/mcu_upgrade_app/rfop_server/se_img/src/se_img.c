/**
 * @file        se_img.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/05
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/05   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "se_img.h"
#include "ofc/ofc_crc16.h"
#include "ofc_ase256.h"
#include "ffc/ffc_file.h"
#include "ffc/ffc_file_utils.h"
#include "str_api.h"
#include "rfop/rfop_common.h"

#define ALOG_TAG "se_img"
#include "alog/alog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/opensslv.h>    // 添加版本检查支持
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>


// 如果需要在代码中使用这些函数，可以这样处理：
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new()     EVP_MD_CTX_create()
#define EVP_MD_CTX_free(ctx) EVP_MD_CTX_destroy(ctx)
#endif


#define _ERR_BREAK(info) if(1){ALOG_E(info); result = -(__LINE__); break;}


// struct TSeImgHeaderTag 
// {
//     const char *filename;
//     FILE *fp;
//     TOtaPackagHeadMsgOtaPackageHead head_info;
//     Bool is_valid;
// };

TSeImg *CreateTSeImg(const char *filename)
{
    TSeImg *self = (TSeImg *)OfMalloc(sizeof(TSeImg));
    memset(self, 0, sizeof(TSeImg));
    if (!self) 
        return NULL;
    
    self->filename = filename;
    self->fp = FfFileOpen(filename, kFfFileModeOpenRead);
    if (self->fp == INVALID_HANDLE_VALUE)
    {
        OfFree(self);
        return NULL;
    }
    self->fp_file_size = FfFileSeek(self->fp, 0, kFfSeekOriginFromEnd); // 获取文件大小
    self->is_valid = False;
    self->file_name_func = NULL;

    if (TSeImgIsValid(self))
        FfFileSeek(self->fp, 0, kFfSeekOriginFromEnd);
    else
    {
        DestroyTSeImg(self);
        return NULL;
    }

    return self;
}

void DestroyTSeImg(TSeImg *self)
{
    if (!self) 
        return;
    
    if (self->fp != INVALID_HANDLE_VALUE) 
    {
        FfFileClose(self->fp);
        self->fp = INVALID_HANDLE_VALUE;
    }
    OfFree(self);
}

Bool TSeImgIsValid(TSeImg *self)
{
    Bool result = False;

    if (self != NULL)
    {
        if (self->is_valid)
            result = True;
        else
        {
            // 保存当前位置
            Int32 rsize = 0;
            Int32 last_seek = FfFileSeek(self->fp, 0, kFfSeekOriginFromCurrent);
            const Int32 tbuff_size = 1024;
            char *tbuff = OfMalloc(tbuff_size);

            do
            {
                // 读取文件头信息
                TOtaPackagHeadMsgOtaPackageHeadMem *head_data = (TOtaPackagHeadMsgOtaPackageHeadMem *)tbuff;

                FfFileSeek(self->fp, 0, kFfSeekOriginFromBegin);

                if (tbuff == NULL
                    || FfFileRead(self->fp, head_data, sizeof(*head_data)) != sizeof(*head_data))
                    break;
                else
                {
                    char *tp = NULL;

                    OtaPackagHeadMsgOtaPackageHeadDecode(&self->head_info, (const Byte *)head_data, sizeof(*head_data));

                    ALOG_I("filename: %s", self->filename);
                    tp = (char *)self->head_info.const_tag;
                    ALOG_I("const_tag: %c%c%c%c", tp[0], tp[1], tp[2], tp[3]);
                    ALOG_I("head_crc16: 0x%x", self->head_info.head_crc16);
                    tp = (char *)self->head_info.time_bcd;
                    ALOG_I("time_bcd: 0x%02x%02x%02x%02x%02x%02x", tp[0], tp[1], tp[2], tp[3], tp[4], tp[5]);
                    ALOG_I("head_size: 0x%x", self->head_info.head_size);
                    ALOG_I("content_struct: 0x%x", self->head_info.content_struct);
                    ALOG_I("content_encrypt: 0x%x", self->head_info.content_encrypt);
                    ALOG_I("signature_type: 0x%x", self->head_info.signature_type);
                    ALOG_I("content_crc16: 0x%x", self->head_info.content_crc16);
                }

                // 判断固定标签头
                if (memcmp(self->head_info.const_tag, "CtTg", 4) != 0)
                    break;

                // 判断文件头大小
                if (self->head_info.head_size!= sizeof(TOtaPackagHeadMsgOtaPackageHeadMem))
                    break;

                // 计算文件头crc16
                // unsigned short RfopCalcCRC16(const char* Data, int Size)
                UInt16 crc16 = OfCalcCrc16s((UInt8 *)&head_data->time_bcd[0], self->head_info.head_size - 6, 0xffff);
                // 判断文件头crc16
                if (crc16 != self->head_info.head_crc16)
                {
                    ALOG_E("head_crc16 error. calc_crc:0x%x, head_info.head_crc16: 0x%x", crc16, self->head_info.head_crc16);
                    break;
                }
                    

                if (self->head_info.content_struct == 0)
                {
                    ALOG_E("content_struct error. content_struct:%d", self->head_info.content_struct);
                    break;
                }
                    
                
                #if (0)  // <2025-03-11> Jpz todo : 暂时跳过
                // 计算文件内容crc16 
                crc16 = 0xffff;
                rsize = FfFileRead(self->fp, tbuff, tbuff_size);
                while (rsize > 0)
                {
                    crc16 = OfCalcCrc16s(tbuff, tbuff_size, crc16); 
                    rsize = FfFileRead(self->fp, tbuff, tbuff_size);
                }
                if (crc16 != self->head_info.content_crc16)
                {
                    ALOG_E("content_crc16 error. calc_crc:0x%x, head_info.content_crc16: 0x%x", crc16, self->head_info.content_crc16);
                    break;
                }
                #endif  
                    
                self->is_valid = True;
                result = True;
            } while (0);

            // 还原当前位置
            FfFileSeek(self->fp, last_seek, kFfSeekOriginFromBegin);

            if (tbuff != NULL)
                OfFree(tbuff);
        }
    }

    return result;
}


Bool TSeImgSetFileNameFunc(TSeImg *self, TSeImgFileNameFunc method, void *user_param)
{
    Bool result = False;

    if (self != NULL)
    {
        self->file_name_func = method;
        self->user_param = user_param;
        result = True;
    }

    return result;
}

const char *TSeImgFileName(TSeImg *self, TSeImgFileId file_id)
{
    TSeImgFileNameFunc func = self == NULL ? NULL : self->file_name_func;
    const char *result = func == NULL ? NULL : func(file_id, self->user_param);

    ALOG_I("file_id: %d, result: %s", file_id, result);

    return result;
}





static Int32 L_TSeImgUnpackContext_NoEncrypt(TSeImg *self, const char *dest_filename, TSeImgReadContextCallback callback)
{
    Int32 result = -1;  // 暂不支持明文数据

    return result;
}

// ase256-CBC加密内容, 格式为: 签名+升级文件
/**
 * @brief 解包AES256-CBC加密的升级文件(格式为: 签名+升级文件)
 * @details 处理流程:
 *          1. 读取加密内容
 *          2. AES256-CBC解密
 *          3. 提取并验证RSA2048签名
 *          4. 写入解密后的升级文件
 * 
 * @param self [in] TSeImg对象指针
 * @param dest_filename [in] 目标文件名
 * @param callback [in] 数据读取回调函数，可为NULL
 * @return Int32 成功返回写入的字节数，失败返回负值
 */
static Int32 L_TSeImgUnpackContext_Ase256Cbc_Case2(TSeImg *self, const char *dest_filename, TSeImgReadContextCallback callback)
{
    Int32 result = -(__LINE__);
    char *_content_buff = NULL;
    Int32 content_size = self->fp_file_size - self->head_info.head_size;
    char *main_data = NULL;
    Int32 main_data_size = -1;

    do
    {
        // step1 取出全部加密内容
        {
            _content_buff = OfMalloc(content_size);

            if (_content_buff == NULL)  
                break;

            FfFileSeek(self->fp, self->head_info.head_size, kFfSeekOriginFromBegin);
            if (FfFileRead(self->fp, _content_buff, content_size) != content_size)
                _ERR_BREAK("FfFileRead failed");
        }
        // step2 解密全部加密内容
        {
            // 使用openssl库进行Ase256Cbc解密
            Aes256Ctx ctx;
            Int32 tmp_size = 0;
            unsigned char tmp_buff[65] = {0};
            unsigned char key[32] = {0};  // 实际使用时需替换为有效密钥
            unsigned char iv[16] = {0};   // 实际使用时需替换为有效IV
            Int32 decrypt_buff_size = content_size + 16; // AES块大小
            char *_decrypt_buff = OfMalloc(decrypt_buff_size); // AES块大小

            //<2025-08-15> Jpz todo : 抽象为接口, 获取密钥和IV
            THandle tfp = FfFileOpen(TSeImgFileName(self, kSeImgFileIdContextAse256CbcKey), kFfFileModeOpenRead);
            if (tfp == INVALID_HANDLE_VALUE)
                _ERR_BREAK("open Ase256CbcKey failed");
            FfFileSeek(tfp, 0, kFfSeekOriginFromBegin);
            tmp_size = FfFileRead(tfp, tmp_buff, sizeof(tmp_buff) - 1);
            tmp_size = StrHexToBytes(tmp_buff, key, sizeof(key));
            ALOG_D("key size: %d, key: %s", tmp_size, tmp_buff);
            ALOG_HEX(6, 32, key, tmp_size);
            FfFileClose(tfp);
            tfp = INVALID_HANDLE_VALUE;
            if (tmp_size != sizeof(key))
                _ERR_BREAK("Get Ase256CbcKey failed");

            tfp = FfFileOpen(TSeImgFileName(self, kSeImgFileIdContextAse256CbcIv), kFfFileModeOpenRead);
            if (tfp == INVALID_HANDLE_VALUE)
                _ERR_BREAK("open Ase256CbcIv failed");
            FfFileSeek(tfp, 0, kFfSeekOriginFromBegin);
            tmp_size = FfFileRead(tfp, tmp_buff, sizeof(tmp_buff) - 1);
            tmp_size = StrHexToBytes(tmp_buff, iv, sizeof(iv));
            ALOG_D("iv size: %d, iv: %s", tmp_size, tmp_buff);
            ALOG_HEX(6, 32, iv, tmp_size);
            FfFileClose(tfp);
            tfp = INVALID_HANDLE_VALUE;
            if (tmp_size != sizeof(iv))
                _ERR_BREAK("Get Ase256CbcIv failed");

            if (_decrypt_buff == NULL)
                _ERR_BREAK("_decrypt_buff OfMalloc failed");

            // 初始化解密上下文(CFC模式)
            if (OfAes256Init(&ctx, kOfAesModeCbc, 0, key, sizeof(key), iv, sizeof(iv), OF_PADDING_PKCS7) != 0)
                _ERR_BREAK("OfAes256Init failed");

            // 解密数据
            Int32 out_len = decrypt_buff_size;
            if (OfAes256Update(&ctx, _content_buff, content_size, _decrypt_buff, &out_len) != 0)
                _ERR_BREAK("OfAes256Update failed");

            // 处理最后的数据块
            Int32 final_len = decrypt_buff_size - out_len;
            if (OfAes256Final(&ctx, _decrypt_buff + out_len, &final_len)!= 0)
                _ERR_BREAK("OfAes256Final failed");

            // 释放缓冲区,并交换缓冲区
            OfFree(_content_buff);
            _content_buff = _decrypt_buff;
            ALOG_I("content_size: %d, decrypt_buff_size: %d, out_len: %d, final_len: %d", content_size, decrypt_buff_size, out_len, final_len);
            content_size = out_len + final_len;
            
        }

        // step3 提取文件签名数据进行验签
        {
            if (self->head_info.signature_type == 1)
            {
                const Int32 signature_size = 256;   
                // char _sign_calc[signature_size] = {0};
                char *sign_data = _content_buff;

                main_data_size = content_size - signature_size;
                main_data = &_content_buff[signature_size];
                
                ALOG_D("signature data");
                ALOG_HEX(6, 32, sign_data, signature_size);
                ALOG_D("main data");
                ALOG_HEX(6, 32, main_data, 256);

                // 计算签名. 调用openssl完成对main_data数据的 Sha256-RSA2048 签名提取
                EVP_PKEY *pub_key = NULL;
                EVP_MD_CTX *md_ctx = NULL;
                FILE *key_file = NULL;
                Bool verify_result = False;
                
                do 
                {
                    // 1. 创建摘要上下文
                    md_ctx = EVP_MD_CTX_new();
                    if (!md_ctx) 
                        _ERR_BREAK("EVP_MD_CTX_new failed");

                    // 2. 读取公钥文件// 获取文件名
                    // key_file = fopen("public.pem", "rb");
                    key_file = fopen(TSeImgFileName(self, kSeImgFileIdSignatureRsaPublicPem), "rb");
                    if (!key_file)
                        _ERR_BREAK("Open public key file failed");

                    pub_key = PEM_read_PUBKEY(key_file, NULL, NULL, NULL);
                    if (!pub_key)
                        _ERR_BREAK("Read public key failed");

                    // 3. 初始化验证
                    if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, pub_key) != 1)
                        _ERR_BREAK("EVP_DigestVerifyInit failed");

                    // 4. 更新待验证数据
                    if (EVP_DigestVerifyUpdate(md_ctx, main_data, main_data_size) != 1)
                        _ERR_BREAK("EVP_DigestVerifyUpdate failed");

                    // 5. 验证签名
                    if (EVP_DigestVerifyFinal(md_ctx, (unsigned char *)sign_data, signature_size) != 1)
                        _ERR_BREAK("Signature verification failed");
                    
                    // // 对比签名
                    // if (memcmp(_sign_calc, sign_data, signature_size) != 0)
                    //     _EER_BREAK("signature error");

                    verify_result = True;
                } while (0);

                // 清理资源
                if (md_ctx) EVP_MD_CTX_free(md_ctx);
                if (pub_key) EVP_PKEY_free(pub_key);
                if (key_file) fclose(key_file);

                if (!verify_result)
                    _ERR_BREAK("Signature verification failed");
                
            }
            else
                _ERR_BREAK("signature_type not support");
        }

        // step5 明文数据写入文件
        {
            result = main_data_size;

            if (dest_filename != NULL)
            {
                FfDeleteFile(dest_filename);
                
                THandle dfp = FfFileOpen(dest_filename, kFfFileModeOpenWrite | kFfFileModeAlways);

                if (dfp == INVALID_HANDLE_VALUE)
                    _ERR_BREAK("FfFileOpen dest_filename failed");
                
                FfFileSeek(dfp, 0, kFfSeekOriginFromBegin);
                result = FfFileWrite(dfp, main_data, main_data_size);
                // 关闭文件句柄
                FfFileClose(dfp);

                ALOG_I("FfFileWrite result: %d", result);
            }
            else
            {
                result = -(__LINE__);
                ALOG_E("dest_filename is NULL");
            }

            if (result > 0 && callback != NULL)
                callback(main_data, result);
        }

    } while (0);
    

    if (_content_buff != NULL)
        OfFree(_content_buff);

    ALOG_I("%s result: %d", __func__, result);
    return result;
}

static Int32 L_TSeImgUnpackContext_Ase256Cbc(TSeImg *self, const char *dest_filename, TSeImgReadContextCallback callback)
{
    Int32 result = -(__LINE__); // 格式错误

    switch (self->head_info.content_struct)
    {
    case 1: // 仅升级文件
    result = -(__LINE__); // 暂不支持
        break;
    case 2: // 签名+升级文件
        result = L_TSeImgUnpackContext_Ase256Cbc_Case2(self, dest_filename, callback);
        break;
    default:
        break;
    }

    return result;
}



Int32 TSeImgUnpackImgToFile(TSeImg *self, const char *dest_filename, TSeImgReadContextCallback callback)
{
    Int32 result = 0;

    switch (self->head_info.content_encrypt)
    {
        case 0: result = L_TSeImgUnpackContext_NoEncrypt(self, dest_filename, callback); break;  // 明文数据
        case 1: result = L_TSeImgUnpackContext_Ase256Cbc(self, dest_filename, callback); break;  // AES256-CBC
        default:
            result = -(__LINE__);
            ALOG_E("content_encrypt error. content_encrypt:%d", self->head_info.content_encrypt);
            break;
    }

    return result;
}
