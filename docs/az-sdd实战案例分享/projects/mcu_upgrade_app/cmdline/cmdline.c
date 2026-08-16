/**
 * @file        cmdline.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/02/07
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/02/07   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 * 
 * 
 * 
 * hqupgrade命令行处理
 * 
 * 无升级包加密
 * hqupgrade --img /media/card/nfxx_upgrade.img
 * 
 * 有升级包加密
 * hqupgrade --img /media/card/nfxx_upgrade.img --keys-dir /media/card/keys 
 */
 
#include "cmdline.h"
#include "ofc/ofc_types.h"
#include "upgrade_app_cfg.h"
#include "srt/srt.h"
#include "srt_pid_type.h"
#include "nf_upgrade_app.h"
#include "sfc/sfc_api.h"
#include "ffc/ffc_file_utils.h"
#include "se_img.h"

#define ALOG_TAG "cmdline"
#include "alog/a_log.h"



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>  // 用于 struct stat 和 stat()
#include <unistd.h>    // 用于 POSIX 标准函数

typedef struct TCmdParamsTag TCmdParams;
struct TCmdParamsTag
{
    char *url;
    char *img;
    char *keys_dir;
};


#define DOWNLOAD_FILE_BY_CURL 0
#if (DOWNLOAD_FILE_BY_CURL)

#include <curl/curl.h>

/**
 * @brief libcurl写数据回调函数，将接收到的数据写入文件
 * @param ptr 指向接收到数据的指针
 * @param size 每个数据元素的大小
 * @param nmemb 数据元素的数量
 * @param stream 要写入的文件流
 * @return 实际写入的数据元素数量
 */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    if (written != nmemb) {
        // 如果未能完全写入，则返回0表示写入失败
        return 0;
    }
    return written;
}

/**
 * @brief 检查服务器是否支持断点续传
 * @param url 要检查的URL地址
 * @return 支持返回1，不支持返回0，出错返回-1
 */
static int check_server_supports_resume(const char *url)
{
    CURL *curl = NULL;
    CURLcode res;
    long response_code = 0;

    ALOG_D("debug");
    
    /* 参数校验 */
    if (!url) {
        return -1;
    }
    
    ALOG_D("debug");

    /* 初始化curl句柄 */
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    ALOG_D("debug");
    
    /* 设置curl选项 */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);          // 只请求头部信息
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);          // 包含头部
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-0");        // 请求第一个字节范围
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);     // 连接超时 30s
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);            // 总超时 30s
    ALOG_D("debug");

    /* 执行请求 */
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        char *error_buffer = NULL;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &error_buffer);
        ALOG_E("curl_easy_perform failed: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }
    ALOG_D("debug");
    
    /* 获取响应码 */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    ALOG_D("debug");
    curl_easy_cleanup(curl);
    
    /* 206表示支持断点续传 */
    if (response_code == 206) {
        ALOG_D("debug");
        return 1;
    } 
    /* 200表示不支持断点续传 */
    else if (response_code == 200) {
        ALOG_D("debug");
        return 0;
    }
    
    return -1;
}

/**
 * @brief 使用libcurl下载文件（支持断点续传）
 * @param url 要下载文件的URL地址
 * @param output_file 保存下载文件的本地路径
 * @return 成功返回0，失败返回-1
 */
int download_file_curl_api(const char *url, const char *output_file) 
{
    CURL *curl = NULL;
    FILE *fp = NULL;
    CURLcode res;
    struct stat file_info;
    curl_off_t resume_from = 0;
    int support_resume = 0;
    long response_code = 0;

    ALOG_D("debug");

    /* 参数校验 */
    if (!url || !output_file) {
        return -1;
    }

    ALOG_D("debug");

    /* 检查服务器是否支持断点续传 */
    support_resume = check_server_supports_resume(url);
    if (support_resume < 0) {
        ALOG_W("Failed to check if server supports resume");
    }

    ALOG_D("debug");

    /* 初始化curl句柄 */
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    ALOG_D("debug");
    /* 如果支持断点续传且文件已存在，则从断点开始下载 */
    if (support_resume == 1 && stat(output_file, &file_info) == 0) {
        resume_from = file_info.st_size;
        ALOG_T("Resuming download from byte: %lld", (long long)resume_from);
        fp = fopen(output_file, "ab"); // 追加模式打开文件
    } else {
        /* 不支持断点续传或文件不存在，重新下载 */
        if (support_resume == 0) {
            ALOG_T("Server does not support resume, downloading from start");
        }
        fp = fopen(output_file, "wb"); // 覆盖模式打开文件
        resume_from = 0;
    }
    ALOG_D("debug");
    if (!fp) {
        curl_easy_cleanup(curl);
        return -1;
    }

    /* 设置curl选项 */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    ALOG_D("debug");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    ALOG_D("debug");
    
    /* 如果需要断点续传，设置起始位置 */
    if (resume_from > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resume_from);
        ALOG_D("debug 1");
    }

    ALOG_D("debug");
    /* 执行下载操作 */
    res = curl_easy_perform(curl);
    ALOG_D("debug");
    /* 获取响应码 */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    ALOG_D("debug");
    /* 如果尝试断点续传但服务器返回200（不支持），则重新下载 */
    if (resume_from > 0 && response_code == 200) {
        ALOG_T("Server ignored resume request, re-downloading from start");
        fclose(fp);
        fp = fopen(output_file, "wb"); // 重新以覆盖模式打开文件
        if (!fp) {
            curl_easy_cleanup(curl);
            return -1;
        }
        
        /* 重新设置curl选项并再次下载 */
        ALOG_D("debug");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        ALOG_D("debug");
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)0);
        ALOG_D("debug");
        res = curl_easy_perform(curl);
        ALOG_D("debug");
    }

    ALOG_D("debug");
    /* 关闭文件并清理资源 */
    if (fp) {
        fclose(fp);
    }
    ALOG_D("debug");
    curl_easy_cleanup(curl);
    ALOG_D("debug");
    /* 根据下载结果返回相应状态码 */
    return (res == CURLE_OK) ? 0 : -1;
}

#else
#include "http_download.h"

uint32_t percent_log = 0;
static void http_download_log_callback(const char* log_msg) {
    ALOG_D("[HTTP_DOWNLOAD] %s\n", log_msg);
}

// 下载进度回调函数
bool progress_callback(uint64_t total_bytes, uint64_t downloaded_bytes) {
    if (total_bytes > 0) {
        uint32_t percent = (downloaded_bytes * 100) / total_bytes;

        if (percent - percent_log >= 15u || percent >= 100)
        {
            percent_log = percent;
            ALOG_I("HTTP Download Progress: %d%% (%llu/%llu bytes)\n", 
               percent, downloaded_bytes, total_bytes);
        }
    }

    return true; // 继续下载
}

int download_file_curl_api(const char *url, const char *output_file) 
{
    if (http_download_init() != 0) {
        ALOG_E("Failed to initialize HTTP downloader: %s\n", 
               http_download_get_error());
        return -1;
    }
    else
    {
        percent_log = 0;
    }

    // 设置下载选项
    http_download_options_t options = {
        .url = url,
        .save_path = output_file,
        .timeout_ms = 30000,  // 30秒超时
        .buffer_size = 4096,  // 4KB缓冲区
        .resume_download = true,  // 支持断点续传
        .log_callback = http_download_log_callback  // 使用默认日志函数
    };
    ALOG_T("HTTP download options set >>>");
    ALOG_T("  URL: %s", options.url);
    ALOG_T("  Save Path: %s", options.save_path);
    ALOG_T("  Timeout: %d ms", options.timeout_ms);
    ALOG_T("  Buffer Size: %d bytes", options.buffer_size);
    // ALOG_T("  Resume Download: %s", options.resume_download ? "Enabled" : "Disabled");


    // 执行下载
    int result = http_download_file(&options, progress_callback);

    if (result == 0) {
        ALOG_W("Download completed successfully!\n");
    } else {
        ALOG_E("Download failed: %s\n", http_download_get_error());
    }
    
    // 清理资源
    http_download_cleanup();
    
    return result;
}

#endif

// 下载升级包
static int L_ExecDownloadImg(TCmdParams *cmdparams)
{
    int result = 0;

    if (cmdparams->url != NULL)
    {
        ALOG_T("URL upgrade start");
        // 设置下载目标路径
        cmdparams->img = cmdparams->keys_dir == NULL? NFCREATE_UPGRADE_IMG_PATH : NFCREATE_UPGRADE_SE_IMG_PATH;
        
#if (DOWNLOAD_FILE_BY_CURL)
        // 构建curl命令，增加连接超时(60秒)和总超时(600秒)设置
        char curl_cmd[512] = {0};
        snprintf(curl_cmd, sizeof(curl_cmd), "curl --connect-timeout 60 -m 600 -o %s %s", cmdparams->img, cmdparams->url);
        ALOG_T("curl_cmd: %s", curl_cmd);

        // 执行curl下载
        // int ret = system(curl_cmd);
#endif

        int download_retry = 2;
        int ret = download_file_curl_api(cmdparams->url, cmdparams->img);
        while (ret != 0 && download_retry > 0) 
        {
            ALOG_E("Download failed, error code: %d", ret);
            ret = download_file_curl_api(cmdparams->url, cmdparams->img);
            download_retry--;
        }
        
        if (ret != 0) 
        {
            result = -(__LINE__);
        }
        else
        {
            result = 1;
            ALOG_T("Download firmware completed: %s", cmdparams->img);
        }
    }


    


    if (result >= 0)
        ALOG_T("%s Pass", __func__);
    else
        ALOG_E("%s Failed. errcode:%d", __func__, result);

    return result;
}

static const char *L_GetSeFilesName(TSeImgFileId id, void *user_param)
{
    const char *result = NULL;
    TCmdParams *cmdparams = (TCmdParams *)user_param;

    switch (id)
    {
        case kSeImgFileIdContextAse256CbcKey:
        {
            static char buff[128] = {0};
            snprintf(buff, sizeof(buff), "%s/aes256_key.hex", cmdparams->keys_dir);
            result = (const char *)buff;
            break;
        }

        case kSeImgFileIdContextAse256CbcIv:
        {
            static char buff[128] = {0};
            snprintf(buff, sizeof(buff), "%s/aes256_iv.hex", cmdparams->keys_dir);
            result = (const char *)buff;
            break;
        }

        case kSeImgFileIdSignatureRsaPublicPem:
        {
            static char buff[128] = {0};
            snprintf(buff, sizeof(buff), "%s/signature_public_key.pem", cmdparams->keys_dir);
            result = (const char *)buff;
            break;
        }
        default: break;
    }

    return result;
}

// 解包升级包
static int L_ExecUnpackSeImg(TCmdParams *cmdparams)
{
    int result = 0;

    if (cmdparams->keys_dir != NULL)
    {
        ALOG_I("security upgrade\n");
        // 解密升级包 
        TSeImg *se_img = CreateTSeImg(cmdparams->img);
        
        if (se_img != NULL && TSeImgIsValid(se_img))
        {
            TSeImgSetFileNameFunc(se_img, L_GetSeFilesName, cmdparams);
            result = TSeImgUnpackImgToFile(se_img, NFCREATE_UPGRADE_IMG_PATH, NULL);
            DestroyTSeImg(se_img);
        }
        else
            result = -(__LINE__);
    }
    else
    {
        ALOG_I("normal upgrade\n");
        // 移动升级包到指定目录
        if (FfFileExists(cmdparams->img))
        {
            if (strcmp(cmdparams->img, NFCREATE_UPGRADE_IMG_PATH) != 0
                && FfFileExists(NFCREATE_UPGRADE_IMG_PATH))
                FfDeleteFile(NFCREATE_UPGRADE_IMG_PATH);
                
            result = FfRenameFile(cmdparams->img, NFCREATE_UPGRADE_IMG_PATH) ? 1 : -(__LINE__);
        }
        else
        {
            ALOG_E("File not exists: %s", cmdparams->img);
            result = -(__LINE__);
        }
    }

    if (result >= 0)
        ALOG_T("%s Pass", __func__);
    else
        ALOG_E("%s Failed. errcode:%d", __func__, result);

    return result;
}

static int L_ExecUpgradeReq(TCmdParams *cmdparams)
{
    int result = 0;
    UInt8 state = 0;
    UInt8 task_id = 0;
    Int32 errcode = SrtGetValueU08(kSidUInt8NfUpgradeState, &state);
    if (errcode < 0) 
    {
        printf("get NfUpgradeState Failed. errcode:%d, state:%d\n", errcode, state);
        result = -(__LINE__);
    }
    else if (state == kNfUpgradeStateIdle
        || state == kNfUpgradeStateSuccess
        || state == kNfUpgradeStateFailed)
    {
        if (SrtGetValueU08(kSidUInt8NfUpgradeRspId, &task_id) < 0)
            task_id = 0;
        result = SrtSetValueU08(kSidUInt8NfUpgradeReqId, task_id + 1);
    }
    else
    {
        printf("NfUpgradeState Error\n");
        result = -(__LINE__);
    }

    if (result >= 0)
        ALOG_T("%s Pass", __func__);
    else
        ALOG_E("%s Failed. errcode:%d", __func__, result);

    return result;
}


// 循环查询状态
static int L_ExecQueryState(TCmdParams *cmdparams)
{
    int result = 0;
    UInt8 state = 0;
    UInt8 task_id = 0;
    UInt8 percent = 0;
    UInt32 errcode = 0;
    UInt16 flag = 0;

    while (errcode == 0 && percent < 100)
    {
        SfSleep(5000);
        SrtGetValueU08(kSidUInt8NfUpgradeState, &state);
        SrtGetValueU08(kSidUInt8NfUpgradeRspId, &task_id);
        SrtGetValueU08(kSidUInt8NfUpgradePrecent, &percent);
        SrtGetValueU32(kSidUInt32NfUpgradeErrCode, &errcode);
        SrtGetValueU16(kSidUInt16NfUpgradeFlag, &flag);
        printf("state:%d, task_id:%d, percent:%d, errcode:%d, flag:0x%x\n", state, task_id, percent, errcode, flag);
    }

    if (state == kNfUpgradeStateWaiteReboot)
    {
        result = 1;
        ALOG_T("%s Pass", __func__);
    }
    else
    {
        result = -(__LINE__);
        ALOG_E("%s Failed. errcode:%d", __func__, result);
    }        

    return result;
}


static int L_CmdExec(TCmdParams *cmdparams)
{
    int result = 0;

    do
    {
        result = L_ExecDownloadImg(cmdparams);
        if (result < 0)
            break;

        result = L_ExecUnpackSeImg(cmdparams);
        if (result < 0)
            break;

        result = L_ExecUpgradeReq(cmdparams);
        if (result < 0)
            break;

        result = L_ExecQueryState(cmdparams);
        if (result < 0)
            break;

    } while (0);
    
    if (result >= 0)
        printf("Upgrade Success\n");
    else
        printf("Upgrade Failed. errcode:%d\n", result);

    // // 删除临时文件
    // FfDeleteFile(MPU_UPGRADE_IMG_NAME);
    // if (strcmp(cmdparams->img, NFCREATE_UPGRADE_IMG_PATH) != 0)
    //     FfDeleteFile(NFCREATE_UPGRADE_IMG_PATH);

    return result;
}



int CmdLine(int argc, char *argv[])
{
    TCmdParams cmdparams = {0};

    // 解析命令行参数, 获取img文件路径和密钥路径
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--buildtime") == 0)
        {
            printf("buildtime:%s\n", __DATE__ " " __TIME__);
        }
        else if (strcmp(argv[i], "--img") == 0)
        {
            if (i + 1 < argc)
            {
                // 获取img文件路径
                cmdparams.img = argv[i + 1];
                printf("img:%s\n", cmdparams.img);
                i++;
            }
        }
        else if (strcmp(argv[i], "--keys-dir") == 0)
        {
            if (i + 1 < argc)
            {
                // 获取密钥路径
                cmdparams.keys_dir = argv[i + 1];
                printf("keys_dir:%s\n", cmdparams.keys_dir);
                i++;
            }
        }
        else if (strcmp(argv[i], "--url") == 0)
        {
            if (i + 1 < argc)
            {
                // 获取密钥路径
                cmdparams.url = argv[i + 1];
                printf("url:%s\n", cmdparams.url);
                i++;
            }
        }
    }

    if (cmdparams.img == NULL && cmdparams.url == NULL)
    {
        ALOG_W("Usage: %s [--img <img_file_path>] [--keys-dir <keys_dir_path>]\n", argv[0]);
        ALOG_W("Usage: %s [--url <url>] [--keys-dir <keys_dir_path>]\n", argv[0]);
        return -1;
    }

    return L_CmdExec(&cmdparams);
}



//---------------------------------------------------------------


int CmdDownloadImg(const char *url, const char *keys_dir)
{
    Int32 result = 0;
    TCmdParams cmdparams = {0};
    cmdparams.url = (char *)url;
    cmdparams.keys_dir = (char *)keys_dir;

    do
    {
        result = L_ExecDownloadImg(&cmdparams);
        if (result < 0)
            break;

        result = L_ExecUnpackSeImg(&cmdparams);
        if (result < 0)
            break;
    } while (0);

    if (result >= 0)
        ALOG_T("%s Pass", __func__);
    else
        ALOG_E("%s Failed. errcode:%d", __func__, result);

    return result;
}


///< Generated on "2025-02-07 11:33:59" by the tool "gen_hq_file.py >> V20231119" 

