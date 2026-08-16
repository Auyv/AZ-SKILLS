/**
 * @addtogroup dpt_decoder
 * dpt_decoder
 * @{
 */
/**
 * @file        dpt_decoder.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/05/12
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/05/12   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _DPT_DECODER_H_
#define _DPT_DECODER_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct TDptDecoderTag TDptDecoder;
struct TDptDecoderTag
{
    UInt8 *buff_begin;
    UInt8 *buff_end;
    UInt8 *buff_pos;
};


/**
 * 创建解码对象
 * @param *self 对象指针
 * @param *buff 数据缓冲区
 * @param buff_size 数据缓冲区长度
 * @return True 成功；False 失败
 * @note: 多线程不安全
 */
Bool TDptDecoderCreate(TDptDecoder *self, const UInt8 *buff, UInt32 buff_size);

/**
 * 释放对象
 * @param *self 对象指针
 * @return 
 * @note:xxxxx
 */
void TDptDecoderDestroy(TDptDecoder *self);


/**
 * 可读字节数
 * @param *self 对象指针
 * @return >= 0 当前可读取的字节数
 *         <  0 异常错误
 * @note:xxxxx
 */
Int32 TDptDecoderReadAble(TDptDecoder *self);

/**
 * 读指针的位置
 * @param *self 对象指针
 * @return >= 0 已完成解码大小
 *         <  0 异常错误
 * @note: 等价于已完成解码大小
 */
Int32 TDptDecoderReadPosition(TDptDecoder *self);

/**
 * 读取UInt8类型数据
 * @param *self 对象指针
 * @param r_data  数据值
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadUInt8(TDptDecoder *self, UInt8 *r_data);

/**
 * 读取Int8类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadInt8(TDptDecoder *self, Int8 *r_data);

/**
 * 读取UInt16类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadUInt16(TDptDecoder *self, UInt16 *r_data, Bool reversal);


/**
 * 读取Int16类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadInt16(TDptDecoder *self, Int16 *r_data, Bool reversal);


/**
 * 读取UInt32类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadUInt32(TDptDecoder *self, UInt32 *r_data, Bool reversal);


/**
 * 读取Int32类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadInt32(TDptDecoder *self, Int32 *r_data, Bool reversal);

/**
 * 读取UInt64类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadUInt64(TDptDecoder *self, UInt64 *r_data, Bool reversal);


/**
 * 读取Int64类型数据
 * @param *self 对象指针
 * @param *r_data  数据值
 * @param reversal 字节序读取控制。 True 反转字节序；False 不反转
 * @return True 成功；False 失败
 * @note:xxxxx
 */
Bool TDptDecoderReadInt64(TDptDecoder *self, Int64 *r_data, Bool reversal);

// /**
//  * 读取TMem类型数据
//  * @param *self 对象指针
//  * @param *r_data  数据值
//  * @return True 成功；False 失败
//  * @note:xxxxx
//  */
// Bool TDptDecoderReadTMem(TDptDecoder *self, TMem *r_data);

/**
 * 读取Mem类型数据
 * @param *self
 * @param *r_data
 * @param size
 * @return 
 * @note:xxxxx
 */
Bool TDptDecoderReadMem(TDptDecoder *self, void **r_data, Int32 size);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _DPT_DECODER_H_

/**
 * @}  Generated on "2024-05-12 11:21:26" by the tool "gen_hq_file.py >> V20231119" 
 */

