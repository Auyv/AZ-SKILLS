#ifndef HTTP_DOWNLOAD_H
#define HTTP_DOWNLOAD_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * @brief HTTP下载选项结构体
 */
typedef struct {
    const char* url;                /**< 下载URL */
    const char* save_path;          /**< 保存文件路径 */
    uint32_t timeout_ms;            /**< 超时时间(毫秒)，0表示无超时 */
    uint32_t buffer_size;           /**< 缓冲区大小，0使用默认值 */
    bool resume_download;           /**< 是否支持断点续传 */
    void (*log_callback)(const char* log_msg);  /**< 日志回调函数 */
} http_download_options_t;

/**
 * @brief HTTP下载进度回调函数
 * @param total_bytes 总字节数
 * @param downloaded_bytes 已下载字节数
 * @return 是否继续下载，true继续，false取消
 */
typedef bool (*http_download_progress_callback_t)(uint64_t total_bytes, uint64_t downloaded_bytes);

/**
 * @brief 初始化HTTP下载器
 * @return 初始化结果，0成功，非0失败
 */
int http_download_init(void);

/**
 * @brief 下载文件
 * @param options 下载选项
 * @param progress_callback 进度回调函数
 * @return 下载结果，0成功，非0失败
 */
int http_download_file(const http_download_options_t* options, 
                       http_download_progress_callback_t progress_callback);

/**
 * @brief 清理HTTP下载器资源
 */
void http_download_cleanup(void);

/**
 * @brief 获取错误信息
 * @return 错误信息字符串
 */
const char* http_download_get_error(void);

#endif // HTTP_DOWNLOAD_H