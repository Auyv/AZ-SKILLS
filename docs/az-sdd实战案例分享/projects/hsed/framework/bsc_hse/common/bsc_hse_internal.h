/**
 * @file        bsc_hse_internal.h
 * @brief       BSC HSE 内部类型声明
 *              - TBscHseObject / TBscHseObjectPool
 *              - 内部辅助函数声明
 *              - TODO: 后续 SPC 适配层会读到本文件；暂不暴露为公共头文件
 * @author      HSED
 * @date        2026/07/21
 * @version     2.0
 */

#ifndef _BSC_HSE_INTERNAL_H_
#define _BSC_HSE_INTERNAL_H_

#include "bsc_hse/bsc_hse.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ============ 内部辅助 ============ */

Int32 BscHseObjectReadInt(const TbscHseObject *obj,
                           TbscHsePropertyId property_id,
                           Int32 *value);

Int32 BscHseObjectWriteInt(TbscHseObject *obj,
                            TbscHsePropertyId property_id,
                            Int32 value);

const char *BscHseResultString(Int32 result);
const char *BscHseAlgorithmString(TbscHseAlgorithm algorithm);
const char *BscHsePropertyIdString(TbscHsePropertyId property_id);

TbscHseAlgorithm BscHsePropertyAlgorithmOf(TbscHsePropertyId property_id);
TbscHseKeyRole BscHsePropertyKeyRole(TbscHsePropertyId property_id);
TbscHseKeyRole BscHseAlgorithmDefaultRole(TbscHseAlgorithm algorithm);

Bool BscHseAlgorithmIsSymmetric(TbscHseAlgorithm algorithm);
Bool BscHseAlgorithmIsAsymmetric(TbscHseAlgorithm algorithm);
Bool BscHseAlgorithmIsHash(TbscHseAlgorithm algorithm);

UInt32 BscHseAlgorithmSm2DigestLen(void);

#ifdef __cplusplus
}
#endif

#endif  ///< _BSC_HSE_INTERNAL_H_
