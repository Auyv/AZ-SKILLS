/**
 * @addtogroup dpt_encoder
 * dpt_encoder
 * @{
 */
/**
 * @file        dpt_encoder.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/05/11
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/05/11   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _DPT_ENCODER_H_
#define _DPT_ENCODER_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct TDptEncoderTag TDptEncoder;
struct TDptEncoderTag
{
    UInt8 *buff_begin;
    UInt8 *buff_end;
    UInt8 *buff_pos;
};

/**
 * 创建编码对象
 * @param *self 对象指针
 * @param *buff 数据缓冲区
 * @param buff_size 数据缓冲区大小
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderCreate(TDptEncoder *self, UInt8 *buff, Int32 buff_size);

/**
 * 释放对象
 * @param *self 对象指针
 * @return 
 * @note:xxxxx
 */
void TDptEncoderDestroy(TDptEncoder *self);


/**
 * 可写字节数
 * @param *self 对象指针
 * @return >= 0 当前可写入的字节数
 *         <  0 异常错误
 * @note:xxxxx
 */
Int32 TDptEncoderWriteAble(TDptEncoder *self);

/**
 * 当前pos指针位置，
 * @param *self 对象指针
 * @return >= 0 已完成编码大小
 *         <  0 异常错误
 * @note: 等价于已完成编码大小
 */
Int32 TDptEncoderWritePosition(TDptEncoder *self);

/**
 * 写入UInt8类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteUInt8(TDptEncoder *self, UInt8 data);

/**
 * 写入Int8类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteInt8(TDptEncoder *self, Int8 data);

/**
 * 写入UInt16类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteUInt16(TDptEncoder *self, UInt16 data, Bool reversal);


/**
 * 写入Int16类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteInt16(TDptEncoder *self, Int16 data, Bool reversal);


/**
 * 写入UInt32类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteUInt32(TDptEncoder *self, UInt32 data, Bool reversal);


/**
 * 写入Int32类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteInt32(TDptEncoder *self, Int32 data, Bool reversal);

/**
 * 写入UInt64类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteUInt64(TDptEncoder *self, UInt64 data, Bool reversal);


/**
 * 写入Int64类型数据
 * @param *self 对象指针
 * @param data  数据值
 * @param reversal 字节序写入控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptEncoderWriteInt64(TDptEncoder *self, Int64 data, Bool reversal);

// /**
//  * 写入TMem类型数据
//  * @param *self 对象指针
//  * @param data  数据值
//  * @return True 成功；False 失败
//  * @note:xxxxx
//  */
// Bool TDptEncoderWriteTMem(TDptEncoder *self, const TMem *data);

/**
 * 写入Mem类型数据
 * @param *self
 * @param *data
 * @param size
 * @return 
 * @note:xxxxx
 */
Bool TDptEncoderWriteMem(TDptEncoder *self, const void *data, Int32 size);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _DPT_ENCODER_H_

/**
 * @}  Generated on "2024-05-11 22:35:35" by the tool "gen_hq_file.py >> V20231119" 
 */

