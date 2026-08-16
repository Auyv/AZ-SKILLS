/**
 * @addtogroup bsc_hse
 * bsc_hse
 * @{
 */
/**
 * @file        bsc_hse.h
 * @brief       BSC HSE 公共接口：11 项密码能力 + Init/Done
 *              - 不包含任何供应商 SDK 头文件
 *              - 线程安全；调用方按 Int32 返回值处理结果（0 成功）
 * @note        Init 时由调用方注入 vendor 接口和对象池内存
 * @author      HSED
 * @date        2026/07/21
 * @version     2.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2026/07/20   <td>1.0         <td>靳普诏       <td> 创建初始骨架
 * <tr><td>2026/07/21   <td>2.0         <td>HSED        <td> 编码规范对齐/去Config/去进程锁
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _BSC_HSE_H_
#define _BSC_HSE_H_

#include "bsc_hse/bsc_hse_types.h"
#include "bsc_hse/bsc_hse_vendor.h"
#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ============ 生命周期 ============ */

/**
 * @brief 初始化 BSC HSE
 *        - binding、对象池地址及对象数量均由调用方注入
 *        - 成功后通过 LOG_I 打印供应商信息
 * @return 0 成功；负值=失败
 */
Int32 BscHseInit(const TbscHseVendorBinding *binding,
                 TbscHseObject *object_pool,
                 UInt32 object_pool_count);

/**
 * @brief 反初始化；等待在途操作，清理对象，断开 SDK
 */
void BscHseDone(void);

/* ============ 对象管理 ============ */

/**
 * @brief 创建对象并装载全部属性默认值
 * @note  密钥槽位固定为 CIU98 的 1..14，调用方不能修改
 */
Int32 BscHseCreate(TbscHseHandle *handle);
Int32 BscHseDestroy(TbscHseHandle handle);

/* ============ 属性管理 ============ */

/**
 * @brief 设置 Int 属性
 * @note  所有 KEY_ID 属性只读；修改时返回 kBscHseResultPermissionDenied
 */
Int32 BscHseSetPropertyInt(TbscHseHandle handle,
                            TbscHsePropertyId property_id,
                            Int32 value);

Int32 BscHseSetPropertyBytes(TbscHseHandle handle,
                              TbscHsePropertyId property_id,
                              const UInt8 *value,
                              UInt32 value_size);

Int32 BscHseGetPropertyInt(TbscHseHandle handle,
                            TbscHsePropertyId property_id,
                            Int32 *value);

Int32 BscHseGetPropertyBytes(TbscHseHandle handle,
                              TbscHsePropertyId property_id,
                              UInt8 *value,
                              UInt32 *value_size);

/* ============ 密码运算 ============ */

Int32 BscHseEncrypt(TbscHseHandle handle,
                     TbscHseAlgorithm algorithm,
                     const UInt8 *input_data,
                     UInt32 input_size,
                     UInt8 *output_data,
                     UInt32 *output_size);

Int32 BscHseDecrypt(TbscHseHandle handle,
                     TbscHseAlgorithm algorithm,
                     const UInt8 *input_data,
                     UInt32 input_size,
                     UInt8 *output_data,
                     UInt32 *output_size);

/**
 * @brief 非对称签名
 *        - SM2 签名要求对象已保存至少 16 字节 USER_ID；不足 INVALID_PARAM
 *        - USER_ID > 16 字节：只使用前 16 字节派生 ZA；后续字节不参与
 *        - ECC256/SM2 按 r||s 返回，r_size+s_size == *signature_size
 *        - RSA/SM9 返回 r_size=0 / s_size=0
 */

/** @cond BSC_HSE_SIGN_PARAM_EXCEEDS_FIVE */

Int32 BscHseSign(TbscHseHandle handle,
                  TbscHseAlgorithm algorithm,
                  const UInt8 *input_data,
                  UInt32 input_size,
                  UInt8 *signature,
                  UInt32 *signature_size,
                  UInt32 *r_size,
                  UInt32 *s_size);

/** @endcond */

Int32 BscHseExportPublicKey(TbscHseHandle handle,
                             TbscHseAlgorithm algorithm,
                             UInt8 *public_key,
                             UInt32 *public_key_size);

/**
 * @brief 导入公钥
 * @note  当前固定槽位策略禁止客户端更新公钥，合法句柄调用返回
 *        kBscHseResultPermissionDenied
 */
Int32 BscHseImportPublicKey(TbscHseHandle handle,
                             TbscHseAlgorithm algorithm,
                             const UInt8 *public_key,
                             UInt32 public_key_size,
                             Bool replace_existing_key);

#ifdef __cplusplus
}
#endif

#endif  ///< _BSC_HSE_H_
