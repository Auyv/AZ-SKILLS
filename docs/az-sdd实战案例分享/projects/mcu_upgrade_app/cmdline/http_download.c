#include "http_download.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>


#define _LINUX

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// #ifdef _EMBEDDED
// // 嵌入式平台特定的优化设置
// #define MEMORY_OPTIMIZED 1
// // 可以在这里添加嵌入式平台特有的头文件和定义
// #endif

// #ifdef _ARM
// // ARM平台特定的优化或处理
// #endif

// #ifdef _RISCV
// // RISC-V平台特定的优化或处理
// #endif

#ifdef _LINUX
// Linux平台特定的优化或处理
#endif





// 错误信息缓冲区
static char g_error_msg[1024] = {0};

// 默认端口号
#define DEFAULT_HTTP_PORT 80

// 默认缓冲区大小
#define DEFAULT_BUFFER_SIZE 8192

// 下载上下文结构体
typedef struct {
    FILE* file;                     /**< 文件句柄 */
    uint64_t file_size;             /**< 文件大小 */
    uint64_t downloaded_bytes;      /**< 已下载字节数 */
    http_download_progress_callback_t progress_callback;  /**< 进度回调 */
    void (*log_callback)(const char* log_msg);  /**< 日志回调 */
    uint32_t buffer_size;           /**< 缓冲区大小 */
    uint32_t timeout_ms;            /**< 超时时间 */
    SOCKET socket;                  /**< socket句柄 */
    bool canceled;                  /**< 是否已取消 */
} download_context_t;

// URL解析结构体
typedef struct {
    char host[256];                 /**< 主机名 */
    int port;                       /**< 端口号 */
    char path[1024];                /**< 路径 */
} url_info_t;

/**
 * @brief 设置错误信息
 * @param format 格式化字符串
 * @param ... 可变参数
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_msg, sizeof(g_error_msg) - 1, format, args);
    va_end(args);
}

/**
 * @brief 默认日志输出函数
 * @param log_msg 日志信息
 */
static void default_log_callback(const char* log_msg) {
    printf("[HTTP_DOWNLOAD] %s\n", log_msg);
}

/**
 * @brief 确保目录存在
 * @param path 目录路径
 * @return 是否成功，true成功，false失败
 */
static bool ensure_directory_exists(const char* path) {
#ifdef _WIN32
    // 使用Windows API创建目录
    if (!CreateDirectoryA(path, NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            set_error("Failed to create directory: %s, error code: %d", path, error);
            return false;
        }
    }
    return true;
#else
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) {
            set_error("Failed to create directory: %s, error: %s", path, strerror(errno));
            return false;
        }
    }
    return true;
#endif
}

/**
 * @brief 解析URL
 * @param url URL字符串
 * @param info 解析结果
 * @return 是否成功，true成功，false失败
 */
static bool parse_url(const char* url, url_info_t* info) {
    if (!url || !info) {
        set_error("Invalid URL or info pointer");
        return false;
    }

    // 检查URL前缀
    const char* http_prefix = "http://";
    if (strncmp(url, http_prefix, strlen(http_prefix)) != 0) {
        set_error("Only HTTP protocol is supported");
        return false;
    }

    // 跳过http://前缀
    const char* host_start = url + strlen(http_prefix);
    const char* path_start = strchr(host_start, '/');
    const char* port_start = strchr(host_start, ':');

    // 提取主机名和端口号
    if (path_start) {
        if (port_start && port_start < path_start) {
            // 有端口号
            int host_len = port_start - host_start;
            if (host_len > sizeof(info->host) - 1) {
                set_error("Host name too long");
                return false;
            }
            strncpy(info->host, host_start, host_len);
            info->host[host_len] = '\0';
            
            // 提取端口号
            info->port = atoi(port_start + 1);
        } else {
            // 无端口号，使用默认端口
            int host_len = path_start - host_start;
            if (host_len > sizeof(info->host) - 1) {
                set_error("Host name too long");
                return false;
            }
            strncpy(info->host, host_start, host_len);
            info->host[host_len] = '\0';
            info->port = DEFAULT_HTTP_PORT;
        }
        
        // 提取路径
        int path_len = strlen(path_start);
        if (path_len > sizeof(info->path) - 1) {
            set_error("Path too long");
            return false;
        }
        strncpy(info->path, path_start, path_len);
        info->path[path_len] = '\0';
    } else {
        // 没有路径，使用默认路径/
        if (port_start) {
            // 有端口号
            int host_len = port_start - host_start;
            if (host_len > sizeof(info->host) - 1) {
                set_error("Host name too long");
                return false;
            }
            strncpy(info->host, host_start, host_len);
            info->host[host_len] = '\0';
            info->port = atoi(port_start + 1);
        } else {
            // 无端口号
            int host_len = strlen(host_start);
            if (host_len > sizeof(info->host) - 1) {
                set_error("Host name too long");
                return false;
            }
            strncpy(info->host, host_start, host_len);
            info->host[host_len] = '\0';
            info->port = DEFAULT_HTTP_PORT;
        }
        strcpy(info->path, "/");
    }

    return true;
}

/**
 * @brief 设置socket超时
 * @param socket socket句柄
 * @param timeout_ms 超时时间(毫秒)
 * @return 是否成功，true成功，false失败
 */
static bool set_socket_timeout(SOCKET socket, uint32_t timeout_ms) {
#ifdef _WIN32
    int timeout = timeout_ms;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) != 0) {
        set_error("Failed to set socket receive timeout: %d", WSAGetLastError());
        return false;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) != 0) {
        set_error("Failed to set socket send timeout: %d", WSAGetLastError());
        return false;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) != 0) {
        set_error("Failed to set socket receive timeout: %s", strerror(errno));
        return false;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) != 0) {
        set_error("Failed to set socket send timeout: %s", strerror(errno));
        return false;
    }
#endif
    return true;
}

/**
 * @brief 连接到服务器
 * @param host 主机名
 * @param port 端口号
 * @param timeout_ms 超时时间
 * @return socket句柄，失败返回INVALID_SOCKET
 */
static SOCKET connect_to_server(const char* host, int port, uint32_t timeout_ms) {
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server_addr;
    struct hostent* server;

    // 创建socket
#ifdef _WIN32
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (sock == INVALID_SOCKET) {
#ifdef _WIN32
        set_error("Failed to create socket: %d", WSAGetLastError());
#else
        set_error("Failed to create socket: %s", strerror(errno));
#endif
        return INVALID_SOCKET;
    }

    // 设置超时
    if (timeout_ms > 0 && !set_socket_timeout(sock, timeout_ms)) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // 解析主机名
    server = gethostbyname(host);
    if (server == NULL) {
#ifdef _WIN32
        set_error("Failed to resolve host: %d", WSAGetLastError());
#else
        set_error("Failed to resolve host: %s", strerror(errno));
#endif
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    // 连接服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        set_error("Failed to connect to server: %d", WSAGetLastError());
#else
        set_error("Failed to connect to server: %s", strerror(errno));
#endif
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

/**
 * @brief 发送HTTP请求
 * @param socket socket句柄
 * @param url_info URL信息
 * @param resume_from 断点续传起始位置
 * @return 是否成功，true成功，false失败
 */
static bool send_http_request(SOCKET socket, const url_info_t* url_info, uint64_t resume_from) {
    char request[2048];
    int request_len;

    if (resume_from > 0) {
        // 断点续传请求
        request_len = snprintf(request, sizeof(request) - 1,
            "GET %s HTTP/1.1\r\n" \
            "Host: %s:%d\r\n" \
            "Range: bytes=%llu-\r\n" \
            "User-Agent: HTTP-Downloader/1.0\r\n" \
            "Connection: close\r\n" \
            "\r\n",
            url_info->path, url_info->host, url_info->port, resume_from);
    } else {
        // 普通请求
        request_len = snprintf(request, sizeof(request) - 1,
            "GET %s HTTP/1.1\r\n" \
            "Host: %s:%d\r\n" \
            "User-Agent: HTTP-Downloader/1.0\r\n" \
            "Connection: close\r\n" \
            "\r\n",
            url_info->path, url_info->host, url_info->port);
    }

    // 发送请求
    int sent = send(socket, request, request_len, 0);
    if (sent == SOCKET_ERROR || sent != request_len) {
#ifdef _WIN32
        set_error("Failed to send HTTP request: %d", WSAGetLastError());
#else
        set_error("Failed to send HTTP request: %s", strerror(errno));
#endif
        return false;
    }

    return true;
}

/**
 * @brief 解析HTTP响应头
 * @param socket socket句柄
 * @param context 下载上下文
 * @param is_resume 是否是断点续传
 * @return 是否成功，true成功，false失败
 */
static bool parse_http_response(SOCKET socket, download_context_t* context, bool is_resume) {
    char buffer[DEFAULT_BUFFER_SIZE];
    char* header_end;
    int bytes_read = 0;
    int total_read = 0;
    bool header_complete = false;

    // 读取HTTP响应头
    while (!header_complete) {
        bytes_read = recv(socket, buffer + total_read, sizeof(buffer) - 1 - total_read, 0);
        if (bytes_read == SOCKET_ERROR) {
#ifdef _WIN32
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) {
                set_error("Socket timeout while reading response header");
            } else {
                set_error("Failed to read response header: %d", error_code);
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                set_error("Socket timeout while reading response header");
            } else {
                set_error("Failed to read response header: %s", strerror(errno));
            }
#endif
            return false;
        }
        if (bytes_read == 0) {
            set_error("Connection closed by server");
            return false;
        }

        total_read += bytes_read;
        buffer[total_read] = '\0';

        // 查找响应头结束标记\r\n\r\n
        header_end = strstr(buffer, "\r\n\r\n");
        if (header_end) {
            header_complete = true;
            header_end += 4; // 跳过\r\n\r\n
            // 检查HTTP状态码
            if (strstr(buffer, "HTTP/1.1 200") == NULL && 
                strstr(buffer, "HTTP/1.0 200") == NULL &&
                (is_resume ? 
                    (strstr(buffer, "HTTP/1.1 206") == NULL && 
                     strstr(buffer, "HTTP/1.0 206") == NULL) : false)) {
                set_error("Invalid HTTP response status code");
                return false;
            }

            // 解析Content-Length
            char* content_length = strstr(buffer, "Content-Length:");
            if (content_length) {
                content_length += strlen("Content-Length:");
                while (*content_length == ' ') content_length++;
                context->file_size = atoll(content_length);
                if (is_resume) {
                    context->file_size += context->downloaded_bytes;
                }
            } else if (is_resume) {
                // 断点续传时需要Content-Length
                set_error("Server does not support Range requests");
                return false;
            }

            // 保存已经读取但不属于头的部分数据
            int body_data_len = total_read - (header_end - buffer);
            if (body_data_len > 0) {
                size_t written = fwrite(header_end, 1, body_data_len, context->file);
                if (written != body_data_len) {
                    set_error("Failed to write data to file");
                    return false;
                }
                context->downloaded_bytes += written;
            }
        }
    }

    return true;
}

/**
 * @brief 下载文件数据
 * @param context 下载上下文
 * @return 是否成功，true成功，false失败
 */
static bool download_file_data(download_context_t* context) {
    char* buffer = NULL;
    
#ifdef MEMORY_OPTIMIZED
    // 嵌入式平台内存优化：限制最大缓冲区大小
    #define MAX_EMBEDDED_BUFFER_SIZE 8192
    uint32_t actual_buffer_size = context->buffer_size;
    if (actual_buffer_size > MAX_EMBEDDED_BUFFER_SIZE) {
        actual_buffer_size = MAX_EMBEDDED_BUFFER_SIZE;
    }
    
    // 如果支持栈分配且缓冲区较小，使用栈内存以避免堆碎片
    #define MAX_STACK_BUFFER_SIZE 4096
    if (actual_buffer_size <= MAX_STACK_BUFFER_SIZE) {
        static char stack_buffer[MAX_STACK_BUFFER_SIZE];
        buffer = stack_buffer;
    } else {
        // 较大的缓冲区仍然使用堆分配
        buffer = (char*)malloc(actual_buffer_size);
    }
#else
    // 非嵌入式平台直接分配请求的缓冲区大小
    buffer = (char*)malloc(context->buffer_size);
#endif
    
    if (!buffer) {
        set_error("Failed to allocate memory for buffer");
        return false;
    }
    
#ifdef MEMORY_OPTIMIZED
    // 使用优化后的缓冲区大小
    uint32_t buffer_size = actual_buffer_size;
#else
    uint32_t buffer_size = context->buffer_size;
#endif

    int bytes_read;
    while (!context->canceled) {
        bytes_read = recv(context->socket, buffer, buffer_size, 0);
        if (bytes_read == SOCKET_ERROR) {
#ifdef _WIN32
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) {
                set_error("Socket timeout while downloading data");
            } else {
                set_error("Failed to read data: %d", error_code);
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                set_error("Socket timeout while downloading data");
            } else {
                set_error("Failed to read data: %s", strerror(errno));
            }
#endif
#ifndef MEMORY_OPTIMIZED
            // 仅在非优化模式下释放内存（优化模式下可能使用栈内存）
            free(buffer);
#endif
            return false;
        }

        if (bytes_read == 0) {
            // 连接关闭，下载完成
            break;
        }

        // 写入文件
        size_t written = fwrite(buffer, 1, bytes_read, context->file);
        if (written != bytes_read) {
            set_error("Failed to write data to file");
            free(buffer);
            return false;
        }

        // 更新下载字节数
        context->downloaded_bytes += written;

        // 记录进度日志
        if (context->log_callback && context->file_size > 0) {
#ifdef MEMORY_OPTIMIZED
            // 嵌入式平台：减少日志频率，每5%进度更新一次，减少字符串操作开销
            static uint32_t last_percent = 0;
            uint32_t percent = (context->downloaded_bytes * 100) / context->file_size;
            if (percent >= last_percent + 5 || percent == 100) {
                // 使用固定格式和较小的缓冲区
                char log_msg[128];
                snprintf(log_msg, sizeof(log_msg), "DL: %u%% (%llu/%llu)",
                         percent, context->downloaded_bytes, context->file_size);
                context->log_callback(log_msg);
                last_percent = percent;
            }
#else
            // 非嵌入式平台使用更详细的日志
            static uint32_t last_percent = 0;
            uint32_t percent = (context->downloaded_bytes * 100) / context->file_size;
            if (percent - last_percent>= 15u || percent >= 100) {
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Downloading... %.2f%% (%llu/%llu bytes)",
                         (double)percent, context->downloaded_bytes, context->file_size);
                context->log_callback(log_msg);
                last_percent = percent;
            }
#endif
        }

        // 调用进度回调
        if (context->progress_callback) {
            if (!context->progress_callback(context->file_size, context->downloaded_bytes)) {
                // 用户取消下载
                context->canceled = true;
                set_error("Download canceled by user");
                free(buffer);
                return false;
            }
        }
    }

    free(buffer);
    return !context->canceled;
}

int http_download_init(void) {
#ifdef _WIN32
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        set_error("Failed to initialize Winsock: %d", WSAGetLastError());
        return -1;
    }
#endif
    return 0;
}

int http_download_file(const http_download_options_t* options, 
                       http_download_progress_callback_t progress_callback) {
    if (!options || !options->url || !options->save_path) {
        set_error("Invalid parameters");
        return -1;
    }

    // 解析URL
    url_info_t url_info;
    if (!parse_url(options->url, &url_info)) {
        return -1;
    }

    // 确保保存目录存在
    char save_dir[1024];
    strncpy(save_dir, options->save_path, sizeof(save_dir) - 1);
    char* last_slash = strrchr(save_dir, '/');
    if (!last_slash) {
        last_slash = strrchr(save_dir, '\\');
    }
    if (last_slash) {
        *last_slash = '\0';
        if (!ensure_directory_exists(save_dir)) {
            return -1;
        }
    }

    // 检查文件是否存在，用于断点续传
    uint64_t resume_from = 0;
    bool is_resume = false;
    if (options->resume_download) {
        struct stat st = {0};
        if (stat(options->save_path, &st) == 0) {
            resume_from = st.st_size;
            if (resume_from > 0) {
                is_resume = true;
                if (options->log_callback) {
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Found existing file, resuming from %llu bytes", resume_from);
                    options->log_callback(log_msg);
                }
            }
        }
    }

    // 打开文件，追加模式用于断点续传
#ifdef MEMORY_OPTIMIZED
    // 嵌入式平台：设置较小的文件缓冲区以减少内存使用
    FILE* file = fopen(options->save_path, is_resume ? "ab" : "wb");
    if (file) {
        // 设置文件缓冲大小为1KB，减少内存占用
        setvbuf(file, NULL, _IOFBF, 1024);
    }
#else
    // 非嵌入式平台使用默认缓冲设置
    FILE* file = fopen(options->save_path, is_resume ? "ab" : "wb");
#endif
    
    if (!file) {
        set_error("Failed to open file: %s, error: %s", options->save_path, strerror(errno));
        return -1;
    }

    // 创建下载上下文
    download_context_t context = {
        .file = file,
        .file_size = 0,
        .downloaded_bytes = resume_from,
        .progress_callback = progress_callback,
        .log_callback = options->log_callback ? options->log_callback : default_log_callback,
        .buffer_size = options->buffer_size > 0 ? options->buffer_size : DEFAULT_BUFFER_SIZE,
        .timeout_ms = options->timeout_ms,
        .socket = INVALID_SOCKET,
        .canceled = false
    };

    // 连接到服务器
    context.socket = connect_to_server(url_info.host, url_info.port, options->timeout_ms);
    if (context.socket == INVALID_SOCKET) {
        fclose(file);
        return -1;
    }

    // 发送HTTP请求
    if (!send_http_request(context.socket, &url_info, resume_from)) {
        closesocket(context.socket);
        fclose(file);
        return -1;
    }

    // 解析HTTP响应头
    if (!parse_http_response(context.socket, &context, is_resume)) {
        closesocket(context.socket);
        fclose(file);
        return -1;
    }

    // 下载文件数据
    bool success = download_file_data(&context);

    // 关闭socket和文件
    closesocket(context.socket);
    fclose(file);

    // 检查下载结果
    if (!success) {
        return -1;
    }

    // 下载成功
    if (options->log_callback) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Download completed successfully, saved to: %s", options->save_path);
        options->log_callback(log_msg);
    }

    return 0;
}

void http_download_cleanup(void) {
#ifdef _WIN32
    // 清理Winsock
    WSACleanup();
#endif
}

const char* http_download_get_error(void) {
    return g_error_msg;
}
