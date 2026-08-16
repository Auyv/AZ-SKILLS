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

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_CEVN)

#include "ofc/BaseTypes.h"              
#include "ffc/ffc_file_utils.h"

#define ALOG_TAG "Ag35Upgrade"
#include "alog/a_log.h"
#include "sfc/sfc_api.h"

#include "srt/srt.h"

#include "nf_upgrade_app.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

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
#define FOTA_IMG_PATH             "/media/card/update.zip" ///< 升级文件路径

#define FOTA_CMD_SET_PATH         "fotainfo -p /media/card"      ///< 升级文件路径设置指令
#define FOTA_CMD_GET_PATH         "fotainfo --get-package"             ///< 升级文件路径查询指令
#define FOTA_CMD_EXEC             "ab_full_update \"start-full-fota\"" ///< 升级执行指令
#define FOTA_CMD_GET_STATUS       "fotainfo --get-all"                ///< 升级状态查询指令
#define FOTA_CMD_GET_ACTIVEPART   "fotainfo --get-activepart"                ///< 当前分区查询指令


static const char *g_fota_state_string_[] = {
    "FOTA STAT:BACKUP (1)", ///< 开始刷写
    "FOTA STAT:BACKUP (2)", ///< 完成刷写等待重启 或 等待分区同步
    "FOTA STAT:SUCCEED", ///< 升级成功
    "FOTA STAT:FAILED", ///< 升级失败
};
static const int g_fota_state_string_count_ = sizeof(g_fota_state_string_) / sizeof(g_fota_state_string_[0]);

typedef enum TModuleFotaStateTag TModuleFotaState;
enum TModuleFotaStateTag
{
    kModuleFotaStateInvalid = -1,
    kModuleFotaStateBackup1 = 0,
    kModuleFotaStateBackup2 = 1,
    kModuleFotaStateSuccess = 2,
    kModuleFotaStateFail = 3,
};


/**
 * @brief 使用popen执行命令并获取命令输出
 * @param cmd:    要执行的命令
 * @param output: 命令输出
 * @param size:   输出缓存尺寸
 * @param status: 命令执行返回状态
 * @return True:  命令执行成功
 *         False: 命令执行失败
 */
static Bool L_FotaPipeExecCmd(const char *cmd, char *output, int size, int *status) 
{
    Bool result = False;
    FILE *fp;
    int total_len = 0;

    // 验证输入参数
    if (cmd == NULL || output == NULL || size <= 0 || status == NULL) {
        return False;
    }

    /// 设置SIGCHLD处理方式为默认值并记住原值
    __sighandler_t  old_handler = signal(SIGCHLD, SIG_DFL);
    
    // 检查信号设置是否成功
    if (old_handler == SIG_ERR) {
        return False;
    }

    /// 使用popen打开命令
    fp = popen(cmd, "r");
    if (fp != NULL) 
    {
        /// 读取命令输出结果
        char buffer[1024];
        while (total_len < size - 1) 
        {
            int remaining = size - 1 - total_len;
            if (remaining <= 0) {
                break;
            }
            
            // 限制单次读取大小，防止缓冲区溢出
            int read_size = (remaining > sizeof(buffer) - 1) ? sizeof(buffer) - 1 : remaining;
            if (fgets(buffer, read_size + 1, fp) == NULL) {
                break;
            }
            
            int line_len = strlen(buffer);
            if (line_len > 0 && (total_len + line_len) < size) {
                memcpy(output + total_len, buffer, line_len);
                total_len += line_len;
            } else {
                break;
            }
        }
        
        output[total_len] = '\0'; // 确保字符串结束

        /// 获取返回值
        int close_result = pclose(fp);
        *status = close_result;

        // 检查命令执行状态
        if (close_result != -1) {
            result = True;
        }
    }
    else
    {
        printf("[Fota] exec cmd: %.128s error:%.128s\n", cmd, strerror(errno));
    }

    /// 还原SIGCHLD处理
    signal(SIGCHLD, old_handler); 
    
    return result;
}

static Bool L_FotaCheckCmdExecResult(UInt32 status)
{
    Bool result = False;

    if (status != -1 && WIFEXITED(status) && 0 == WEXITSTATUS(status))
    {
        result = True;
    }

    return result;
}


static const char *L_ModuleOtaStateStr(TModuleFotaState state)
{
    const char *result = NULL;

    if (state >= kModuleFotaStateBackup1 && state <= kModuleFotaStateFail)
    {
        result = (const char *)g_fota_state_string_[state];
    }

    return result;
}

// 获取升级状态
TModuleFotaState L_Ag35ModuleGetAbFotaState()
{
    TModuleFotaState result = kModuleFotaStateInvalid;

    char cmd_output[512] = {0};
    int status = -1;
    if (L_FotaPipeExecCmd(FOTA_CMD_GET_STATUS, cmd_output, sizeof(cmd_output), &status)
        && L_FotaCheckCmdExecResult(status))
    {
        ALOG_T("%s: \n%.128s", FOTA_CMD_GET_STATUS, cmd_output);

        Int32 state;

        for (state = 0; state < g_fota_state_string_count_; state++)
        {
            if (strstr(cmd_output, g_fota_state_string_[state]))
            {
                result = (TModuleFotaState)state;
                break;
            }
        }
    }

    return result;
}


int L_Ag35ModuleDoBackup(UInt32 timeout_ms)
{
    int result = 0;

    // <2026-01-26> Jpz todo : 没有主动备份接口，直接返回完成
    L_MpuUpgradeDoBackupCallback(50);
    SfSleep(500);
    L_MpuUpgradeDoBackupCallback(100);

    return result;
}





static TMpuUpgradeState L_MpuUpgradeOnStateInvalid(void)
{
    // 刚开机. 检查分区同步情况是否需要执行
    TMpuUpgradeState next_state = kMpuUpgradeStateInvalid;

    // 等待属性同步完成
    Int32 nf_state = NfUpgradeState();

    if (nf_state == kNfUpgradeStateInit)
    {
        ALOG_I("NF upgrade state: %d", nf_state);
        next_state = kMpuUpgradeStateInvalid;
    }
    else 
    {
        TModuleFotaState fota_state = L_Ag35ModuleGetAbFotaState();
        
        if (fota_state == kModuleFotaStateSuccess)
        {
            next_state = kMpuUpgradeStateUpdateSuccess;
        }
        else if (fota_state == kModuleFotaStateFail)
        {
            next_state = kMpuUpgradeStateUpdateFailed;
        }
        else if (fota_state == kModuleFotaStateBackup1
            || fota_state == kModuleFotaStateBackup2)
        {
            next_state = kMpuUpgradeStateWaitConfirm;
        }
        else
        {
            next_state = kMpuUpgradeStateIdle;
        }
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
    TMpuUpgradeState next_state = kMpuUpgradeStateWaitReboot;

    g_mpu_upgrade.req_upgrade = False;  // 升级请求标志位. 开始执行前重置为False

    L_MpuUpgradeDoUpgradeCallback(1);

    char cmd_output[512] = {0};
    int status = -1;

    {   
        // 移动升级文件到FOTA路径
        char *img_path = MpuUpgradeImgPath();
        char cmd[128] = {0};
        snprintf(cmd, sizeof(cmd) - 1, "mv %s %s", img_path, FOTA_IMG_PATH);
        // 设置升级路径
        if (L_FotaPipeExecCmd(cmd, cmd_output, sizeof(cmd_output), &status)
            && L_FotaCheckCmdExecResult(status))
        {
            ALOG_I("FOTA exec cmd: %.128s", cmd);
        }
        else
        {
            ALOG_E("FOTA exec cmd: %.128s error:%.128s", cmd, strerror(errno));
            L_MpuUpgradeDoUpgradeCallback(-4);  // 移动文件失败
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }

    if (next_state != kMpuUpgradeStateUpdateFailed)
    {
        // 设置升级路径
        if (L_FotaPipeExecCmd(FOTA_CMD_SET_PATH, cmd_output, sizeof(cmd_output), &status)
            && L_FotaCheckCmdExecResult(status))
        {
            ALOG_I("FOTA exec cmd: %.128s", FOTA_CMD_SET_PATH);
            L_MpuUpgradeDoUpgradeCallback(2);
            SfSleep(1000);
        }
        else
        {
            ALOG_E("FOTA exec cmd: %.128s error:%.128s", FOTA_CMD_SET_PATH, strerror(errno));
            L_MpuUpgradeDoUpgradeCallback(-1);
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }

    if (next_state != kMpuUpgradeStateUpdateFailed)
    {
        // 执行升级
        if (L_FotaPipeExecCmd(FOTA_CMD_EXEC, cmd_output, sizeof(cmd_output), &status)
            && L_FotaCheckCmdExecResult(status))
        {
            ALOG_I("FOTA exec cmd: %.128s", FOTA_CMD_EXEC);
            L_MpuUpgradeDoUpgradeCallback(10);
            SfSleep(1000);
        }
        else
        {
            ALOG_E("FOTA exec cmd: %.128s error:%.128s", FOTA_CMD_EXEC, strerror(errno));
            L_MpuUpgradeDoUpgradeCallback(-1);
            next_state = kMpuUpgradeStateUpdateFailed;
        }
    }

    Int32 progress = 20;
    while (next_state != kMpuUpgradeStateUpdateFailed)
    {
        TModuleFotaState fota_state = L_Ag35ModuleGetAbFotaState();
        // 循环查询状态
        if (fota_state == kModuleFotaStateBackup2)
        {
            SfSleep(1000);
            L_MpuUpgradeDoUpgradeCallback(100);
            break;  // 分区刷写完成，等待重启
        }
        else if (fota_state == kModuleFotaStateFail)
        {
            ALOG_E("FOTA upgrade fail: %s", L_ModuleOtaStateStr(fota_state));
            L_MpuUpgradeDoUpgradeCallback(-1);
            next_state = kMpuUpgradeStateUpdateFailed;
            break;
        }
        else if (SfGetTickCount() - g_mpu_upgrade.upgrade_req_tick > g_mpu_upgrade.upgrade_timeout_ms)
        {
            ALOG_E("FOTA upgrade timeout: %s", L_ModuleOtaStateStr(fota_state));
            L_MpuUpgradeDoUpgradeCallback(-2);  // 升级超时失败
            next_state = kMpuUpgradeStateUpdateFailed;
            break;
        }
        else
        {
            L_MpuUpgradeDoUpgradeCallback(progress++);
            if (progress > 97)
                progress = 97;
            SfSleep(5000);
        }
    }

    return next_state;
}

static TMpuUpgradeState L_MpuUpgradeOnStateBackuping(void)
{
    UInt32 curr_tick = SfGetTickCount();
    TMpuUpgradeState next_state = kMpuUpgradeStateBackuping;

    g_mpu_upgrade.req_backup = False;

    
    if (L_Ag35ModuleDoBackup(g_mpu_upgrade.backup_timeout_ms) < 0)
    {
        next_state = kMpuUpgradeStateUpdateFailed;
    }
    else
    {
        next_state = kMpuUpgradeStateIdle;
    }

    return next_state;
}


static TMpuUpgradeState L_MpuUpgradeOnStateWaitConfirm(void)
{
    TMpuUpgradeState next_state = kMpuUpgradeStateUpdateSuccess;
    UInt32 start_tick = SfGetTickCount();

    while (next_state != kMpuUpgradeStateUpdateFailed)
    {
        TModuleFotaState fota_state = L_Ag35ModuleGetAbFotaState();
        // 循环查询状态
        if (fota_state == kModuleFotaStateSuccess)
        {
            SfSleep(1000);
            break;  // 分区刷写完成，等待重启
        }
        else if (fota_state == kModuleFotaStateFail)
        {
            next_state = kMpuUpgradeStateUpdateFailed;
        }
        else if (SfGetTickCount() - start_tick > 25*60*1000)
        {
            L_MpuUpgradeDoUpgradeCallback(-3);  // 确认超时失败
            next_state = kMpuUpgradeStateUpdateFailed;
            break;
        }
        else
        {
            SfSleep(5000);
        }
    }

    return next_state;
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
        char cmd_output[512] = {0};
        int status = -1;
        if (L_FotaPipeExecCmd(FOTA_CMD_GET_ACTIVEPART, cmd_output, sizeof(cmd_output), &status)
            && L_FotaCheckCmdExecResult(status))
        {
            Int32 flag = 0;

            ALOG_T("%s: \n%.128s", FOTA_CMD_GET_ACTIVEPART, cmd_output);
            
            if (strstr(cmd_output, "activerootfs:system") != NULL)
                flag |= (0x1 << 0);
            
            if (strstr(cmd_output, "activemodem:modem") != NULL)
                flag |= (0x1 << 1);

            if (strstr(cmd_output, "activebootimg:boot") != NULL)
                flag |= (0x1 << 2);

            result = 0;
            
            if (flag == 0x7)
                result = 'A';
            else if (flag == 0x0)
                result = 'B';
            else
                ALOG_E("unknown partition flag: %d", flag);
        }
        else
            ALOG_E("run %s failed", FOTA_CMD_GET_ACTIVEPART);
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
        ALOG_E("img_path too long: %.128s", img_path);
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

    // 设置升级参数
    g_mpu_upgrade.req_backup = True;
    g_mpu_upgrade.backup_timeout_ms = timeout > 0 ? timeout * 1000 : 10*60*1000; // 默认10min超时
    g_mpu_upgrade.backup_req_tick = SfGetTickCount();
    g_mpu_upgrade.backup_user_param = user_param;
    g_mpu_upgrade.backup_callback = callback;

    result = 0;

    return result;
}

#endif 


