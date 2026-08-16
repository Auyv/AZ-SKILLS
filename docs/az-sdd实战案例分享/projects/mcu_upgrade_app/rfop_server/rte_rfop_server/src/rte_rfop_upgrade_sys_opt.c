/**
 * @file        rte_rfop_upgrade_sys_opt.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/07/23
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/07/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "rte_rfop_upgrade_sys_opt.h"
#include "rfop/rfop_common.h"
#include "mfc/mfc_md5.h"

#define ALOG_TAG "RfopSysOpt"
#include "alog/a_log.h"
#include <string.h>

#define INVALID_FD -1

/*---------------------------------------------------------*/
#define MAX_HANDLE_COUNT 1  // 最大文件句柄数量

/*---------------------------------------------------------*/

#define FILE_API_TYPE_STD     0 // 使用标准文件操作接口. (fopen)
#define FILE_API_TYPE_FFC     1 // 使用ffc文件操作接口. (FfFileOpen)
#define FILE_API_TYPE_SDMME   2 // 片外EMMC

#define USE_FILE_API_TYPE   FILE_API_TYPE_FFC   // 使用ffc文件操作接口.

#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
#include "ffc/ffc_file.h"
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
#include <stdio.h>
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
#include "nvm_dev.h"
#include "nvm_sd_stream.h"
#include "rte_nvm_part_config.h"
#include "srt/srt.h"
#include "srt_pid_type.h"

typedef struct TRteRfopUpgradeNvmCfgTag TRteRfopUpgradeNvmCfg;
struct TRteRfopUpgradeNvmCfgTag
{
    UInt32 page_start;
    UInt32 page_count;
    TNvmDev *dev;
};

typedef struct TRteRfopUpgradeNvmDevTag TRteRfopUpgradeNvmDev;
struct TRteRfopUpgradeNvmDevTag
{
    TNvmSdStream sd_stream;
    TFfStream *fd;
    const TRteRfopUpgradeNvmCfg *cfg;
    Int32 file_size; // 支持读取的文件大小. 因为裸写分区大小不一定, 在升级过程中, 需要重新计算文件大小.
};

static const TRteRfopUpgradeNvmCfg g_upgrade_nvm_dev_cfg_[MAX_HANDLE_COUNT] = 
{
    {.page_start = NVM_UPGRADE_START_PAGE, .page_count = NVM_UPGRADE_PAGE_COUNT, .dev = &g_nvm_sd}, // <2024-11-15> Jpz todo : 需要确认mcu img文件范围
};

static TRteRfopUpgradeNvmDev g_upgrade_nvm_dev_[MAX_HANDLE_COUNT] = {0};

#else
    #error "USE_FILE_API_TYPE is error in rte_rfop_upgrade_sys_opt.c!"
#endif




/*---------------------------------------------------------*/
// 文件操作相关管理
typedef struct TRteRfopUpgradeSysOptFileFileHandleTag TRteRfopUpgradeSysOptFileFileHandle;
struct TRteRfopUpgradeSysOptFileFileHandleTag 
{
#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
    THandle handle;
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
    FILE *handle;
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
    TRteRfopUpgradeNvmDev *handle;
#endif
    char path_[128];
    int mode_;
    int fd;
};

static TRteRfopUpgradeSysOptFileFileHandle g_file_handles_[MAX_HANDLE_COUNT] = {0};
static UInt8 g_file_handle_count_ = 0;


static int L_LookupFileHandle(const char *path, int mode)
{
    int i;
    int result = INVALID_FD;

    if (g_file_handle_count_ > 0)
    {
        TRteRfopUpgradeSysOptFileFileHandle *pos = NULL;

        for (i = 0; i < MAX_HANDLE_COUNT; i++)
        {
            pos = &g_file_handles_[i];

            if (pos->fd > INVALID_FD
                && strncmp(pos->path_, path, sizeof(pos->path_)) == 0
                && pos->mode_ == mode)
            {
                ALOG_I("LookupFileHandle success, fd:%d, path:%.128s, mode:%d", pos->fd, pos->path_, pos->mode_);
                result = pos->fd;
                break;
            }
            else
            {
                ALOG_D("g_file_handle: fd:%d, path:%.128s, mode:%d", pos->fd, pos->path_, pos->mode_);
            }
        }
    }

    return result;
}



/*---------------------------------------------------------*/

#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
static int L_RteRfopUpgradeSysOptAddFileHandle(THandle handle, const char *path, UInt8 modes)
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
static int L_RteRfopUpgradeSysOptAddFileHandle(FILE *handle, const char *path, UInt8 modes)
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
static int L_RteRfopUpgradeSysOptAddFileHandle(TRteRfopUpgradeNvmDev *handle, const char *path, UInt8 modes)
#endif
{
    int i;
    int result = kRfopHccNoMem;
    TRteRfopUpgradeSysOptFileFileHandle *pos = NULL;
    
    ALOG_D("g_file_handle_count_:%d / %d", g_file_handle_count_, MAX_HANDLE_COUNT);

    for (i = 0; i < MAX_HANDLE_COUNT; i++)
    {
        pos = &g_file_handles_[i];

        if (pos->fd <= INVALID_FD)
        {
            pos->fd = i;
            pos->handle = handle;
            pos->mode_ = modes;
            strncpy(pos->path_, path, sizeof(pos->path_) - 1);
            pos->path_[sizeof(pos->path_) - 1] = '\0';
            result = pos->fd;
            g_file_handle_count_++;
            ALOG_D("[add]pos->fd:%d i:%d mode:%d path:%.128s", pos->fd, i, modes, pos->path_);
            break;
        }
        else
        {
            ALOG_D("[exist]pos->fd:%d i:%d mode:%d path:%.128s", pos->fd, i, modes, pos->path_);
        }
    }

    ALOG_D("************result:%d", result);

    return result;
}


static void L_RteRfopUpgradeSysOptRemoveFileHandle( int fd )
{
    TRteRfopUpgradeSysOptFileFileHandle *pos = NULL;
    int i;

    for (i = 0; i < MAX_HANDLE_COUNT; i++)
    {
        pos = &g_file_handles_[i];
        if (pos->fd == fd)
        {
            pos->fd = INVALID_FD;
            #if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
            pos->handle = INVALID_HANDLE_VALUE;
            #elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
            pos->handle = NULL;
            #elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
            pos->handle = NULL;
            #endif
            g_file_handle_count_--;
            break;
        }
    }
}

static void L_RteRfopUpgradeSysOptClearFileHandles()
{
    TRteRfopUpgradeSysOptFileFileHandle *pos = NULL;
    int i;

    for (i = 0; i < MAX_HANDLE_COUNT; i++)
    {
        pos = &g_file_handles_[i];
        if (pos->fd > INVALID_FD)
        {
            RteRfopUpgradeSysOptDoFileClose(pos->fd);
        }
        pos->fd = INVALID_FD;
    }

    g_file_handle_count_ = 0;
}

#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
static THandle L_RteRfopUpgradeSysOptGetFileHandle(int fd)
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
static FILE *L_RteRfopUpgradeSysOptGetFileHandle(int fd)
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
static TRteRfopUpgradeNvmDev *L_RteRfopUpgradeSysOptGetFileHandle(int fd)
#endif
{
    #if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
    THandle result = INVALID_HANDLE_VALUE;
    #elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
    FILE *result = NULL;
    #elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
    TRteRfopUpgradeNvmDev *result = NULL;
    #endif

    int i;

    for (i = 0; i < MAX_HANDLE_COUNT; i++)
    {
        if (g_file_handles_[i].fd == fd)
        {
            result = g_file_handles_[i].handle;
            break;
        }
    }

    return result;
}

/*---------------------------------------------------------*/


void RteRfopUpgradeSysOptInit(void)
{
    int i;
    // 初始化变量
    for (i = 0; i < MAX_HANDLE_COUNT; i++)
    {
        #if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
        g_file_handles_[i].handle = INVALID_HANDLE_VALUE;
        #elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
        g_file_handles_[i].handle = NULL;
        #elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
        g_file_handles_[i].handle = NULL;
        #endif
        g_file_handles_[i].fd = INVALID_FD;
    }
    g_file_handle_count_ = 0;
}

void RteRfopUpgradeSysOptClear(void)
{
    // <2025-12-26> Jpz : 升级异常后强制执行清理，防止mcu端忘记关闭文件句柄
    L_RteRfopUpgradeSysOptClearFileHandles();
}

void RteRfopUpgradeSysOptDone(void)
{
    L_RteRfopUpgradeSysOptClearFileHandles();
}

/*---------------------------------------------------------*/

// 文件操作
/**
 * 文件打开接口
 * @param *self     rfop客户端对象指针
 * @param *path     远端文件名。长度不超过255Byte，包含结尾‘\0’
 * @param mode      打开模式。 参考 TRfopFom
 * @return >= 0     打开成功，文件句柄
 *         <  0     打开失败，返回错误码
 * @note:<2024-07-23> Jpz todo : 
 */
Int32 RteRfopUpgradeSysOptDoFileOpen(const char *path, UInt8 modes)
{
    int fd = L_LookupFileHandle(path, modes);

    if (fd == INVALID_FD)
    {
#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)

        Int32 mode = kFfFileModeOpenRead;

        if (modes == kRfopFomRead) 
            mode = kFfFileModeOpenRead;
        // else if (modes == kRfopFomWrite || modes == kRfopFomCreate)  // <2024-11-01> Jpz : 暂不支持
        //     mode = kFfFileModeOpenWrite | kFfFileModeAlways;
        // else if (modes == kRfopFomTrunc)
        //     mode = kFfFileModeOpenWrite | kFfFileModeAlways;
        else
        {
            return kRfopHccParamInvalid;
        }

        THandle handle = FfFileOpen(path, mode);
        ALOG_D("handle:%p", handle);

        if (handle != INVALID_HANDLE_VALUE)
        {
            if (modes == kRfopFomTrunc)
                FfFileSeek(handle, 0, kFfSeekOriginFromEnd);

            fd = L_RteRfopUpgradeSysOptAddFileHandle(handle, path, modes);
            if (fd < 0)
            {
                FfFileClose(handle);
            }
        }
        else
        {
            ALOG_E("FfFileOpen failed: %s, mode:%d", path, mode);
        }
    
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)    
        
        char mode_str[3] = {0};

        // 根据位标记组合模式字符串
        if (modes & kRfopFomRead) {
            mode_str[0] = 'r';
        } 
        // else if (modes & kRfopFomWrite) { // <2024-11-01> Jpz : 暂不支持
        //     mode_str[0] = 'w';
        // } else if (modes & kRfopFomTrunc) {
        //     mode_str[0] = 'a';
        // } else if (modes & kRfopFomCreate)
        // {
        //     mode_str[0] = 'w';
        // }

        mode_str[1] = 'b'; // 添加二进制模式标志
        mode_str[2] = 0; 

        ALOG_D("path:%s, mode:%s", path, mode_str);

        FILE *handle = fopen(path, mode_str);

        if (handle != NULL)
        {
            fd = L_RteRfopUpgradeSysOptAddFileHandle(handle, path, modes);
            if (fd < 0)
            {
                fclose(handle);
            }
        }
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
        TRteRfopUpgradeNvmDev *handle = NULL;

        if (path != NULL && modes == kRfopFomRead)
        {
            UInt32 idx = 0; // <2024-11-15> Jpz : 固定区域

            if (idx < MAX_HANDLE_COUNT)
            {
                Int32 obos_download_filesize = 244768;  

                handle = &g_upgrade_nvm_dev_[idx];
                handle->cfg = &g_upgrade_nvm_dev_cfg_[idx];

                #if (1) // <2024-11-22> Jpz  : 获取裸写分区下载的文件大小
                if (SrtGetValueU32(kSidUInt32ObosHttpFileSize, (UInt32 *)&obos_download_filesize) < 0)
                {
                    ALOG_E("ObosHttpFileSize Get failed");
                }
                else if (obos_download_filesize <= 0 || obos_download_filesize > handle->cfg->page_count * 512)
                {
                    ALOG_E("ObosHttpFileSize invalid: %d", obos_download_filesize);
                }
                else
                #endif
                {
                    handle->file_size = obos_download_filesize; // <2024-11-22> Jpz : 需要改为读写属性
                    idx = TNvmSdStreamCreate(&handle->sd_stream, handle->cfg->page_start, handle->cfg->page_count);

                    if (idx >= 0)
                    {
                        handle->fd = &handle->sd_stream.parent_;

                        fd = L_RteRfopUpgradeSysOptAddFileHandle(handle, path, modes);
                        if (fd < 0)
                        {
                            OfDestroyObject((TOfObject *)&handle->sd_stream);
                        }
                    }
                }
            }

        }

#endif
    }

    ALOG_I("DoFileOpen: %s, modes: %d, fd:%d\n", path, modes, fd);

    return fd;
}

/**
 * 文件数据读取
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param *buffer   存放读取数据的缓冲区指针
 * @param size      缓冲区的字节数
 * @return  >= 0     读取成功, 返回实际读取的字节数
 *          (其它值)  读取失败的返回码
 * @note:
 */
Int32 RteRfopUpgradeSysOptDoFileRead(Int32 fd, char *buffer, Int32 size)
{
#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
    THandle FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    int r = FfFileRead(FHandle, buffer, size);
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
    FILE *FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    int r = fread(buffer, 1, size, FHandle);
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
    TRteRfopUpgradeNvmDev *handle = L_RteRfopUpgradeSysOptGetFileHandle(fd);

    Int32 curr_pos = TFfStreamPosition(handle->fd);

    if (curr_pos + size > handle->file_size)
    {
        size = handle->file_size - curr_pos;
    }

    int r = TFfStreamRead(handle->fd, buffer, size);
#endif

    if (r < 0 || r != size)
        ALOG_W("DoFileRead: fd:%d, size:%d, r:%d\n", fd, size, r);
    
    return r;
}

/**
 * 文件数据写入
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param *data     待写入的数据指针
 * @param size      待写入的数据字节数
 * @return  >= 0     写入成功, 返回实际写入的字节数
 *          (其它值)  写入失败的返回码
 * @note:xxxxx
 */
// Int32 RteRfopUpgradeSysOptDoFileWrite(Int32 fd, const char *data, Int32 size);

/**
 * 文件关闭
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @return  >= 0     关闭成功
 *          (其它值)  关闭失败的返回码
 * @note:<2024-07-23> Jpz todo : 
 */
Bool RteRfopUpgradeSysOptDoFileClose(Int32 fd)
{
    ALOG_W("DoFileClose: fd:%d", fd);
#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
    THandle FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    L_RteRfopUpgradeSysOptRemoveFileHandle(fd);
    if (FHandle != INVALID_HANDLE_VALUE)
    {
        FfFileClose(FHandle);
    }

    return FHandle != INVALID_HANDLE_VALUE;
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
    FILE *FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    L_RteRfopUpgradeSysOptRemoveFileHandle(fd);
    int r = fclose(FHandle);
    // ALOG_W("DoFileClose: fd:%d, r:%d\n", fd, r);
    return r >= 0;

#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
    TRteRfopUpgradeNvmDev *handle = L_RteRfopUpgradeSysOptGetFileHandle(fd);

    if (handle != NULL)
    {
        L_RteRfopUpgradeSysOptRemoveFileHandle(fd);

        OfDestroyObject((TOfObject *)&handle->sd_stream);
        handle->cfg = NULL;
        handle->fd = NULL;
    }
    
    return True;
#endif
}


/**
 * 文件光标偏移量设置
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param offset    偏移量
 * @param whence    参考 TRfopSeekWhence
 * @return 返回新的文件指针位置，单位通常是字节。如果发生错误，会返回-1
 * @note:<2024-07-23> Jpz todo : 
 */
Int64 RteRfopUpgradeSysOptDoFileSeek(Int32 fd, Int64 offset, Int32 whence)
{
#if (USE_FILE_API_TYPE == FILE_API_TYPE_FFC)
    THandle FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    Int64 r = FfFileSeek(FHandle, offset, whence);
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_STD)
    FILE *FHandle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    int w = SEEK_CUR;

    if (whence == kRfopSeekWhenceSet)
    {
        w = SEEK_SET;
    }
    else if (whence == kRfopSeekWhenceCur)
    {
        w = SEEK_CUR;
    }
    else if (whence == kRfopSeekWhenceEnd)
    {
        w = SEEK_END;
    }
    else
    {
        ALOG_E("DoFileSeek: fd:%d, mode:%d, ofs:%lld", fd, whence, offset);
        return -1;
    }

    Int64 r = fseek(FHandle, offset, whence);
    ALOG_I("fseek(FHandle, offset, whence) == %d", r);

    r = ftell(FHandle);
#elif (USE_FILE_API_TYPE == FILE_API_TYPE_SDMME)
    TRteRfopUpgradeNvmDev *handle = L_RteRfopUpgradeSysOptGetFileHandle(fd);
    Int32 curr_pos = TFfStreamPosition(handle->fd);
    Int32 file_size = handle->file_size;

    if (curr_pos > file_size)
        curr_pos = file_size;

    if (whence == kRfopSeekWhenceSet)
    {
        curr_pos = offset;
    }
    else if (whence == kRfopSeekWhenceCur)
    {
        curr_pos += offset;
    }
    else if (whence == kRfopSeekWhenceEnd)
    {
        curr_pos = file_size - offset;
    }
    else
    {
        ALOG_E("DoFileSeek: fd:%d, mode:%d, ofs:%lld", fd, whence, offset);
        return -1;
    }

    if (curr_pos < 0)
    {
        curr_pos = 0;
    }
    else if (curr_pos > file_size)
    {
        curr_pos = file_size;
    }
    else
    {
        // do nothing
    }

    Int64 r = TFfStreamSeek(handle->fd, curr_pos, kFfSeekOriginFromBegin);
#endif

    if (r < 0)
        ALOG_W("DoFileSeek: fd:%d, mode:%d, ofs:%lld, r:%lld\n", fd, whence, offset, r);
        
    return r;
}

/*---------------------------------------------------------*/

/**
 * 升级过程中状态通知
 * @param step      当前升级流程步骤
 * @param percent   升级进度
 * @param result    升级结果。当 step == kRteRfopUpgradeStepDone 时有效。 > 0 表示成功，0 表示无效值，< 0 表示失败.
 * @return 
 * @note:<2024-07-23> Jpz todo : 
 */
void RteRfopUpgradeSysOptStateNotify(UInt8 task_id, UInt8 step, UInt8 percent, Int32 result)
{
    static UInt8 last_percent = 0;
    if (percent - last_percent >= 15u || percent >= 100 || result != 0)
    {
        last_percent = percent;
        ALOG_W("StateNotify: task_id:%d, step:%d, percent:%d, result:%d", task_id, step, percent, result);
    }
}

void RteRfopUpgradeSysOptExceptionNotify(const char *func_name, UInt32 line, Int32 error_code, const char *error_info)
{
    ALOG_E("!!! OnExceptionNotify: %s, line:%d, error_code:%d, error_info:%s", func_name, line, error_code, error_info);
}


/*---------------------------------------------------------*/

static Int32 L_TRteRfopUpgradeSysOptFileStreamDoRead(void *sender, void *buffer, Int32 count)
{
    TRteRfopUpgradeSysOptFileStream *self = OF_CONTAINER_OF(sender, TRteRfopUpgradeSysOptFileStream, stream);
    return RteRfopUpgradeSysOptDoFileRead(self->fd_, buffer, count);
}


static Int32 L_TRteRfopUpgradeSysOptFileStreamDoSeek(void *sender, Int32 offset, TFfSeekOriginEnum origin)
{
    TRteRfopUpgradeSysOptFileStream *self = OF_CONTAINER_OF(sender, TRteRfopUpgradeSysOptFileStream, stream);
    return RteRfopUpgradeSysOptDoFileSeek(self->fd_, offset, origin);
}

Int32 TRteRfopUpgradeSysOptFileStreamCreate(TRteRfopUpgradeSysOptFileStream *self, const char *filename)
{
    Int32 result = kRfopHccSuccess;

    if (self == NULL || filename == NULL || *filename == 0)
    {
        result = kRfopHccParamInvalid;
    }
    else
    {
        memset(self, 0, sizeof(*self));
        // 打开文件
        result = RteRfopUpgradeSysOptDoFileOpen(filename, kRfopFomRead);
        if (result >= 0)
        {
            self->fd_ = result;
            TFfStreamCreate(&self->stream);
            self->stream.OnRead = L_TRteRfopUpgradeSysOptFileStreamDoRead;
            self->stream.OnWrite = NULL;
            self->stream.OnSeek = L_TRteRfopUpgradeSysOptFileStreamDoSeek;
            self->stream.OnSetSize = NULL;
        }
        else
        {
            self->fd_ = -1;
        }
    }

    return result;
}

void TRteRfopUpgradeSysOptFileStreamDestroy(TRteRfopUpgradeSysOptFileStream *self)
{
    if (self != NULL)
    {
        OfDestroyObject((TOfObject *)&self->stream);

        if (self->fd_ >= 0)
        {
            RteRfopUpgradeSysOptDoFileClose(self->fd_);
            self->fd_ = -1;
        }
            
    }
}

TFfStream *TRteRfopUpgradeSysOptFileStreamFfcHandle(TRteRfopUpgradeSysOptFileStream *self)
{
    TFfStream *result = NULL;

    if (self != NULL)
    {
        result = &self->stream;
    }

    return result;
}

Int32 TRteRfopUpgradeSysOptFileStreamFileSize(TRteRfopUpgradeSysOptFileStream *self)
{
    Int32 result = 0;

    if (self != NULL)
    {
        Int32 curr = RteRfopUpgradeSysOptDoFileSeek(self->fd_, 0, kRfopSeekWhenceCur);
        result = RteRfopUpgradeSysOptDoFileSeek(self->fd_, 0, kRfopSeekWhenceEnd);
        RteRfopUpgradeSysOptDoFileSeek(self->fd_, curr, kRfopSeekWhenceSet);
    }

    return result;
}

/*-----------------------------------------------------------------------------*/



///< Generated on "2024-07-23 10:54:25" by the tool "gen_hq_file.py >> V20231119" 

