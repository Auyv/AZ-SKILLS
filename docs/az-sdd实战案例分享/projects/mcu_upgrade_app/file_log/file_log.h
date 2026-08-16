/**
 * @addtogroup file_log
 * file_log
 * @{
 */
/**
 * @file        file_log.h
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

#ifndef _FILE_LOG_H_
#define _FILE_LOG_H_


#ifdef __cplusplus
extern "C"
{
#endif



typedef struct TFileLogCfgTag TFileLogCfg;
struct TFileLogCfgTag
{
    char *file_dir;  ///< 日志文件目录
    char *file_tag;  ///< 日志文件名
    int max_size;  ///< 日志文件最大大小
    int max_count;  ///< 日志文件最大个数
    int ram_cache_size;  ///< RAM缓存大小
    char *ram_cache;  ///< RAM缓存指针
};

int  FileLogInit(const TFileLogCfg *cfg);  ///< 初始化文件日志系统
void FileLogDone();  ///< 结束文件日志

void FileLogWrite(const char *msg, int len);  ///< 写入日志
void FileLogFlush();  ///< 刷新日志


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _FILE_LOG_H_


