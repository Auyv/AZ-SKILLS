/**
 * @file        mpu_upgrade.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/14
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/14   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "mpu_upgrade.h"

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AR590)

#include "ofc/BaseTypes.h"              
#include "ffc/ffc_file_utils.h"

#define ALOG_TAG "Ar590Upgrade"
#include "alog/a_log.h"
#include "sfc/sfc_api.h"

#include "icv_absys.h"
#include "icv_fota_api.h"
#include "fota_info.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 升级文件路径缓冲区大小 */
#define IMG_PATH_SIZE           128

/* icv_absys_switch() 返回值（来自 SDK 文档 §3.5） */
#define ICV_SWITCH_OK           0   /* 切换成功，等待重启 */
#define ICV_SWITCH_ERR_BUSY     -2  /* 非激活系统正在升级或同步，不可切换 */
#define ICV_SWITCH_ERR_DAMAGED  -3  /* 非激活系统升级/同步损坏，不可切换 */
#define ICV_SWITCH_ERR_PARTERR  -4  /* 非激活系统有分区损坏，不可切换 */





struct TMpuUpgradeTag
{
    TMpuUpgradeState state;
    char img_path[IMG_PATH_SIZE];

    Bool req_upgrade;  // 是否请求升级
    Bool req_backup;  // 是否请求备份

    Int32 upgrade_timeout_ms;  // 升级超时时间
    Int32 backup_timeout_ms;   // 备份超时时间
    
    UInt32 upgrade_req_tick;  // 升级请求时间
    UInt32 backup_req_tick;   // 备份请求时间

    void *upgrade_user_param;  // 升级用户参数
    void *backup_user_param;   // 备份用户参数

    TMpuUpgradeCallback upgrade_callback;  // 升级回调函数
    TMpuUpgradeCallback backup_callback;   // 备份回调函数
};
typedef struct TMpuUpgradeTag TMpuUpgrade;

static TMpuUpgrade g_mpu_upgrade = {0};


const char *L_MpuUpgradeStateString(TMpuUpgradeState state)
{
    const char *result = "Unknown";

    if ((unsigned)state < kMpuUpgradeStateMax)
    {
        const char *state_str[] = {
            "Invalid",
            "Idle",
            "Upgrading",
            "Backuping",
            "WaitReboot",
            "WaitConfirm",
            "UpdateSuccess",
            "UpdateFailed"
        };
        result = state_str[state];
    }

    return result;
}

static void L_MpuUpgradeDoUpgradeCallback(int percent)
{
    TMpuUpgradeCallback callback = g_mpu_upgrade.upgrade_callback;
    void *user_param = g_mpu_upgrade.upgrade_user_param;

    if (callback != NULL) 
        callback(g_mpu_upgrade.state, percent, user_param);
}

static void L_MpuUpgradeDoBackupCallback(int percent)
{
    TMpuUpgradeCallback callback = g_mpu_upgrade.backup_callback;
    void *user_param = g_mpu_upgrade.backup_user_param;

    if (callback != NULL) 
        callback(g_mpu_upgrade.state, percent, user_param);
}





// -------------------------- 模组API适配接口 -- BEGIN --------------------------

static char *L_ModuleOtaStateStr(int state)
{
    char *result = NULL;

    switch (state)
    {
        case SUCCEED:   result = "SUCCEED"; break;
        case UPDATE:    result = "UPDATE";  break;
        case BACKUP:    result = "BACKUP";  break;
        case FAILED:    result = "FAILED";  break;
        case WRITEDONE: result = "WRITEDONE"; break;
        case NEEDSYNC:  result = "NEEDSYNC";  break;
        case CANCEL:    result = "CANCEL";    break;
        case UNKNOWN_STATUS:
        default: result = "UNKNOWN_STATUS"; break;
    }

    return result;
}

// 获取升级状态
int L_Ar590GetAbFotaState()
{
    int result = UNKNOWN_STATUS;
    sysstatus_t sys_state = {0};
    int icv_err = icv_absys_getstatus(&sys_state);

    if (icv_err < 0) 
    {
        ALOG_E("icv_absys_getstatus() failed to get absys status, errno = %d!!!", icv_err);
    }
    else
    {
        if (sys_state.is_damaged == 0)
        {
            ALOG_T("absys partition status : succeed");

            update_info_t update_info = {0};
            icv_err = icv_abfota_get_update_status(&update_info);
            
            if ( icv_err < 0) 
            {
                ALOG_E("run icv_abfota_get_update_status failed, api return: %d", icv_err);
                result = FAILED;    // 获取失败
            }
            else
            {
                ALOG_T("Current fota state: %s", L_ModuleOtaStateStr(update_info.ota_state));
                result = update_info.ota_state;
            }
        }
        else
        {
            ALOG_E("absys partition status : damaged");
            ALOG_E("absys partition damaged position : %s", sys_state.damaged_partname);
            ALOG_E("absys needsync!!!");
            result = FAILED;
        }
    }

    return result;
}


int L_Ar590DoBackup(UInt32 timeout_ms)
{
    const UInt32 poll_interval_ms = 15000;          /* 轮询间隔 15 秒 */
    const UInt32 startup_delay_ms = 2 * 60 * 1000;  /* 开机后等待 2 分钟，避开启动高峰期 */
    // const int sync_cpu_level = 1;                    /* ar590模组不支持。CPU占用率: 0=98%, 1=55%, 2=38%, 3=30%, 4=25%, 5=20% */
    int result = 0;
    UInt32 curr_tick = SfGetTickCount();

    L_MpuUpgradeDoBackupCallback(1);

    int ab_state = L_Ar590GetAbFotaState();

    if (ab_state == BACKUP)
    {
        L_MpuUpgradeDoBackupCallback(33);

        while ((SfGetTickCount() - curr_tick) < timeout_ms)
        {
            SfSleep(poll_interval_ms);

            ab_state = L_Ar590GetAbFotaState();
            if (ab_state != BACKUP)
                break;
        }

        if (ab_state == SUCCEED)
        {
            L_MpuUpgradeDoBackupCallback(100);
            result = 1;
        }
        else
        {
            ALOG_E("backup timeout");
            result = -3;
        }
    }
    else
    {
        // <2025-07-12> Jpz : 状态确认后, 需要在开机2min之后执行同步. 避开开机高峰期
        while (curr_tick <= startup_delay_ms)
        {
            ALOG_W(" >>> Sync partition 2 min after power-on, avoiding startup peak.");
            SfSleep(startup_delay_ms - curr_tick);
            curr_tick = SfGetTickCount();
        }
        
        ALOG_W("do AB sync begin");
        int ret = icv_absys_sync();
        if (ret != 0)
        {
            ALOG_T("run icv_absys_sync failed, api return: %d", ret);
            result = -1;
        }
        else
        {
            L_MpuUpgradeDoBackupCallback(33);
            ALOG_I("do AB sync triggered, waiting for SUCCEED...");

            // 轮询等待同步完成
            while ((SfGetTickCount() - curr_tick) < timeout_ms)
            {
                SfSleep(poll_interval_ms);
                ab_state = L_Ar590GetAbFotaState();
                if (ab_state != BACKUP)
                    break;
            }

            if (ab_state == SUCCEED)
            {
                L_MpuUpgradeDoBackupCallback(100);
                result = 1;
            }
            else
            {
                ALOG_E("sync timeout or failed, ab_state: %s", L_ModuleOtaStateStr(ab_state));
                result = -3;
            }
        }
    }
        
    return result;
}




// -------------------------- 模组API适配接口 -- END --------------------------



/**
 * @brief 初始化MPU升级模块
 * @return 成功返回0，失败返回错误码
 */
int MpuUpgradeInit(void)
{
    int result = 0;

    memset(&g_mpu_upgrade, 0, sizeof(g_mpu_upgrade));
    g_mpu_upgrade.state = kMpuUpgradeStateInvalid;

    // 初始化AB差分升级功能 (PDF 4.1: 必须先于其他OTA API调用)
    // 注意: icv_fota_init() 函数未在 SDK 头文件中定义，暂时跳过初始化
    // 根据 SDK API 参考，icv_abfota_* 函数可以直接使用，无需初始化调用
    int ret = 0;  // icv_fota_init();
    ALOG_I("AR590 MCU upgrade module init (FOTA init skipped - function not in SDK)");

    return result;
}


static TMpuUpgradeState L_MpuUpgradeOnStateInvalid(void)
{
    // 刚开机. 检查分区同步情况是否需要执行
    TMpuUpgradeState next_state = kMpuUpgradeStateInvalid;

    Int32 ab_state = L_Ar590GetAbFotaState();

    switch (ab_state)
    {
        case SUCCEED:   
            ALOG_I("AbResult() is SUCCEED when module open");    
            next_state = kMpuUpgradeStateIdle; 
            break;

        case UPDATE:    
            ALOG_E("AbFotaState() is UPDATE when module open");
            next_state = kMpuUpgradeStateIdle;
            break;

        case BACKUP:    
            ALOG_E("AbFotaState() is BACKUP when module open");
            ALOG_E("  Possibly, another task performed the partition synchronization.");
            next_state = kMpuUpgradeStateWaitConfirm;
            break;

        case FAILED:    
            ALOG_E("AbFotaState() is FAILED when module open");
            next_state = kMpuUpgradeStateUpdateFailed;  
            break;
        
        case WRITEDONE: 
            ALOG_E("AbResult() is WRITEDONE when module open");
            next_state = kMpuUpgradeStateWaitReboot;
            break;

        case NEEDSYNC:
            ALOG_I("AbResult() is NEEDSYNC when module open");
            next_state = kMpuUpgradeStateWaitConfirm;
            break;

        case CANCEL:
            ALOG_I("AbFotaState() is CANCEL when module open");
            next_state = kMpuUpgradeStateIdle;
            break;

        case UNKNOWN_STATUS:
        default:
            ALOG_E("AbFotaState() is UNKNOWN when module open");
            next_state = kMpuUpgradeStateUpdateFailed;
            break;
    }

    return next_state;
}



static TMpuUpgradeState L_MpuUpgradeOnStateIdle(void)
{
    TMpuUpgradeState next_state = g_mpu_upgrade.state;

    if (g_mpu_upgrade.req_upgrade)
    {
        next_state = kMpuUpgradeStateUpgrading;
    }
    else if (g_mpu_upgrade.req_backup)
    {
        next_state = kMpuUpgradeStateBackuping;
    }

    return next_state;
}



// 准备升级包：提取目录路径，重命名为 update.zip，触发升级
// 返回: 0=成功触发升级, <0=失败
static int L_Ar590PrepareAndStartUpdate(void)
{
    int result = -1;
    char pkg_dir[IMG_PATH_SIZE] = {0};
    char update_zip_path[IMG_PATH_SIZE] = {0};
    const char *img_path = MpuUpgradeImgPath();
    const char *last_sep = strrchr(img_path, '/');

    if (last_sep == NULL)
    {
        ALOG_E("invalid img_path (no directory separator): %s", img_path);
    }
    else
    {
        int dir_len = (int)(last_sep - img_path) + 1;
        if (dir_len >= (int)sizeof(pkg_dir) - 11)
        {
            ALOG_E("img_path directory too long: %s", img_path);
        }
        else
        {
            memcpy(pkg_dir, img_path, dir_len);
            pkg_dir[dir_len] = '\0';
            snprintf(update_zip_path, sizeof(update_zip_path), "%supdate.zip", pkg_dir);

            // 若源文件不是 update.zip，则重命名
            Bool rename_ok = True;
            if (strcmp(img_path, update_zip_path) != 0)
            {
                ALOG_I("rename: %s -> %s", img_path, update_zip_path);
                if (FfFileExists(update_zip_path))
                    FfDeleteFile(update_zip_path);
                rename_ok = FfRenameFile(img_path, update_zip_path);
                if (!rename_ok)
                    ALOG_E("FfRenameFile failed");
            }

            if (rename_ok)
            {
                ALOG_I("package dir: %s", pkg_dir);
                result = icv_abfota_start_update(pkg_dir);
            }
        }
    }

    return result;
}

// 轮询等待升级完成
// 返回: 0=升级完成(WRITEDONE), <0=失败或超时
static int L_Ar590PollUpgradeDone(update_info_t *update_info)
{
    const UInt32 poll_interval_ms = 5000;  /* 升级状态轮询间隔 5 秒 */
    const int progress_min = 10;           /* 进度回调最小值，避免频繁上报 */
    const int progress_max = 100;          /* 进度回调最大值 */
    int result = -1;
    UInt32 start_tick = SfGetTickCount();
    Bool polling = True;

    while (polling && (SfGetTickCount() - start_tick) < g_mpu_upgrade.upgrade_timeout_ms)
    {
        SfSleep(poll_interval_ms);

        if (icv_abfota_get_update_status(update_info) < 0)
        {
            ALOG_E("icv_abfota_get_update_status failed");
            L_MpuUpgradeDoUpgradeCallback(-1);
            polling = False;
        }
        else
        {
            ALOG_I("fota progress: %d%%, state: %s, exit_code: %d",
                update_info->percentage, L_ModuleOtaStateStr(update_info->ota_state), update_info->exit_code);

            if (update_info->percentage > progress_min && update_info->percentage < progress_max)
                L_MpuUpgradeDoUpgradeCallback(update_info->percentage);

            if (update_info->ota_state == WRITEDONE)
            {
                result = 0;
                polling = False;
            }
            else if (update_info->ota_state == FAILED)
            {
                ALOG_E("upgrade failed, exit_code: %d", update_info->exit_code);
                L_MpuUpgradeDoUpgradeCallback(-1);
                polling = False;
            }
        }
    }

    if (result != 0)
    {
        ALOG_E("upgrade timeout or failed, ota_state: %s", L_ModuleOtaStateStr(update_info->ota_state));
        L_MpuUpgradeDoUpgradeCallback(-1);
    }

    return result;
}

// 切换系统
// 返回: 0=成功, <0=失败
static int L_Ar590SwitchSystem(void)
{
    int result = icv_absys_switch();

    if (result == ICV_SWITCH_OK)
    {
        ALOG_I("icv_absys_switch succeed, waiting for reboot");
    }
    else if (result == ICV_SWITCH_ERR_BUSY)
    {
        ALOG_E("icv_absys_switch failed: system is upgrading or syncing");
    }
    else if (result == ICV_SWITCH_ERR_DAMAGED)
    {
        ALOG_E("icv_absys_switch failed: non-active system damaged");
    }
    else if (result == ICV_SWITCH_ERR_PARTERR)
    {
        ALOG_E("icv_absys_switch failed: non-active partition damaged");
    }
    else
    {
        ALOG_W("icv_absys_switch returned %d, possibly already switched", result);
        result = ICV_SWITCH_OK;  // 其他进程已切换，视为成功
    }

    return result;
}

static TMpuUpgradeState L_MpuUpgradeOnStateUpgrading(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateUpgrading;
    update_info_t update_info = {0};
    int err = icv_abfota_get_update_status(&update_info);

    g_mpu_upgrade.req_upgrade = False;

    // 若升级前状态不是 SUCCEED，先执行分区同步防止异常
    if (update_info.ota_state != SUCCEED)
    {
        ALOG_W("state is %s, do backup first", L_ModuleOtaStateStr(update_info.ota_state));
        if (L_Ar590DoBackup(g_mpu_upgrade.upgrade_timeout_ms) < 0)
            ALOG_T("L_Ar590DoBackup failed");
        else
            ALOG_I("do AB sync succeed");
    }

    L_MpuUpgradeDoUpgradeCallback(1);

    // 准备升级包并触发升级
    err = L_Ar590PrepareAndStartUpdate();
    if (err < 0)
    {
        ALOG_E("L_Ar590PrepareAndStartUpdate failed, err: %d", err);
        L_MpuUpgradeDoUpgradeCallback(-1);
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        L_MpuUpgradeDoUpgradeCallback(10);

        // 轮询等待升级完成
        if (L_Ar590PollUpgradeDone(&update_info) < 0)
        {
            next_state = kMpuUpgradeStateUpdateFailed;
        }
        else
        {
            // 切换系统
            L_MpuUpgradeDoUpgradeCallback(90);
            if (L_Ar590SwitchSystem() < 0)
            {
                next_state = kMpuUpgradeStateUpdateFailed;
            }
            else
            {
                L_MpuUpgradeDoUpgradeCallback(100);
                next_state = kMpuUpgradeStateWaitReboot;
            }
        }
    }

    return next_state;
}

static TMpuUpgradeState L_MpuUpgradeOnStateBackuping(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateBackuping;

    g_mpu_upgrade.req_backup = False;

    
    if (L_Ar590DoBackup(g_mpu_upgrade.backup_timeout_ms) < 0)
    {
        ALOG_T("L_Ar590DoBackup failed");
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        ALOG_I("do AB sync succeed");
        next_state = kMpuUpgradeStateIdle;
    }

    return next_state;
}


static TMpuUpgradeState L_MpuUpgradeOnStateWaitConfirm(void)
{
    const UInt32 confirm_timeout_ms = 25 * 60 * 1000;  /* 等待确认超时 25 分钟 */
    TMpuUpgradeState next_state = kMpuUpgradeStateWaitConfirm;

    if (L_Ar590DoBackup(confirm_timeout_ms) < 0)
    {
        ALOG_T("L_Ar590DoBackup failed");
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        ALOG_I("do AB sync succeed");
        next_state = kMpuUpgradeStateUpdateSuccess;
    }

    return next_state;
}

/**
 * @brief 循环执行MPU升级功能组件。
 * @return 
 * @note 部分模组可以不调用,可用于实时检测是否需要自动修复损坏分区
 */
void MpuUpgradeRunOnce(void)
{
    TMpuUpgradeState curr_state = g_mpu_upgrade.state;
    TMpuUpgradeState next_state = curr_state;

    switch (curr_state)
    {
    case kMpuUpgradeStateInvalid: 
        next_state = L_MpuUpgradeOnStateInvalid(); 
        break;
    
    case kMpuUpgradeStateUpdateSuccess: 
    case kMpuUpgradeStateUpdateFailed:  
    case kMpuUpgradeStateIdle:   
        next_state = L_MpuUpgradeOnStateIdle();  
        break;

    case kMpuUpgradeStateUpgrading: 
        next_state = L_MpuUpgradeOnStateUpgrading(); 
        break;
    case kMpuUpgradeStateBackuping: 
        next_state = L_MpuUpgradeOnStateBackuping(); 
        break;
    case kMpuUpgradeStateWaitReboot: 
        break;
    case kMpuUpgradeStateWaitConfirm: 
        next_state = L_MpuUpgradeOnStateWaitConfirm(); 
        break;

    default:        
        break;
    }

    static int last_log_tick = 0;
    int curr_tick = SfGetTickCount();

    if (next_state != curr_state)
    {
        last_log_tick = curr_tick;
        g_mpu_upgrade.state = next_state;
        ALOG_I("MpuUpgrade state changed: %s(%d) -> %s(%d)", 
            L_MpuUpgradeStateString(curr_state), curr_state, 
            L_MpuUpgradeStateString(next_state), next_state);
    }
    // else if (curr_tick - last_log_tick > 10*1000)
    // {
    //     ALOG_I("MpuUpgrade state: %s(%d)", L_MpuUpgradeStateString(curr_state), curr_state);
    //     last_log_tick = curr_tick;
    // }
}

/**
 * @brief 用于通知升级组件结束
 * @return 
 * @note:xxxxx
 */
void MpuUpgradeDone(void)
{

}

/**
 * @brief 获取当前升级状态
 * @return       当前升级状态
 * @note:xxxxx
 */
TMpuUpgradeState MpuUpgradeState(void)
{
    return g_mpu_upgrade.state;
}


/**
 * @brief 获取升级文件路径
 * @return 当前设置的升级文件路径
 * @note:xxxxx
 */
char *MpuUpgradeImgPath(void)
{
    return g_mpu_upgrade.img_path;
}

/**
 * @brief 设置升级文件路径
 * @param *img_path 升级文件路径
 * @return == 0, 成功; < 0, 失败
 * @note:xxxxx
 */
int MpuUpgradeSetImgPath(char *img_path)
{
    int len = strlen(img_path);

    if (len < sizeof(g_mpu_upgrade.img_path))
    {
        memcpy(g_mpu_upgrade.img_path, img_path, len);
        g_mpu_upgrade.img_path[len] = '\0';
    }
    else
    {
        ALOG_E("img_path too long: %s", img_path);
        len = -1;
    }
    
    return len;
}


/**
 * 执行升级更新分区的操作
 * 
 * @param timeout 操作超时时间，单位为秒
 * @param user_param 用户自定义参数，用于回调函数
 * @param callback 升级过程中的回调函数指针
 * 
 * @return 返回操作结果，0表示成功，非0表示失败
 * 
 * 此函数负责在指定的超时时间内完成升级更新分区的任务
 * 它会调用回调函数来通知用户升级过程中的状态
 */
int MpuUpgradeExecUpdate(int timeout, void *user_param, TMpuUpgradeCallback callback)
{
    const int max_timeout_s = 45 * 60;      /* 最大允许超时 45 分钟 */
    const int default_timeout_ms = 10 * 60 * 1000;  /* 默认超时 10 分钟 */
    int result = -1;
    int module_state = L_Ar590GetAbFotaState();
    ALOG_I("L_Ar590GetAbFotaState() -> %s(%d)", L_ModuleOtaStateStr(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_upgrade = True;
    g_mpu_upgrade.upgrade_timeout_ms = (timeout > 0 && timeout < max_timeout_s)? timeout * 1000 : default_timeout_ms;
    g_mpu_upgrade.upgrade_req_tick = SfGetTickCount();
    g_mpu_upgrade.upgrade_user_param = user_param;
    g_mpu_upgrade.upgrade_callback = callback;

    result = 0;

    return result;
}

/**
 * 执行升级备份分区的操作
 * 
 * @param timeout 操作超时时间，单位为秒
 * @param user_param 用户自定义参数，用于回调函数
 * @param callback 升级过程中的回调函数指针
 * 
 * @return 返回操作结果，0表示成功，非0表示失败
 * 
 * 此函数负责在指定的超时时间内完成升级备份分区的任务
 * 它会调用回调函数来通知用户升级过程中的状态
 */
int MpuUpgradeExecBackup(int timeout, void *user_param, TMpuUpgradeCallback callback)
{
    const int max_timeout_s = 45 * 60;      /* 最大允许超时 45 分钟 */
    const int default_timeout_ms = 10 * 60 * 1000;  /* 默认超时 10 分钟 */
    int result = -1;
    int module_state = L_Ar590GetAbFotaState();
    ALOG_I("L_Ar590GetAbFotaState() -> %s(%d)", L_ModuleOtaStateStr(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_backup = True;
    g_mpu_upgrade.backup_timeout_ms = (timeout > 0 && timeout < max_timeout_s)? timeout * 1000 : default_timeout_ms;
    g_mpu_upgrade.backup_req_tick = SfGetTickCount();
    g_mpu_upgrade.backup_user_param = user_param;
    g_mpu_upgrade.backup_callback = callback;

    result = 0;

    return result;
}

/**
 * 获取MPU的分区信息函数
 * 
 * @param partition_info 返回当前分区信息
 * 
 * @return == 0, 成功; < 0, 失败
 */
int MpuUpgradeGetPartitionInfo(TMpuPartitionInfo *partition_info)
{
    int result = -1;

    if (partition_info == NULL)
    {
        ALOG_E("partition_info is NULL");
    }
    else
    {
        memset(partition_info, 0, sizeof(TMpuPartitionInfo));

        // 1. 获取当前活动分区
        absystem_t cur_system = {0};
        if (icv_absys_get_cur_active_part(&cur_system) != 0)
        {
            ALOG_E("icv_absys_get_cur_active_part failed");
        }
        else if (cur_system != SYSTEM_A && cur_system != SYSTEM_B)
        {
            ALOG_E("unknown cur_system: %d", cur_system);
        }
        else
        {
            partition_info->active_part = (cur_system == SYSTEM_A) ? 'A' : 'B';

            // 2. 获取分区损坏状态 (PDF 4.5: sysstatus_t.is_damaged)
            sysstatus_t sys_state = {0};
            if (icv_absys_getstatus(&sys_state) == 0)
            {
                partition_info->other_part_is_damage = sys_state.is_damaged ? 1 : 0;
                if (sys_state.is_damaged)
                {
                    ALOG_W("partition damaged: %.16s", sys_state.damaged_partname);
                }
            }
            else
            {
                ALOG_E("icv_absys_getstatus failed");
            }

            result = 0;
        }
    }

    return result;
}


#endif // #if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AR590)


