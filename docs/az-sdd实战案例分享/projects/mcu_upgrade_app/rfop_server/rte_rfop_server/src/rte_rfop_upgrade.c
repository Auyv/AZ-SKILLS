/**
 * @file        hq_local_upgrade.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/07/09
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/07/09   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "rte_rfop_upgrade.h"
#include "rfop/rfop_common.h"
#include "rte_rfop_upgrade_srt_adp.h"
#include "rte_rfop_upgrade_sys_opt.h"
#include "rte_rfop_upgrade_img.h"
#include "sfc/sfc_api.h"

#include <stdio.h>
#include <string.h>

#define ALOG_TAG "RFOP"
#include "alog/a_log.h"

#define FILENAME_LEN_MAX (128)

enum TCltBlkStateTag
{
    kCltBlkStateInvalid = 0,
    kCltBlkStateIdle = 1,       // rfop服务空闲中
    kCltBlkStateRunning = 2,    // 循环更新应答状态
};
typedef enum TCltBlkStateTag TCltBlkState;


// #pragma pack(push, 1)
// 升级任务请求。 mpu -> mcu 掉电不保存
typedef struct TRteRfopUpgradeRequestTag TRteRfopUpgradeRequest;
struct TRteRfopUpgradeRequestTag
{
    UInt8 start_task_id;          // 请求升级任务id
    UInt8 cancel_task_id;       // 是否取消升级任务. True:升级任务id取消；False:升级任务id开始\继续
    UInt8 img_filename_len;
    UInt8 reserver_;
    Int8 img_filename[FILENAME_LEN_MAX];     // 文件名。 包含'\0'
};
// #pragma pack(pop)

// #pragma pack(push, 1)    
// 升级结果应答。 mcu -> mpu 掉电不保存
typedef struct TRteRfopUpgradeAckTag TRteRfopUpgradeAck;
struct TRteRfopUpgradeAckTag
{
    UInt8 task_id;          // 当前升级任务id
    UInt8 state;             // 升级步骤。 @ref TRteRfopUpgradestate
    Int8 percent;          // 升级进度。
    UInt8 reserver_;         // 保留
    Int32 result;           // 升级结果。当 state == kRteRfopUpgradestateDone 时有效。 > 0 表示成功，0 表示无效值，< 0 表示失败.
};
// #pragma pack(pop)


typedef struct TRteRfopUpgradeCltBlkTag TRteRfopUpgradeCltBlk;
struct TRteRfopUpgradeCltBlkTag
{
    TRteRfopUpgradeRequest req_;
    TRteRfopUpgradeAck cur_;
    TRteRfopUpgradeImgStream img_stream_;
    
    UInt8 clt_blk_state;  // 控制块本身状态, @ref TCltBlkState。0:无效；1:空闲；2:执行中

    // UInt8 prev_state_upgrade_;  // 上一个升级状态. 
    UInt32 cur_state_tick_; // 当前状态生效时间戳。
    UInt8 clt_reinit_flag;  // 升级过程中是否需要重置
};


//-------------------------------------------------------------------------

#define CLT_BLK_HANDLE(x) TRteRfopUpgradeCltBlk*x=&g_rfop_upgrade_clt_blk_

static TRteRfopUpgradeCltBlk g_rfop_upgrade_clt_blk_ = {0};

/*---------------------------------------------------------*/

typedef struct TRteRfopUpgradeDoSendDataMethodTag TRteRfopUpgradeDoSendDataMethod;
struct TRteRfopUpgradeDoSendDataMethodTag 
{
    RteRfopUpgradeDoSendDataMethod method;
    void *user_param;
};

static TRteRfopUpgradeDoSendDataMethod g_rte_rfop_upgrade_sys_opt_do_send_data_method_ = {0};

Int32 RteRfopUpgradeSetSendDataMethod(RteRfopUpgradeDoSendDataMethod method, void *user_param)
{
    g_rte_rfop_upgrade_sys_opt_do_send_data_method_.method = method;
    g_rte_rfop_upgrade_sys_opt_do_send_data_method_.user_param = user_param;
    return kRfopHccPass;
}

/**
 * 交互数据发送方法
 * @param *data
 * @param len
 * @return 
 * @note:<2024-07-23> Jpz todo : 
 */
Int32 RteRfopUpgradeDoSendData(const Int8 *data, Int32 len)
{
    Int32 result = 0;
    TRteRfopUpgradeDoSendDataMethod callback = g_rte_rfop_upgrade_sys_opt_do_send_data_method_;

    if (callback.method != NULL)
    {
        result = callback.method(callback.user_param, data, len);
    }

    if (result >= 0)
    {
        // ALOG_T("Send size: %dBytes data:", len);
        // ALOG_HEX(kALogLevelTrace, 32, data, len);
    }
    else
    {
        RFOP_UPGRADE_EXCEPTION(result, "RteRfopUpgradeDoSendData Error");
    }

    return result;
}

//-------------------------------------------------------------------------

static int L_TRfopSrvDoSend(void *user_param, const char *data, int size)
{
    // TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    int result = RteRfopUpgradeDoSendData((const Int8 *)data, size);
    return result;
}

static Bool L_TRfopSrvDoLock(void *user_param, Bool lock_type)
{
    // TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    return True;
}

static void L_TPfopCltDoRaiseException(void *user_param, const char *error_info, Int32 error_code)
{
    // TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    RFOP_UPGRADE_EXCEPTION(error_code, error_info);
    ALOG_E("============================================================");
    ALOG_E(">>>Exception error_code:%d, %s", error_code, error_info);
    ALOG_E("============================================================");

}


static Int32 L_TRfopSrvDoFileOpen(void *user_param, const char *path, UInt8 modes)
{
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    int fd = 0;

    // ALOG_D("DoFileOpen: %s, modes: %d", path, modes);

    // 打开文件前判断是否处于 kCltBlkStateIdle 状态, 是则表示升级任务未开始, 需要更新升级任务状态
    if (self->clt_blk_state == kCltBlkStateIdle)
    {
        fd = RteRfopUpgradeStartTask(path, RteRfopUpgradeCurTaskId());
    }
        
    if (fd < 0)
        ;
    else if (modes != kRfopFomRead)
        fd = kRfopHccFileModeErr;
    else if (/*memcmp(self->req_.img_filename, path, self->req_.img_filename_len) == 0 todo 临时关闭 */ True)  // <2024-08-01> Jpz todo : 临时关闭检查
    {
        self->req_.img_filename_len = RfopStrncpy((char *)self->req_.img_filename, path, self->req_.img_filename_len); 

        if (TRteRfopUpgradeImgStreamIsOpen(&self->img_stream_)
            || TRteRfopUpgradeImgStreamOpen(&self->img_stream_))
        {
            self->img_stream_.app_item_bin_pos = 0;
            fd = self->img_stream_.ff.fd_;
        }
        else
        {
            fd = kRfopHccImgOpenErr;
        }
    }
    else
    {
        fd = kRfopHccNoFile;
        RFOP_UPGRADE_EXCEPTION(fd, "DoFileOpen: path not match");
    }

    if (fd < 0)
        ALOG_W("DoFileOpen: %s, modes: %d, fd:%d\n", self->req_.img_filename, modes, fd);

    return fd;
}

static Int32 L_TRfopSrvDoFileRead(void *user_param, Int32 fd, char *buffer, Int32 size)
{
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    Int32 r = 0;
    
    if (!TRteRfopUpgradeImgStreamIsOpen(&self->img_stream_))
    {
        r = kRfopHccImgOpenErr;
    }
    else if (fd != self->img_stream_.ff.fd_)
    {
        r = kRfopHccFdInvalid;
    }
    else
    {
        r = TRteRfopUpgradeImgStreamAppItemRead(&self->img_stream_, self->img_stream_.app_item_bin_pos, (UInt8 *)buffer, size);
        if (r > 0)
        {
            self->img_stream_.app_item_bin_pos += r;
        }
    }

    if (r < 0 || r != size)
        ALOG_W("DoFileRead: fd:%d, size:%d, r:%d\n", fd, size, r);

    return r;
}

static Int32 L_TRfopSrvDoFileReadFrom(void *user_param, Int32 fd, char *buffer, Int32 size, Int64 from_offset)
{
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    Int32 r = 0;
    
    if (!TRteRfopUpgradeImgStreamIsOpen(&self->img_stream_))
    {
        r = kRfopHccImgOpenErr;
    }
    else if (fd != self->img_stream_.ff.fd_)
    {
        r = kRfopHccFdInvalid;
    }
    else
    {
        r = TRteRfopUpgradeImgStreamAppItemRead(&self->img_stream_, (Int32)from_offset, (UInt8 *)buffer, size);
        // if (r > 0)
        // {
        //     self->img_stream_.app_item_bin_pos += r;
        // }
    }

    if (r < 0 || r != size)
        ALOG_W("DoFileReadFrom: fd:%d, pos:%lld, size:%d, r:%d\n", fd, from_offset, size, r);

    return r;
}

static Int32 L_TRfopSrvDoFileWrite(void *user_param, Int32 fd, const char *data, Int32 size)
{
    return -1;
}

static Int32 L_TRfopSrvDoFileWriteFrom(void *user_param, Int32 fd, const char *data, Int32 size, Int64 from_offset)
{
    return -1;
}

static Bool L_TRfopSrvDoFileClose(void *user_param, Int32 fd)
{
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    Bool r = True;  // 固定返回关闭成功
    
    TRteRfopUpgradeImgStreamClose(&self->img_stream_);

    if (!r)
        ALOG_W("DoFileClose: fd:%d, r:%d\n", fd, r);

    return r;
}

static Int64 L_TRfopSrvDoFileSeek(void *user_param, Int32 fd, Int64 offset, Int32 whence)
{
    // ALOG_T("DoFileSeek: fd:%d, whence:%d, ofs:%lld", fd, whence, offset);
    
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;
    Int64 r = 0;
    // ALOG_T("debug header:%p", self->img_stream_.app_item_header);
    UInt32 max = self->img_stream_.app_item_header->size;

    ALOG_W("DoFileSeek: fd:%d, whence:%d, ofs:%lld\n", fd, whence, offset);

    if (fd != self->img_stream_.ff.fd_)
    {
        r = kRfopHccFdInvalid;
    }
    else if (whence == 1/*SEEK_CUR*/)
    {
        r = offset + self->img_stream_.app_item_bin_pos;
    }
    else if (whence == 2/*SEEK_END*/)
    {
        r = (Int64)max - offset;
    }
    else if (whence == 0/*SEEK_SET*/)
    {
        r = offset;
    }

    if (r <= 0)
        self->img_stream_.app_item_bin_pos = 0;
    else if (offset >= max)
        self->img_stream_.app_item_bin_pos = max;
    else
        self->img_stream_.app_item_bin_pos = r;

    r = self->img_stream_.app_item_bin_pos;

    ALOG_W("DoFileSeek: fd:%d, whence:%d, ofs:%lld, r:%lld\n", fd, whence, offset, r);
    return r;
}

static Int32 L_TRfopSrvDoFileInfoGet(void *user_param, Int32 fd, TRfopFileInfo *r_info)
{
    Int32 result = kRfopHccFileNotExist;

    ALOG_W("DoFileInfoGet: fd:%d\n", fd);
    TRteRfopUpgradeCltBlk *self = (TRteRfopUpgradeCltBlk *)user_param;

    r_info->FileDateSize = self->img_stream_.app_item_header->size;
    r_info->Crc16 = self->img_stream_.app_item_header->crc;

    return result;
}

//-------------------------------------------------------------------------

static const TRfopSrvBgo g_rte_rfop_upgrade_bgo_impl_ = {
    .user_param = &g_rfop_upgrade_clt_blk_,
    .DoSend = L_TRfopSrvDoSend,
    .DoLock = L_TRfopSrvDoLock,

    .DoRaiseException = L_TPfopCltDoRaiseException,

    .DoFileOpen = L_TRfopSrvDoFileOpen,
    .DoFileRead = L_TRfopSrvDoFileRead,
    .DoFileReadFrom = L_TRfopSrvDoFileReadFrom, 
    .DoFileWrite = L_TRfopSrvDoFileWrite,
    .DoFileWriteFrom = L_TRfopSrvDoFileWriteFrom, 
    .DoFileClose = L_TRfopSrvDoFileClose,
    .DoFileSeek = L_TRfopSrvDoFileSeek,
    .DoFileInfoGet = L_TRfopSrvDoFileInfoGet,
};



Int32 RteRfopUpgradeCreate(void)
{
    CLT_BLK_HANDLE(self);
    memset(self, 0, sizeof(*self));
    self->img_stream_.ff.fd_ = -1;
    self->clt_reinit_flag = 0;
    
    return 0;
}



static Bool L_FlushCurAckInfo(void)
{
    Bool result = False;
    CLT_BLK_HANDLE(self);

    UInt8 val_task_id;
    UInt8 val_task_state;
    Int8 val_task_percent;
    Int32 val_task_result;

    if (RteRfopUpgradeSrtAdpGetCurTaskId(&val_task_id) >= 0
        && RteRfopUpgradeSrtAdpGetCurTaskState(&val_task_state) >= 0
        && RteRfopUpgradeSrtAdpGetCurTaskPercent(&val_task_percent) >= 0
        && RteRfopUpgradeSrtAdpGetCurTaskResult(&val_task_result) >= 0)
    {
        if (val_task_state != self->cur_.state)
        {
            ALOG_W("StateChanged: %d->%d", self->cur_.state, val_task_state);
            
            self->cur_.state = val_task_state;
            self->cur_state_tick_ = SfGetTickCount();
            result = True;
        }

        if (val_task_id != self->cur_.task_id
            || val_task_percent != self->cur_.percent
            || val_task_result != self->cur_.result)
        {
            self->cur_.task_id = val_task_id;
            self->cur_.percent = val_task_percent;
            self->cur_.result = val_task_result;

            result = True;
        }

        if (result)
            RteRfopUpgradeSysOptStateNotify(val_task_id, val_task_state, val_task_percent, val_task_result);

        result = True;  // <2024-12-26> Jpz : result最终应表示当前刷新状态有效
    }

    return result;
}


//-----------------------------------------------------------------------

Int32 RteRfopUpgradeInit(void)
{
    CLT_BLK_HANDLE(self);
    Int32 result = 0;

    RteRfopUpgradeSysOptInit();

    // 加载控制块信息 如果失败则重新初始化
    RteRfopUpgradeSrtAdpGetReqImgFilename((UInt8 *)self->req_.img_filename, sizeof(self->req_.img_filename));
    RteRfopUpgradeSrtAdpGetReqStartTaskId(&self->req_.start_task_id);
    RteRfopUpgradeSrtAdpGetReqCancelTaskId(&self->req_.cancel_task_id);

    if (L_FlushCurAckInfo())
    {
        switch (self->cur_.state)
        {
            case kRteRfopUpgradeStateIdle:
            case kRteRfopUpgradeStatePreCheck:
            case kRteRfopUpgradeStateFileCheck:
            case kRteRfopUpgradeStateUpgrade:
            case kRteRfopUpgradeStateWaitReboot:
            case kRteRfopUpgradeStateWaitConfirm:
            case kRteRfopUpgradeStateDone:
            case kRteRfopUpgradeStateFailed:
                self->clt_blk_state = kCltBlkStateIdle;
                break;

            case kRteRfopUpgradeStateDownload:
                #if (0) // 暂不支持断电续传
                self->clt_blk_state = kCltBlkStateRunning;
                #else
                self->clt_blk_state = kCltBlkStateInvalid;
                #endif
                break;

            case kRteRfopUpgradeStateInvlalid:
            default:
                result = -2;
                self->clt_blk_state = kCltBlkStateInvalid;
                ALOG_W("Invalid State");
                break;
        }
    }
    else
    {
        result = -1;
        self->clt_blk_state = kCltBlkStateInvalid;
        ALOG_W("Invalid State");
    }
    
    return result;
}

void RteRfopUpgradeClear(void)
{
    ALOG_D("RteRfopUpgradeClear");
    RteRfopUpgradeSysOptClear();
}

void RteRfopUpgradeDone(void)
{
    // <2024-07-23> Jpz todo : 
    RteRfopUpgradeSysOptDone();
}


void RteRfopUpgradeRunOnce(void)
{
    UInt32 cur_tick = SfGetTickCount();
    static UInt32 flush_cur_ack_tick_ = 0;
    CLT_BLK_HANDLE(self);

    // 定时刷新当前状态
    if (cur_tick - flush_cur_ack_tick_ > 500)
    {
        flush_cur_ack_tick_ = cur_tick;
        L_FlushCurAckInfo();
        // Int32 ret = RteRfopUpgradeCurTaskState();
        // ALOG_D("RteRfopUpgradeCurTaskState: %d", ret);
    }

    switch (self->clt_blk_state)
    {
        case kCltBlkStateIdle:
        {
            break;
        }
        case kCltBlkStateRunning:
        {
            // <2024-07-24> Jpz : 等待mcu请求拉取文件
            L_FlushCurAckInfo();  // 加速刷新状态

            if (self->cur_.state >= kRteRfopUpgradeStateDone)
            {
                self->clt_blk_state = kCltBlkStateIdle;
                TRteRfopUpgradeImgStreamDestroy(&self->img_stream_);  // 由远端确认结束升级时析构img
            }
            break;
        }
        case kCltBlkStateInvalid:  // case1: 刚开机时, 属性同步未完成. 重新初始化等待基础服务启动完成
        default:                   // case2: mcu出现异常, rfop相关属性值加载异常. 
        {   
            static UInt32 reinit_tick_ = 0;
            const UInt8 reinit_durtion_s[] = {1, 1, 1, 2, 2, 5, 10, 30, 60};
            const UInt8 max_idx = sizeof(reinit_durtion_s) - 1;
            
            UInt8 idx = self->clt_reinit_flag > max_idx ? max_idx : self->clt_reinit_flag;

            if (cur_tick - reinit_tick_ >= reinit_durtion_s[idx] * 1000)
            {
                reinit_tick_ = cur_tick;
                
                if (RteRfopUpgradeInit() >= 0)
                    self->clt_reinit_flag = 0;
                else if (self->clt_reinit_flag < max_idx)
                    self->clt_reinit_flag++;
                else
                {}
            }
            break;
        }
    }

    // 本地持续判断当前状态维持时间, 如果超时5min, 主动发送任务取消
    switch (self->cur_.state)
    {
        case kRteRfopUpgradeStatePreCheck:
        case kRteRfopUpgradeStateDownload:
        case kRteRfopUpgradeStateFileCheck:
        case kRteRfopUpgradeStateUpgrade:
        // case kRteRfopUpgradeStateWaitReboot: // <2024-07-24> Jpz : 有开机升级需求, 所以等待重启不做状态超时判断
        case kRteRfopUpgradeStateWaitConfirm:
            if (SfGetTickCount() - self->cur_state_tick_ >= (1000 * 60 * 10))
            {
                ALOG_E("State timeout, force cancellation.  curr_state: %d", self->cur_.state);
                RteRfopUpgradeCancelTask(0);
                self->cur_state_tick_ = SfGetTickCount();   // 防止属性同步失效, 导致取消指令连续触发
            }
            break;
        default:
            break;
    }
}

void RteRfopUpgradeDestroy(void)
{
    CLT_BLK_HANDLE(self);

    TRteRfopUpgradeImgStreamDestroy(&self->img_stream_); 
    
}

// ---------------------------------------------------------------------------


const TRfopSrvBgo *RteRfopUpgradeBgoImpl(void)
{
    return &g_rte_rfop_upgrade_bgo_impl_;
}

// ---------------------------------------------------------------------------


static Bool L_SendReqInfo(void)
{
    Bool result = False;
    CLT_BLK_HANDLE(self);

    Int32 len = strnlen((const char *)self->req_.img_filename, sizeof(self->req_.img_filename));

    if (self->req_.img_filename_len != len)
    {
        self->req_.img_filename_len = len;
    }

    if (RteRfopUpgradeSrtAdpSetReqImgFilename((UInt8 *)self->req_.img_filename, self->req_.img_filename_len) >= 0
        && RteRfopUpgradeSrtAdpSetReqCancelTaskId(self->req_.cancel_task_id) >= 0
        && RteRfopUpgradeSrtAdpSetReqStartTaskId(self->req_.start_task_id) >= 0)
    {
        result = True;
    }

    return result;
}


// 检测升级文件是否有效
static Bool L_ImgStreamCreate(const char *upgrade_filename)
{
    CLT_BLK_HANDLE(self);
    Bool result = False;
    TRteRfopUpgradeImgStream *img_stream = &self->img_stream_;
    Int32 err = TRteRfopUpgradeImgStreamCreate(img_stream, upgrade_filename);

    ALOG_D("L_ImgStreamCreate err:%d", err);

    if (err >= 0)
    {
        
        // 版本信息核对 <2024-07-24> Jpz todo : 临时忽略文件名中项目检查
        if (False /*memcmp(img_stream->filename_format.project_name, RTE_RFOP_UPGRADE_PROJECT_NAME, img_stream->filename_format.project_name_len) != 0*/ )
        {
            ALOG_W("project_name_len:%d project_name:%s", img_stream->filename_format.project_name_len, img_stream->filename_format.project_name);

            err = kRfopHccFilenameInvalid;
        }
        // 校验文件格式并打开
        else if (!TRteRfopUpgradeImgStreamOpen(img_stream))
        {
            err = kRfopHccImgOpenErr;
        }
        // MD5文件校验 <2024-10-16> Jpz todo : 暂时关闭
        // else if (!TRteRfopUpgradeImgStreamCheckMd5(img_stream))
        // {
        //     err = kRfopHccImgMd5Err;
        // }
        else if (!TRteRfopUpgradeImgStreamCheckAppItemHeader(img_stream))
        {
            err = kRfopHccImgItemErr;
        }
        else
        {
            err = kRfopHccPass;
        }

        // <2024-07-24> Jpz : 文件检查完成后保证关闭
        TRteRfopUpgradeImgStreamClose(img_stream);  
    }

    ALOG_D("L_ImgStreamCreate err:%d", err);

    if (err >= 0)
    {
        result = True;
    }
    else
    {
        RFOP_UPGRADE_EXCEPTION(err, "Img File Is Invalid");
    }
    
    return result;
}



Int32 RteRfopUpgradeStartTask(const char *upgrade_filename, UInt8 task_id)
{
    CLT_BLK_HANDLE(self);
    Int32 result = 0;   // 返回 当前新建的任务id。 范围 0 < task_id <= 255
    
    ALOG_D("upgrade_filename:%s",upgrade_filename);
    ALOG_D("self->clt_blk_state:%d",self->clt_blk_state);

    self->req_.img_filename_len = (UInt8)RfopStrncpy((char *)self->req_.img_filename, upgrade_filename, sizeof(self->req_.img_filename));
        
    ALOG_D("self->req_.img_filename_len:%d", self->req_.img_filename_len);
    ALOG_D("self->req_.img_filename:%s", self->req_.img_filename);


    //---------------------------------------------------------------

    if (self->clt_blk_state != kCltBlkStateIdle)  // 判断当前是否处于空闲
    {
        result = kRfopHccIsBusy;
    }
    else if (!L_ImgStreamCreate(upgrade_filename))  // 判断升级文件是否有效
    {
        result = kRfopHccFileFormatError;
    }
    else 
    {
        // 设置参数
        // <2024-07-23> Jpz : 获取上次一应答的任务id
        L_FlushCurAckInfo();
        if (task_id == 0)
        {
            // 自动递增获取本次任务id
            self->req_.start_task_id = self->cur_.task_id == 0xff ? 1 : self->cur_.task_id + 1;
        }
        else
        {
            self->req_.start_task_id = task_id;
        }
        
        self->req_.cancel_task_id = 0;
        
        // 发送请求信息
        if (L_SendReqInfo())
        {
            self->clt_blk_state = kCltBlkStateRunning;

            RteRfopUpgradeSrtAdpSetCurTaskState(kRteRfopUpgradeStatePreCheck);
            RteRfopUpgradeSrtAdpSetCurTaskPercent(0);
            RteRfopUpgradeSrtAdpSetCurTaskResult(0);
            L_FlushCurAckInfo();
            result = kRfopHccSuccess;
        }
        else
        {
            // 关闭img文件
            TRteRfopUpgradeImgStreamDestroy(&self->img_stream_); 
            result = kRfopHccSrtError;
        }
    }
    
    ALOG_T("RteRfopUpgradeStartTask result:%d", result);

    return result == kRfopHccSuccess ? self->req_.start_task_id : result;
}


Int32 RteRfopUpgradeCancelTask(Int32 task_id)
{
    CLT_BLK_HANDLE(self);
    Int32 result = kRfopHccParamInvalid;

    L_FlushCurAckInfo();

    if (task_id == 0)
    {
        task_id = self->cur_.task_id;
    }

    self->req_.cancel_task_id = task_id;
    result = RteRfopUpgradeSrtAdpSetReqCancelTaskId(task_id);

    L_SendReqInfo();

    return result;
}

// -------------------------------------------------------------------------


Int32 RteRfopUpgradeCurTaskId(void)
{
    // UInt8 val;
    // Int32 result = RteRfopUpgradeSrtAdpGetCurTaskId(&val);
    CLT_BLK_HANDLE(self);
    return self->req_.start_task_id;
}

 
Int32 RteRfopUpgradeCurTaskState(void)
{
    CLT_BLK_HANDLE(self);
    return self->clt_blk_state == kCltBlkStateInvalid ? kRteRfopUpgradeStateInvlalid : self->cur_.state;
}

 
Int32 RteRfopUpgradeCurTaskPercent(void)
{
    Int8 val;
    Int32 result = RteRfopUpgradeSrtAdpGetCurTaskPercent(&val);
    
    return result >= 0 ? val : result;
}

 
Int32 RteRfopUpgradeCurTaskResult(void)
{
    Int32 val;
    Int32 result = RteRfopUpgradeSrtAdpGetCurTaskResult(&val);
    
    return result >= 0 ? val : result;
}


///< Generated on "2024-07-09 15:14:42" by the tool "gen_hq_file.py >> V20231119" 

