/**
 * @file        sys_shell_cmd_crypto.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/05/24
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/05/24   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 

#include "srt/srt.h"
#include "srt_pid_type.h"
#include "mfc/mfc_string.h"
#include "shell/assist_shell.h"
#include "rte_rfop_client.h"
#include "rfop_client/rfop_common.h"
#include <string.h>
#include <stdio.h>
#include "sfc/sfc_api.h"

#define Printf AssistShellLogOut

#if (0)
static char *L_FormatHexU8Str(UInt8 data)
{
    static char buff[4] = {0};
    
    if (data <= 0x0fu)
    {
        snprintf(buff, sizeof(buff), "0%x", data);
    }
    else
    {
        snprintf(buff, sizeof(buff), "%x", data);
    }

    return buff;
}
#endif


/**
 * 字符串比较
 * @param *str1 字符串1
 * @param *str2 字符串2
 * @return True 相同
 *         False 不相同
 */
static Bool L_StringCmp(const char *str1, const char *str2)
{
    char *pos1 = (char *)str1;
    char *pos2 = (char *)str2;

    while (*pos1 == *pos2 && *pos1 != '\0')
    {
        pos1++;
        pos2++;
    }

    return *pos1 == '\0' && *pos2 == '\0';
}

#define ONCE_READ_SIZE 512
static char g_buff_[ONCE_READ_SIZE + 1] = {0};

static void L_Read(UInt8 *filename)
{
    UInt32 duration_tick = SfGetTickCount();
    Int32 ret = RteRfopCltFileOpen((const char *)filename, kRfopFomRead);

    if (ret == kRfopHccSuccess)
    {
        Printf("Open Success, filename:%s\r\n", filename);
    }
    else
    {
        Printf("Open Fail, ret:%d, filename:%s\r\n", ret, filename);
        return;
    }

    Int32 fd = ret;
    Int32 data_cnt = 0;
    Int32 file_size = 0;
    
    file_size = RteRfopCltFileSeek(fd, 0, kRfopSeekWhenceEnd);
    ret = RteRfopCltFileSeek(fd, 0, kRfopSeekWhenceSet);
    if (ret == kRfopHccSuccess)
    {
        Printf("Seek Success, filename:%s , size:%d\r\n", filename, file_size);
    }
    else
    {
        Printf("Seek Fail, ret:%d, \r\n", ret);
        return;
    }
    
    do
    {
        ret = RteRfopCltFileRead(fd, g_buff_, ONCE_READ_SIZE);
        if (ret > 0)
        {
            data_cnt += ret;
            g_buff_[ret] = '\0';
//            Printf("\r\nret:%d, data:%s\r\n", ret, g_buff_);
//            Printf("\r\nret:%d\r\n", ret);
            Printf("\r\nread cnt: %d / %d = %d%\r\n", data_cnt, file_size, (100 * data_cnt / file_size));
        }
    } while (ret > 0);
    
    duration_tick = SfGetTickCount() - duration_tick;
    SfSleep(500);
    
    Printf("\r\nRead last_ret:%d, data_cnt:%dBytes\r\n", ret, data_cnt);
    
    ret = RteRfopCltFileClose(fd);
    if (ret == kRfopHccSuccess)
    {
        Printf("Close Success\r\n");
    }
    else
    {
        Printf("Close Fail, ret:%d\r\n", ret);
    }
    
    
    Printf("\r\nDoRead %s Operation duration %u ms\r\n", filename, duration_tick);
}

extern void RteRfopUpgrade(char *filename, char *md5);

void L_Upgrade(char *filename, char *md5)
{
    
    RteRfopUpgrade(filename, md5);
}


void SysShellCmdRfop(char argc, char *argv)
{
    char *opt = NULL;

    if (argc >= 2)
        opt = (char *)&argv[(Int32)argv[1]];

    if (opt != NULL)
    {
        // Int32 opt_len = strlen(opt);

        if (L_StringCmp(opt, "-h"))
            Printf("Sorry, have not yet!!! \r\n");
        else if (L_StringCmp(opt, "read"))   // 读取文件
        {
            if (argc >= 3)
            {
                char *param = (char *)&argv[(Int32)argv[2]];
                L_Read((UInt8 *)param);
            }
            else
            {
                Printf("Params Error\r\n");
            }
        }
        else if (L_StringCmp(opt, "upgrade"))
        {
            if (argc >= 4)
            {
                char *filename = (char *)&argv[(Int32)argv[2]];
                char *md5 = (char *)&argv[(Int32)argv[3]];
                L_Upgrade(filename, md5);
            }
            else
            {
                Printf("Params Error\r\n");
            }
        }
        else
        {
            Printf("Params Error\r\n");
        }
    }
    else
    {
        Printf("Params Error\r\n");
    }
}




///< Generated on "2024-05-24 17:09:17" by the tool "gen_hq_file.py >> V20231119" 

