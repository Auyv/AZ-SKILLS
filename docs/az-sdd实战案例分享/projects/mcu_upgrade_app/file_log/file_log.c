/**
 * @file        file_log.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/07/29
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/07/29   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "file_log.h"
#include <ofc_object.h>
#include "sfc/sfc_mutex.h"
#include "sfc/sfc_api.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>



#define FILE_LOG_DIR            g_file_log_.cfg->file_dir   // "./log"  ///< 日志文件目录
#define FILE_LOG_IDX_FILE_NAME  g_file_log_.cfg->file_tag   ///< 日志索引文件名
#define FILE_LOG_TAG_FILE_NAME  g_file_log_.cfg->file_tag   ///< 日志标签文件名. 完整日志名称为: %FILE_LOG_TAG_FILE_NAME%_<index>.log
#define FILE_LOG_MAX_SIZE       g_file_log_.cfg->max_size  ///< 日志文件最大大小,1M
#define FILE_LOG_MAX_COUNT      g_file_log_.cfg->max_count  ///< 日志文件最大个数

#define FILE_LOG_RAM_CACHE_SIZE (1024 * 4)  ///< 日志文件缓存大小,4K


typedef struct TFileLogTag TFileLog;
struct TFileLogTag
{
    Bool is_init_;  ///< 是否初始化
    UInt8 file_idx;
    FILE *file_handle;

    char *cache_pos;
    char *cache_end;

    const TFileLogCfg *cfg;

    TSfCriticalSection locker_;
};

static TFileLog g_file_log_ = {0};

static Bool L_WriteFileIdx(UInt8 file_idx)
{
    Bool result = False;
    char file_name[256] = {0};
    snprintf(file_name, sizeof(file_name) - 1, "%s/%s.idx", FILE_LOG_DIR, FILE_LOG_IDX_FILE_NAME);

    FILE *file_handle = fopen(file_name, "wb");
    
    if (file_handle != NULL)
    {
        char data[11] = {0};
        snprintf(data, sizeof(data) - 1, "%u", file_idx);

        fwrite(data, strlen(data), 1, file_handle);
        fclose(file_handle);
        printf("L_WriteFileIdx: %s >> %s\n", file_name, data);
        result = True;
    }

    return result;
}

static UInt8 L_ReadFileIdx()
{
    UInt8 result = 0;
    char file_name[256] = {0};
    snprintf(file_name, sizeof(file_name) - 1, "%s/%s.idx", FILE_LOG_DIR, FILE_LOG_IDX_FILE_NAME);

    FILE *file_handle = fopen(file_name, "rb");
    
    if (file_handle != NULL)
    {
        char data[11] = {0};
        char *p = data;
        int len = fread(data, sizeof(data) - 1, 1, file_handle);
        fclose(file_handle);

        result = strtoul(data, &p, 10);

        if (p <= data)
        {
            result = 0;
            L_WriteFileIdx(result);  ///< 如果文件不存在, 则创建并写入初始值
            printf("L_ReadFileIdx: %s Content is invalid, create with index 0\n", file_name);
        }

        printf("L_ReadFileIdx: %s >> %d\n", file_name, result);
    }
    else
    {
        L_WriteFileIdx(result);  ///< 如果文件不存在, 则创建并写入初始值
        printf("L_ReadFileIdx: %s not found, create with index 0\n", file_name);
    }

    return result;
}



static FILE *L_OpenFile(UInt8 file_idx, Bool new_file)
{
    FILE *result = NULL;
    char file_name[256] = {0};

    snprintf(file_name, sizeof(file_name) - 1, "%s/%s_%u.log", FILE_LOG_DIR, FILE_LOG_TAG_FILE_NAME, file_idx);


    if (new_file)
    {
        result = fopen(file_name, "wb");
    }
    else
    {
        result = fopen(file_name, "ab");
    }

    return result;
}

static void L_CloseFile(FILE *file_handle)
{
    if (file_handle != NULL)
    {
        fclose(file_handle);
    }
}




int  FileLogInit(const TFileLogCfg *cfg)
{
    int result = -1;
    
    if (cfg != NULL)
    {
        if (!g_file_log_.is_init_)  ///< 文件日志已经初始化
        {
            Bool allow = True;

            g_file_log_.cfg = cfg;
            
            // 文件不存在创建文件目录
            if (access(FILE_LOG_DIR, F_OK) == -1)
            {
                if (mkdir(FILE_LOG_DIR, 0777) == -1) {
                    perror("mkdir failed");
                    allow = False;
                }
            }
            
            if (allow)
            {
                g_file_log_.cache_pos = cfg->ram_cache;
                g_file_log_.cache_end = g_file_log_.cache_pos + FILE_LOG_RAM_CACHE_SIZE;
                
                g_file_log_.file_idx = L_ReadFileIdx();
                
                g_file_log_.file_handle = L_OpenFile(g_file_log_.file_idx, False);
                
                if (TSfCriticalSectionCreate(&g_file_log_.locker_))
                    g_file_log_.is_init_ = True;
            }
        }

        result = g_file_log_.is_init_ ? 1 : -2;
    }

    return result;
}

void FileLogDone()  ///< 结束文件日志
{
    if (g_file_log_.is_init_)
    {
        FileLogFlush();
        L_CloseFile(g_file_log_.file_handle);
        g_file_log_.is_init_ = False;
    }
}

static void L_FileFlush()  ///< 刷新日志
{
    // 将日志写入文件
    if (g_file_log_.file_handle != NULL && g_file_log_.cache_pos > g_file_log_.cfg->ram_cache)
    {
        fwrite(g_file_log_.cfg->ram_cache, g_file_log_.cache_pos - g_file_log_.cfg->ram_cache, 1, g_file_log_.file_handle);

        long current_pos = ftell(g_file_log_.file_handle);
        if (current_pos >= FILE_LOG_MAX_SIZE)
        {
            L_CloseFile(g_file_log_.file_handle);

            g_file_log_.file_idx++;
            if (g_file_log_.file_idx >= FILE_LOG_MAX_COUNT)
                g_file_log_.file_idx = 0;

            L_WriteFileIdx(g_file_log_.file_idx);
            g_file_log_.file_handle = L_OpenFile(g_file_log_.file_idx, True);
        }

        g_file_log_.cache_pos = g_file_log_.cfg->ram_cache;
    }
}


static void L_FileWrite(const char *msg, Int32 len)  ///< 写入日志
{
    // 写入缓存
    Int32 write_once = len;
    Int32 write_able = g_file_log_.cache_end - g_file_log_.cache_pos;
    
    if (write_once < write_able)
    {
        memcpy(g_file_log_.cache_pos, msg, write_once);
        g_file_log_.cache_pos += write_once;
    }
    else if (write_once == write_able)
    {
        memcpy(g_file_log_.cache_pos, msg, write_once);
        g_file_log_.cache_pos += write_once;
        L_FileFlush();  ///< 刷新日志
    }
    else
    {
        memcpy(g_file_log_.cache_pos, msg, write_able);
        g_file_log_.cache_pos += write_able;
        L_FileFlush();  ///< 刷新日志

        L_FileWrite(&msg[write_able], len - write_able);
    }
}



void FileLogWrite(const char *msg, int len)
{   
    if (g_file_log_.is_init_ && TSfCriticalSectionEnter(&g_file_log_.locker_))
    {
        L_FileWrite(msg, len);
        TSfCriticalSectionLeave(&g_file_log_.locker_);
    }

}

void FileLogFlush()  ///< 刷新日志
{
    if (g_file_log_.is_init_ && TSfCriticalSectionEnter(&g_file_log_.locker_))
    {
        // 写入日志
        L_FileFlush();
        TSfCriticalSectionLeave(&g_file_log_.locker_);

        fflush(g_file_log_.file_handle);
    }
}


