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

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_SIMCOM7805)

#include "ofc/BaseTypes.h"              
#include "ffc/ffc_file_utils.h"

#define ALOG_TAG "Sim7805Upgrade"
#include "alog/a_log.h"
#include "sfc/sfc_api.h"

#include "OTAUpgrade_AB.h"

#include <string.h>

struct TMpuUpgradeTag
{
    TMpuUpgradeState state;
    char img_path[128];

    Bool req_upgrade;  // 是否请求升级
    Bool req_backup;  // 是否请求备份

    Int32 upgrade_timeout_ms;  // 升级超时时间
    Int32 backup_timeout_ms;   // 备份超时时间
    
    UInt32 upgrade_req_tick;  // 升级超时时间
    UInt32 backup_req_tick;   // 备份超时时间

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


const char *L_Sim7805AbResultString(Int32 state)
{
    const char *result = "Unknown";

    switch (state)
    {
        case NORMAL_MODE:                   result = "Normal Mode";            break;
        case UPDATE_MODE:                   result = "Update Mode";            break;
        case BACKUP_MODE:                   result = "Backup Mode";            break;
        case UPDATE_SUCCESS:                result = "Update Success";            break;
        case BACKUP_SUCCESS:                result = "Backup Success";            break;
        case PAUSE_MODE:                    result = "Pause Mode";            break;
        case UPDATE_FAIL_CHECK_OTA_FLAG:    result = "Update Fail Check OTA Flag";            break;
        case UPDATE_FAIL_CHECK_OTA_ZIP:     result = "Update Fail Check OTA Zip";            break;
        case UPDATE_FAIL_FULL_UPDATE:       result = "Update Fail Full Update";            break;
        default:
            break;
    }
    return result;
}


static Int32 L_Sim7805GetAbResult(void)
{
    static Int32 last_state = UNKNOWN;
    Int32 state = UNKNOWN;
    char buf[64] = {0};
    Int32 len = sizeof(buf) - 1;
    Int32 errcode = ota_get_ab_result(buf, len);

    if (errcode >= 0)
    {
        if (strncmp(buf, "NORMAL MODE", 11) == 0)
            state = NORMAL_MODE;
        else if (strncmp(buf, "UPDATE MODE", 11) == 0)
            state = UPDATE_MODE;
        else if (strncmp(buf, "BACKUP MODE", 11) == 0)
            state = BACKUP_MODE;
        else if (strncmp(buf, "UPDATE SUCCESS", 14) == 0)
            state = UPDATE_SUCCESS;
        else if (strncmp(buf, "BACKUP SUCCESS", 14) == 0)
            state = BACKUP_SUCCESS;
        else if (strncmp(buf, "PAUSE MODE", 10) == 0)
            state = PAUSE_MODE;
        else if (strncmp(buf, "UPDATE FAIL 441", 15) == 0)
            state = UPDATE_FAIL_CHECK_OTA_FLAG;
        else if (strncmp(buf, "UPDATE FAIL 442", 15) == 0)
            state = UPDATE_FAIL_CHECK_OTA_ZIP;
        else if (strncmp(buf, "UPDATE FAIL 443", 15) == 0)
            state = UPDATE_FAIL_FULL_UPDATE;
        
        if (last_state != state)
        {
            ALOG_I("ota_get_ab_result() -> %s", buf);
            last_state = state;
        }
    }
    else
    {
        ALOG_E("ota_get_ab_result() failed with error code: %d", errcode);
    }

    return state;
}

// 阻塞式操作
static int L_Sim7805DoUpgrade(int timeout_ms)
{
    int result = ota_update_ab();
    
    if (timeout_ms <= 0)
        timeout_ms = 10*60*1000; // 默认10min超时

    if (result >= 0)
    {
        char buf[64] = {0};
        Int32 len = sizeof(buf) - 1;
        UInt32 start_tick = SfGetTickCount();
        int module_state;
        UInt32 log_tick = 0;

        ALOG_I("ota_update_ab() start");

        result = -1;

        do
        {
            SfSleep(5000);
            module_state = L_Sim7805GetAbResult();

            if (module_state == UPDATE_SUCCESS)
            {
                result = 1;
                ALOG_I("ota_update_ab() completed");
                break;
            }
            else if (module_state == UPDATE_FAIL_CHECK_OTA_FLAG
                    || module_state == UPDATE_FAIL_CHECK_OTA_ZIP
                    || module_state == UPDATE_FAIL_FULL_UPDATE)
            {
                result = -1;
                ALOG_E("ota_update_ab() failed with module_state: %d", module_state);
                break;
            }
            else
            {
                if (SfGetTickCount() - log_tick >= 10*1000)
                {
                    ALOG_I("L_Sim7805DoUpgrade.... module_state: %s(%d)", L_Sim7805AbResultString(module_state), module_state);
                    log_tick = SfGetTickCount();
                }

                static int last_percent = 0;
                int percent = ota_get_ab_process(buf, len);
                
                if (percent != last_percent)
                {
                    last_percent = percent;
                    ALOG_I("ota_get_ab_process() = %d, buf:%s", percent, buf);
                }
                
                if (percent < 0)
                    ALOG_E("ota_get_ab_process() = %d, buf:%s", percent, buf);
                else if (percent < 10*100)
                    L_MpuUpgradeDoUpgradeCallback(10);
                else if (percent < 90*100)
                    L_MpuUpgradeDoUpgradeCallback(percent / 100);
                else
                    L_MpuUpgradeDoUpgradeCallback(90);
            }

        } while (SfGetTickCount() - start_tick < timeout_ms);
        
        if (result < 0)
        {
            ALOG_E("ota_update_ab() timeout");
        }
    }
    else
        ALOG_E("Sim7805DoBackup() failed with error code: %d", result);

    return result;
}

// 阻塞式操作
static int L_Sim7805DoBackup(int timeout_ms)
{
    int result = ota_backup_ab();
    
    if (timeout_ms <= 0)
        timeout_ms = 10*60*1000; // 默认10min超时

    if (result >= 0)
    {
        UInt32 start_tick = SfGetTickCount();
        int module_state;
        UInt32 log_tick = 0;

        result = -1;

        do
        {
            SfSleep(5000);
            module_state = L_Sim7805GetAbResult();

            if (module_state == BACKUP_SUCCESS)
            {
                result = 1;
                ALOG_I("ota_backup_ab() completed");
                break;
            }
            else if (SfGetTickCount() - log_tick >= 10*1000)
            {
                ALOG_I("L_Sim7805DoBackup.... module_state: %s(%d)", L_Sim7805AbResultString(module_state), module_state);
                log_tick = SfGetTickCount();
            }

        } while (SfGetTickCount() - start_tick < timeout_ms);
        
        if (result < 0)
        {
            ALOG_E("ota_backup_ab() timeout");
        }
    }
    else
        ALOG_E("Sim7805DoBackup() failed with error code: %d", result);

    return result;
}

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
    TMpuUpgradeState next_state = kMpuUpgradeStateInvalid;

    Int32 ab_state = L_Sim7805GetAbResult();

    if (ab_state == NORMAL_MODE)
    {
        next_state = kMpuUpgradeStateIdle;
    }
    else if (ab_state == UPDATE_MODE)
    {
        ALOG_E("Abnormal -> AbResult() is UPDATE_MODE when module open");
        next_state = kMpuUpgradeStateIdle;
    }
    else if (ab_state == BACKUP_MODE)
    {
        next_state = kMpuUpgradeStateWaitConfirm;
    }
    else if (ab_state == UPDATE_SUCCESS)
    {
        ALOG_E("Abnormal -> AbResult() is UPDATE_SUCCESS when module open");
        next_state = kMpuUpgradeStateUpdateSuccess;
    }
    else if (ab_state == BACKUP_SUCCESS)
    {
        ALOG_W("Abnormal -> AbResult() is BACKUP_SUCCESS when module open");
        next_state = kMpuUpgradeStateWaitConfirm;
    }
    else if (ab_state == PAUSE_MODE)
    {
        ALOG_E("Abnormal -> AbResult() is PAUSE_MODE when module open");
        next_state = kMpuUpgradeStateIdle;
    }
    else
    {
        ALOG_E("Abnormal -> AbResult() is %s(%d) when module open", L_Sim7805AbResultString(ab_state), ab_state);
        next_state = kMpuUpgradeStateIdle;
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
    int err;

    g_mpu_upgrade.req_upgrade = False;

    err = ota_clear_ab();
    if (err == 0)
        printf("do clear success,please use ota_get_ab_result to check clear result");
    else
        printf("do clear fail");

    L_MpuUpgradeDoUpgradeCallback(1);

    char modlue_fota_dir[128] = {0};
    const char *file_name = "/update_ota.zip";
    // 移动固件包
        /// 查看默认升级分区

    ota_set_update_ab_dir("/media/card",64);  // <2025-05-16> Jpz : 不设置升级会异常
    err = ota_get_update_ab_dir(&modlue_fota_dir[0], sizeof(modlue_fota_dir) - 1);

    if (err >= 0)
    {
        strcat(modlue_fota_dir, file_name); // 拼接完整路径

        ALOG_I("FfRenameFile %s to %s", g_mpu_upgrade.img_path, modlue_fota_dir);
        if (FfRenameFile(g_mpu_upgrade.img_path, modlue_fota_dir))
        {
            L_MpuUpgradeDoUpgradeCallback(5);

            if (L_Sim7805DoUpgrade(g_mpu_upgrade.upgrade_timeout_ms) >= 0)
            {
                L_MpuUpgradeDoUpgradeCallback(100);
                ALOG_I("Upgrade success");
                next_state = kMpuUpgradeStateWaitReboot;
            }
            else
            {
                L_MpuUpgradeDoUpgradeCallback(-1);
                ALOG_E("Upgrade failed");
                next_state = kMpuUpgradeStateUpdateFailed;
            }
        }
        else
        {
            ALOG_E("FfRenameFile failed");
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }
    else
    {
        ALOG_E("ota_get_update_ab_dir() failed with error code: %d", err);
        next_state = kMpuUpgradeStateUpdateFailed;
    }

    return next_state;
}

static TMpuUpgradeState L_MpuUpgradeOnStateBackuping(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateBackuping;

    g_mpu_upgrade.req_backup = False;

    L_MpuUpgradeDoBackupCallback(1);

    if (L_Sim7805DoBackup(g_mpu_upgrade.backup_timeout_ms) >= 0)
    {
        L_MpuUpgradeDoBackupCallback(100);
        ALOG_I("Backup success");
        next_state = kMpuUpgradeStateIdle;
    }
    else
    {
        L_MpuUpgradeDoBackupCallback(-1);
        ALOG_E("Backup failed");
        next_state = kMpuUpgradeStateUpdateFailed;
    }

    return next_state;
}


static TMpuUpgradeState L_MpuUpgradeOnStateWaitConfirm(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateWaitConfirm;
    int module_state = L_Sim7805GetAbResult();

    if (module_state == BACKUP_MODE)
    {
        // 等待同步中
        ALOG_I("Waiting for backup to complete");
        if (L_Sim7805DoBackup(25*60*1000) >= 0) // 25min
        {
            ALOG_I("Backup completed");
            next_state = kMpuUpgradeStateUpdateSuccess;
        }
        else
        {
            ALOG_E("Backup failed");
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }
    else if (module_state == BACKUP_SUCCESS)
    {
        next_state = kMpuUpgradeStateUpdateSuccess;
    }
    else if (module_state == UPDATE_FAIL_CHECK_OTA_FLAG
            || module_state == UPDATE_FAIL_CHECK_OTA_ZIP
            || module_state == UPDATE_FAIL_FULL_UPDATE)
    {
        ALOG_E("Update failed");
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        ALOG_E("MpuUpgradeState error: %d", module_state);
        next_state = kMpuUpgradeStateUpdateFailed;
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
    // else if (curr_tick - last_log_tick > 10*1000)  // <2026-06-10> Jpz : 正式发布时防止日志过多，可以注释掉或者调整日志频率
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
    int result = -1;
    int module_state = L_Sim7805GetAbResult();
    ALOG_I("L_Sim7805GetAbResult() -> %s(%d)", L_Sim7805AbResultString(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_upgrade = True;
    g_mpu_upgrade.upgrade_timeout_ms = timeout > 0 ? timeout * 1000 : 10*60*1000; // 默认10min超时
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
    int module_state = L_Sim7805GetAbResult();
    ALOG_I("L_Sim7805GetAbResult() -> %s(%d)", L_Sim7805AbResultString(module_state), module_state);

    // 设置升级参数
    g_mpu_upgrade.req_backup = True;
    g_mpu_upgrade.backup_timeout_ms = timeout > 0 ? timeout * 1000 : 10*60*1000; // 默认10min超时
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
    char slot_info[8] = {0};

    if (partition_info != NULL)
    {
        memset(partition_info, 0, sizeof(TMpuPartitionInfo));
        result = ota_get_ab_slot_info(slot_info, sizeof(slot_info) - 1);
        
        if (result >= 0)
        {
            if (strncmp(slot_info, "A", 1) == 0)
            {
                partition_info->active_part = 'A';
            }
            else if (strncmp(slot_info, "B", 1) == 0)
            {
                partition_info->active_part = 'B';
            }
            else
            {
                ALOG_E("ota_get_ab_slot_info() value invalied: %s", slot_info);
                result = -1;
            }
        }
        else
        {
            ALOG_E("ota_get_ab_slot_info() failed with error code: %d", result);
        }
    }

    return result;
}




#endif  // #if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_SIMCOM7805)



