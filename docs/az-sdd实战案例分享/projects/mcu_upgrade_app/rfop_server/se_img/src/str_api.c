/**
 * 
 * @File    : 
 * @Brief   : ... 
 * @Version : 1.0
 * @Author  : 靳普诏
 * @E-Mail  : 499367619@qq.com
 */
/**
 * @file        str_api.c
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
 
#include "str_api.h"
#include <string.h>

/**
 * 查找字符串中符合目标字符集的第一个字符位置
 * @param *str  待查找的字符串
 * @param *charset  目标字符集
 * @param max_len   最大查找长度
 * @return >= 0: 目标字符位置
 *        -1: 未找到
 * @note:xxxxx
 */
int StrFindChar(const char *str, const char *charset, int max_len)
{
    int i = 0;

    if (max_len >= 0)
    {
        while (i < max_len && str[i] != '\0')
        {
            if (strchr(charset, str[i]) != 0)
                return i;
            i++;
        }
    }
    else
    {
        while (str[i] != '\0')
        {
            if (strchr(charset, str[i]) != 0)
                return i;
            i++;
        }
    }
    
    return -1;
}


/**
 * 字符串转数字值
 * @param *in_str       字符串
 * @param in_str_len    字符串长度
 * @param *out_val      输出值
 * @return 消耗字符串长度
 * @note:xxxxx
 */
unsigned int StrToUInt32(const char *in_str, int in_str_len, unsigned int *out_val)
{
    char *str = (char *)in_str;
    char *str_end = 0;
    unsigned int val = 0;
    unsigned char d;
    const unsigned int kUintMaxDiv10 = 0xffffffffu / 10u;
    const unsigned int kUintMaxCmp   = kUintMaxDiv10 * 10u;

    if (in_str_len < 0)
        str_end = str + strlen(in_str);
    else
        str_end = str + in_str_len;

    while (str < str_end)
    {
        d = *str - '0';
        if (d <= 9u)
        {
            str++;
            val = d + val * 10;
            if (val >= kUintMaxDiv10)
                break;
        }
        else
            break;
    }

    if (val == kUintMaxDiv10 && str < str_end)
    {
        d = *str - '0';
        if (d < '6')
        {
            val = d + kUintMaxCmp;
            str++;
        }
    }

    if (out_val != 0)
        *out_val = val;

    return str - (char *)in_str;
}



/**
 * 字符串比较
 * @param *str1 字符串1
 * @param *str2 字符串2
 * @return True 相同
 *         False 不相同
 */
int StrCmp(const char *str1, const char *str2)
{
    const char *pos1 = str1;
    const char *pos2 = str2;

    while (*pos1 != *pos2 && *pos1 != '\0')
    {
        pos1++;
        pos2++;
    }

    return *pos1 == '\0' && *pos2 == '\0';
}


/**
 * 单字节转换为Hex字符串
 * @param data  单字节数据
 * @param buff  输出缓冲区
 * @return == 0: 转换失败
 *        != 0: 转换成功
 */
char *UInt8ToHexStr(unsigned char data, char buff[3]) 
{
    // 确保缓冲区足够大，（2 个十六进制字符）
    if (buff == 0)
        return 0;

    // 十六进制字符映射表
    const char hex_chars[16] = "0123456789ABCDEF";

    // 计算高位和低位的十六进制字符
    buff[0] = hex_chars[(data >> 4) & 0x0F];
    buff[1] = hex_chars[data & 0x0F];
    buff[2] = 0;

    return buff;
}



static signed char HexCharToNibble(char c) 
{
    
    signed char result = c - '0';
    
    if ((unsigned)result > 9u)
    {
        result = (c | 0x20) - 'a';
        
        if ((unsigned)result <= ('f' - 'a'))
            result += 10;
        else
            result = -1;
    }
    
    return result;
}

int StrHexToBytes(const char *hex_str, char *buff, int buff_size)
{
    char *src_pos = (char*)hex_str;
    char *dst_pos = buff;
    char *dst_end = dst_pos + buff_size;
    signed char h = 0;
    signed char l = 0;
    
    while (dst_pos < dst_end
           && src_pos[0] != '\0'
           && src_pos[1] != '\0')
    {
        h = HexCharToNibble(src_pos[0]);
        l = HexCharToNibble(src_pos[1]);
        
        if (h < 0 || l < 0)
        {
            break;
        }
        *dst_pos = (h << 4) | l;
        
        dst_pos++;
        src_pos += 2;
    }
    
    return dst_pos - buff;  // 返回转换成功的字节长度
}
