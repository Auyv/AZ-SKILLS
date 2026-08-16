/**
 * @addtogroup str_api
 * str_api
 * @{
 */
/**
 * @file        str_api.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/02/11
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/02/11   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _STR_API_H_
#define _STR_API_H_


#ifdef __cplusplus
extern "C"
{
#endif


/**
 * 查找字符串中符合目标字符集的第一个字符位置
 * @param *str  待查找的字符串
 * @param *charset  目标字符集
 * @param max_len   最大查找长度
 * @param *out_char 输出目标字符
 * @return >= 0: 目标字符位置
 *        -1: 未找到
 * @note:xxxxx
 */
int StrFindChar(const char *str, const char *charset, int max_len);



/**
 * 字符串转数字值
 * @param *in_str       字符串
 * @param in_str_len    字符串长度
 * @param *out_val      输出值
 * @return 消耗字符串长度
 * @note:xxxxx
 */
unsigned int StrToUInt32(const char *in_str, int in_str_len, unsigned int *out_val);



int StrHexToBytes(const char *hex_str, char *buff, int buff_size);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _STR_API_H_

