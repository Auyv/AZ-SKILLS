/**
 * @file        a_log_backend_file_impl.h.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/23
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#include "a_log_backend_file_impl.h"
#include "sfc/sfc_api.h"
#include "ofc/ofc_time_codec.h"
#include "ffc/ffc_file_utils.h"
#include "ffc/ffc_file.h"
#include "file_log.h"
#include "upgrade_app_cfg.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>


static char g_file_log_cache_buf[1024 * 5] = {0};
static TFileLogCfg g_file_log_cfg = {0};

// static void L_DicInfoSetOldestName(TDicInfo *info, const char *filename)
// {
//     strncpy(info->oldest_name, filename, sizeof(info->oldest_name));
// }

// static void L_DicInfoSetLatestName(TDicInfo *info, const char *filename)
// {
//     strncpy(info->latest_name, filename, sizeof(info->latest_name));
// }

static void L_WriteFileOpenTime()
{
    char log[128] = {0};

    TTime curr_tick = SfGetSystemTime() + 8 * 60 * 60;
    Word year;
    Word month;
    Word day;
    Word hour;
    Word minute;
    Word second;
    OfDecodeDateTime(curr_tick, &year, &month, &day, &hour, &minute, &second);

    Int32 len = snprintf(log, sizeof(log), "====== Log Open Time: %04d-%02d-%02d %02d:%02d:%02d ======\n", year, month, day, hour, minute, second);

    // FfFileWrite(fd, log, len);
    FileLogWrite(log, len);
}

static void L_WriteFileCloseTime()
{
    char log[128] = {0};

    TTime curr_tick = SfGetSystemTime() + 8 * 60 * 60;
    Word year;
    Word month;
    Word day;
    Word hour;
    Word minute;
    Word second;
    OfDecodeDateTime(curr_tick, &year, &month, &day, &hour, &minute, &second);

    Int32 len = snprintf(log, sizeof(log), "====== Log Close Time: %04d-%02d-%02d %02d:%02d:%02d ======\n", year, month, day, hour, minute, second);

    // FfFileWrite(fd, log, len);
    FileLogWrite(log, len);
}

// 生成日志文件名
// static char *L_TAlogBackendFileImplGenerateLogFilename(TAlogBackendFileImpl *self, char *filename, UInt32 size)
// {
//     char *result = NULL;
//     TTime curr_tick = SfGetSystemTime() + 8 * 60 * 60;
//     UInt32 nlen = strlen(self->file_name) + 24; // filename_20241223145347_000.log

//     if (size >= nlen)
//     {
//          Word year;
//          Word month;
//          Word day;
//          Word hour;
//          Word minute;
//          Word second;
//         OfDecodeDateTime(curr_tick, &year, &month, &day, &hour, &minute, &second);

//         snprintf(filename, size, "%s_%04d%02d%02d%02d%02d%02d_%03d.log", self->file_name, year, month, day, hour, minute, second, SfGetTickCount() % 1000);
//         printf("GenerateLogFilename: %s\n", filename);
//         result = filename;
//     }

//     return result;
// }


// static Int32 L_TAlogBackendFileImplFilenameCmp(TAlogBackendFileImpl *self, const char *a, const char *b)
// {
//     UInt32 nlen = strlen(self->file_name) + 24; // filename_20241223145347_000.log
//     return strncmp(a, b, nlen);
// }



// static Bool L_TAlogBackendFileImplFilenameIsValid(TAlogBackendFileImpl *self, const char *filename)
// {
//     Bool result = False;
//     UInt32 nlen = strlen(self->file_name) + 24; // filename_20241223145347_000.log

//     nlen -= 1; // 去掉\0

//     if (strlen(filename) == nlen)
//     {
//         if (strncmp(filename, self->file_name, strlen(self->file_name)) == 0)
//         {
//             // <2024-12-23> Jpz todo : 进一步判断文件格式有效性
//             result = memcmp(self->file_name, filename, strlen(self->file_name)) == 0 ? True : False;
//         }
//     }

//     return result;
// }


// static Int32 L_TAlogBackendFileImplFileCount(TAlogBackendFileImpl *self, TDicInfo *r_dic_info)
// {
//     Int32 result = 0;
//     DIR *d;
//     struct dirent *entry;
//     int file_count = 0;

//     memset(r_dic_info, 0, sizeof(*r_dic_info));

//     // 文件不存在创建文件目录
//     if (access(self->file_dir, F_OK) == -1)
//     {
//         if (mkdir(self->file_dir, 777) == -1) {
//             perror("mkdir failed");
//             return 1;
//         } else {
//             printf("Directory created successfully\n");
//         }
//     }

//     d = opendir(self->file_dir);
//     if (d == NULL)
//     {
//         printf("Failed to open directory\n");
//         perror("opendir");
//         return -1;
//     }

//     r_dic_info->tag_file_cnt = 0;
//     entry = readdir(d);

//     while (entry != NULL)
//     {
//         if (L_TAlogBackendFileImplFilenameIsValid(self, entry->d_name))
//         {
//             printf("Valid Entry: %s\n", entry->d_name);
//             r_dic_info->tag_file_cnt++;

//             if (!r_dic_info->latest_exist)
//             {
//                 r_dic_info->latest_exist = True;
//                 L_DicInfoSetLatestName(r_dic_info, entry->d_name);
//                 printf("D1 latest_name: %s\n", r_dic_info->latest_name);
//             }
//             else if (L_TAlogBackendFileImplFilenameCmp(self, entry->d_name, r_dic_info->latest_name) > 0)
//             {
//                 L_DicInfoSetLatestName(r_dic_info, entry->d_name);
//                 printf("D2 latest_name: %s\n", r_dic_info->latest_name);
//             }
//             else
//             {}

//             if (!r_dic_info->oldest_exist)
//             {
//                 r_dic_info->oldest_exist = True;
//                 L_DicInfoSetOldestName(r_dic_info, entry->d_name);
//                 printf("D3 oldest_name: %s\n", r_dic_info->oldest_name);
//             }
//             else if (L_TAlogBackendFileImplFilenameCmp(self, entry->d_name, r_dic_info->oldest_name) < 0)
//             {
//                 L_DicInfoSetOldestName(r_dic_info, entry->d_name);
//                 printf("D4 oldest_name: %s\n", r_dic_info->oldest_name);
//             }
//             else
//             {}
//         }
//         else
//             printf("Invalid Entry: %s\n", entry->d_name);

//         entry = readdir(d);
//     }

//     // 关闭目录流
//     closedir(d);

//     result = r_dic_info->tag_file_cnt;

//     printf("result:%d\n", result);

//     return result;
// }


static Int32 TAlogBackendFileImplOutput(IALog *sender, UInt32 level, const Int8 *tag, UInt32 ticks, const Int8 *log, Int16 len)
{
    Int32 result = 0;

    if (sender != NULL && log != NULL && len > 0)
    {
        TAlogBackendFileImpl *self = (TAlogBackendFileImpl *)OF_CONTAINER_OF(sender, TAlogBackendFileImpl, alog_interface_);

        if (self->enable_)
        {           
            FileLogWrite(log, len);
            result = len;
        }
    }

    return result;
}

static void TAlogBackendFileImplFlush(IALog *sender)
{
    if (sender != NULL)
    {
        FileLogFlush();
    }
}


void TAlogBackendFileImplCreate(TAlogBackendFileImpl *self, TAlogBackendFileParam *param)
{
    Int32 cnt = 0;

    
    memset(self, 0, sizeof(*self));
    
    // 使用 param 初始化日志配置
    g_file_log_cfg.file_dir = param->file_dir;
    g_file_log_cfg.file_tag = param->file_name;
    g_file_log_cfg.max_size = param->file_size_max;
    g_file_log_cfg.max_count = param->file_count_max;
    g_file_log_cfg.ram_cache = g_file_log_cache_buf;
    g_file_log_cfg.ram_cache_size = sizeof(g_file_log_cache_buf);

    self->alog_interface_.DoBackendOutput = TAlogBackendFileImplOutput;
    self->alog_interface_.DoBackendFlush = TAlogBackendFileImplFlush;
    
    self->enable_ = FileLogInit(&g_file_log_cfg);
    L_WriteFileOpenTime();
}

void TAlogBackendFileImplDestroy(TAlogBackendFileImpl *self)
{
    L_WriteFileCloseTime();

    FileLogDone();
}

IALog *TAlogBackendFileImplInterface(TAlogBackendFileImpl *self)
{
    return &self->alog_interface_;
}


void TAlogBackendFileImplSetEnable(TAlogBackendFileImpl *self, Bool enable)
{
    self->enable_ = enable;
}

Bool TAlogBackendFileImplEnable(TAlogBackendFileImpl *self)
{
    return self->enable_;
}


///< Generated on "2024-12-23 14:10:39" by the tool "gen_hq_file.py >> V20231119"

