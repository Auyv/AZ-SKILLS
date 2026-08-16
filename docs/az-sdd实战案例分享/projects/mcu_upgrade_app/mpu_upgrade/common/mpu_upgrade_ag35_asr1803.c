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

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_ASR1803)

#include "ofc/BaseTypes.h"              
#include "ffc/ffc_file_utils.h"

#define ALOG_TAG "Ag35Upgrade"
#include "alog/a_log.h"
#include "sfc/sfc_api.h"

#include "ql_absys_api.h"
#include "ql_fota_api.h"
#include "fota_info.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>





struct TMpuUpgradeTag
{
    TMpuUpgradeState state;
    char img_path[128];

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

static char *L_AsrOtaStateStr(int state)
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
        case UNKNOWN_STATUS:
        default: result = "UNKNOWN_STATUS"; break;
    }

    return result;
}

// 获取升级状态
int L_Ag35Asr1806GetAbFotaState()
{
    int result = UNKNOWN_STATUS;
    sysstatus_t sys_state = {0};
    int ql_err = ql_absys_getstatus(&sys_state);

    if (ql_err < 0) 
    {
        ALOG_E("ql_absys_getstatus() failed to get absys status, errno = %d!!!", ql_err);
    }
    else
    {
        if (sys_state.is_damaged == 0)
        {
            ALOG_T("absys partition status : succeed");

            update_info_t update_info = {0};
            ql_err = ql_abfota_get_update_status(&update_info);
            
            if ( ql_err < 0) 
            {
                ALOG_E("run ql_abfota_get_update_status failed, api return: %d", ql_err);
                result = FAILED;    // 获取失败
            }
            else
            {
                ALOG_T("Current fota state: %s", L_AsrOtaStateStr(update_info.ota_state));
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


int L_Ag35Asr1806DoBackup(UInt32 timeout_ms)
{
    int result = 0;
    UInt32 curr_tick = SfGetTickCount();

    L_MpuUpgradeDoBackupCallback(1);

    int ab_state = L_Ag35Asr1806GetAbFotaState();

    if (ab_state == BACKUP)
    {
        L_MpuUpgradeDoBackupCallback(33);

        while (SfGetTickCount() - curr_tick < timeout_ms)
        {
            SfSleep(15000);

            ab_state = L_Ag35Asr1806GetAbFotaState();
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
        while (curr_tick <= 2 * 60 * 1000)
        {
            ALOG_W(" >>> Sync partition 2 min after power-on, avoiding startup peak.");
            SfSleep(2 * 60 * 1000 - curr_tick);
            curr_tick = SfGetTickCount();
        }
        
        ALOG_W("do AB sync begin");
        int ret = ql_absys_sync();
        if (ret != 0)
        {
            ALOG_T("run ql_absys_sync failed, api return: %d", ret);
            result = -1;
        }
        else
        {
            L_MpuUpgradeDoBackupCallback(90);
            ALOG_I("do AB sync succeed");
            result = -2;
        }

        if (L_Ag35Asr1806GetAbFotaState() == SUCCEED)
        {
            L_MpuUpgradeDoBackupCallback(100);
            result = 1;
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
    memset(&g_mpu_upgrade, 0, sizeof(g_mpu_upgrade));
    g_mpu_upgrade.state = kMpuUpgradeStateInvalid;

    return 0;
}


static TMpuUpgradeState L_MpuUpgradeOnStateInvalid(void)
{
    // 刚开机. 检查分区同步情况是否需要执行
    TMpuUpgradeState next_state = kMpuUpgradeStateInvalid;

    Int32 ab_state = L_Ag35Asr1806GetAbFotaState();

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
            next_state = kMpuUpgradeStateUpgrading;
            break;

        case NEEDSYNC:   
            ALOG_I("AbResult() is NEEDSYNC when module open");
            next_state = kMpuUpgradeStateWaitConfirm;
            break;

        case UNKNOWN_STATUS:
        default: 
            ALOG_E("AbFotaState() is FAILED when module open");
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



static TMpuUpgradeState L_MpuUpgradeOnStateUpgrading(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateUpgrading;
    update_info_t update_info = {0};
    int err = ql_abfota_get_update_status(&update_info);    // 这是同步接口

    g_mpu_upgrade.req_upgrade = False;

    if (update_info.ota_state != SUCCEED)   // <2026-02-24> Jpz : 如果请求升级前不是成功状态。统一先执行备份后再升级。防止上一次升级时异常掉电造成状态卡在 WRITEDONE
    {
        ALOG_W("AbFotaState() is %s when upgrade req, do backup first", L_AsrOtaStateStr(update_info.ota_state));
        // 立刻执行分区同步，防止升级失败
        if (L_Ag35Asr1806DoBackup(g_mpu_upgrade.upgrade_timeout_ms) < 0)
        {
            ALOG_T("L_Ag35Asr1806DoBackup failed");
        }
        else
        {
            ALOG_I("do AB sync succeed");
        }
    }

    L_MpuUpgradeDoUpgradeCallback(1);

    err = ql_abfota_start_update(MpuUpgradeImgPath());
    L_MpuUpgradeDoUpgradeCallback(10);
    
    if (err < 0)
        ALOG_E("run ql_abfota_start_update failed, api return: %d", err);
    else
    {
        L_MpuUpgradeDoUpgradeCallback(50);
        ALOG_I("Update in-active partition SUCCEED");
    }
        
    SfSleep(5000);

    err = ql_abfota_get_update_status(&update_info);    // 这是同步接口
    if ( err < 0) 
    {
        ALOG_E("run ql_abfota_get_update_status failed, api return: %d", err);
        L_MpuUpgradeDoUpgradeCallback(-1);
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        ALOG_I("Current fota progress: %d", update_info.percentage);
        ALOG_I("Current fota state: %s", L_AsrOtaStateStr(update_info.ota_state));
        ALOG_I("Current fota exit code: %d", update_info.exit_code);

        if (update_info.percentage >= 100 && update_info.exit_code == E_NO_ERROR)
        {
            // <2024-10-13> Jpz : 升级成功
            SfSleep(3000);
            if (update_info.ota_state == WRITEDONE || update_info.ota_state == SUCCEED)
            {
                err = ql_absys_switch();
                if (err != 0) {
                    ALOG_E("run ql_absys_switch failed, api return: %d", err);
                    // self->percentage = -1;  // <2025-07-07> Jpz : 存在调用结果失败, 但是结果生效的情况. 例如其他进程已经执行过1次
                    // self->ota_state = ret;
                    // TAsrABSysNotifyUpgradeInfo(self);
                    // L_TAsrABSysStateChange(self, kAsrABSysStateIdle);
                    // return;
                }
            }

            L_MpuUpgradeDoUpgradeCallback(100);
            next_state = kMpuUpgradeStateWaitReboot;
        }
        else
        {
            // <2024-10-13> Jpz : 升级失败
            ALOG_E("ql_abfota_get_update_status failed. ota_state: %s, exit_code: %d", L_AsrOtaStateStr(update_info.ota_state), update_info.exit_code);
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }

    return next_state;
}

static TMpuUpgradeState L_MpuUpgradeOnStateBackuping(void)
{
    UInt32 curr_tick = SfGetTickCount();
    TMpuUpgradeState next_state = kMpuUpgradeStateBackuping;

    g_mpu_upgrade.req_backup = False;

    
    if (L_Ag35Asr1806DoBackup(g_mpu_upgrade.backup_timeout_ms) < 0)
    {
        ALOG_T("L_Ag35Asr1806DoBackup failed");
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
    TMpuUpgradeState next_state = kMpuUpgradeStateWaitConfirm;
    UInt32 curr_tick = SfGetTickCount();

    if (L_Ag35Asr1806DoBackup(25*60*1000) < 0)
    {
        ALOG_T("L_Ag35Asr1806DoBackup failed");
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
    // 关机时检查升级状态，如果处于WRITEDONE就重新执行一次分区切换，防止异常
    int module_state = L_Ag35Asr1806GetAbFotaState();
    ALOG_T("MpuUpgradeDone L_Ag35Asr1806GetAbFotaState() -> %s(%d)", L_AsrOtaStateStr(module_state), module_state);
    if (module_state == WRITEDONE)
    {
        ql_absys_switch();
        module_state = L_Ag35Asr1806GetAbFotaState();
        ALOG_W("MpuUpgradeDone L_Ag35Asr1806GetAbFotaState() -> %s(%d)", L_AsrOtaStateStr(module_state), module_state);
    }
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
    int result = -1;
    int module_state = L_Ag35Asr1806GetAbFotaState();
    ALOG_I("L_Ag35Asr1806GetAbFotaState() -> %s(%d)", L_AsrOtaStateStr(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_upgrade = True;
    g_mpu_upgrade.upgrade_timeout_ms = (timeout > 0 && timeout < 45*60)? timeout * 1000 : 10*60*1000; // 默认10min超时
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
    int result = -1;
    int module_state = L_Ag35Asr1806GetAbFotaState();
    ALOG_I("L_Ag35Asr1806GetAbFotaState() -> %s(%d)", L_AsrOtaStateStr(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_backup = True;
    g_mpu_upgrade.backup_timeout_ms = (timeout > 0 && timeout < 45*60)? timeout * 1000 : 10*60*1000; // 默认10min超时
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

    if (partition_info != NULL)
    {
        memset(partition_info, 0, sizeof(TMpuPartitionInfo));

        // 1. 获取当前活动分区
        absystem_t cur_system = {0};
        if (ql_absys_get_cur_active_part(&cur_system) != 0)
        {
            ALOG_E("ql_absys_get_cur_active_part failed");
        }
        else if (cur_system != SYSTEM_A && cur_system != SYSTEM_B)
        {
            ALOG_E("unknown cur_system: %d", cur_system);
        }
        else
        {
            partition_info->active_part = (cur_system == SYSTEM_A) ? 'A' : 'B';

            // 2. 获取分区损坏状态
            sysstatus_t sys_state = {0};
            if (ql_absys_getstatus(&sys_state) == 0)
            {
                partition_info->other_part_is_damage = sys_state.is_damaged ? 1 : 0;
                if (sys_state.is_damaged)
                {
                    ALOG_W("partition damaged: %.16s", sys_state.damaged_partname);
                }
            }
            else
            {
                ALOG_E("ql_absys_getstatus failed");
            }

            result = 0;
        }
    }
    
    return result;
}


#endif // #if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_ASR1806)


