/**
 * @file        mpu_upgrade_app.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/23
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "mpu_upgrade_app.h"
#include "upgrade_app_cfg.h"
#include "mpu_upgrade.h"

#define ALOG_TAG "MpuUpgrade"
#include "alog/a_log.h"

#include <string.h>
#include <stdio.h>

typedef struct TMpuUpgradeTag TMpuUpgrade;
struct TMpuUpgradeTag
{
    Int32 exec_perc;   // 执行进度      
    Int32 exec_state;  // 执行状态      @ TMpuUpgradeState
    Int32 exec_errcode;  // 执行错误码  @ TMpuUpgradeErr
};

static TMpuUpgrade g_mpu_upgrade_ = {0};

Int32 MpuUpgradeAppCreate(void)
{
    // <2024-12-23> Jpz : 获取 当前执行id. 用于断电重启后，继续执行
    memset(&g_mpu_upgrade_, 0, sizeof(g_mpu_upgrade_)); // 目前全部清零
    return MpuUpgradeInit();
}

void  MpuUpgradeAppDestroy(void)
{
    MpuUpgradeDone();
}

Int32 MpuUpgradeAppInit(void)
{
    TMpuPartitionInfo partition_info;

    if (MpuUpgradeGetPartitionInfo(&partition_info) >= 0)
    {
        ALOG_I("active_part:%c\r\n", partition_info.active_part);
        ALOG_I("other_part_is_damage:%s\r\n", partition_info.other_part_is_damage ? "yes" : "no");

        if (partition_info.other_part_is_damage)
        {
            g_mpu_upgrade_.exec_state = kMpuUpgradeStateUpdateFailed;
            MpuUpgradeExecBackup(60*20, NULL, NULL);
        }
    }

    g_mpu_upgrade_.exec_state = MpuUpgradeState();

    return MpuUpgradeSetImgPath(MPU_UPGRADE_FILE_NAME);
}

void  MpuUpgradeAppDone(void)
{

}

Int32 MpuUpgradeAppRunOnce(void)
{
    MpuUpgradeRunOnce();
    g_mpu_upgrade_.exec_state = MpuUpgradeState();

    return 0;
}



static void L_MpuUpgradeCallback(TMpuUpgradeState state, int percent, void *user_param)
{
    TMpuUpgrade *sender = (TMpuUpgrade *)user_param;

    if (sender == &g_mpu_upgrade_)
    {
        sender->exec_state = state;
        sender->exec_perc = percent;

        if (percent < 0)
            sender->exec_errcode = percent;
    }
    else
    {
        printf("????? line:%d\r\n", __LINE__);
    }
}

/**
 * 请求执行mpu自升级
 * @return 
 * @note:xxxxx
 */
Int32 MpuUpgradeRequest(void)
{
    Int32 result = MpuUpgradeExecUpdate(60*15, &g_mpu_upgrade_, L_MpuUpgradeCallback);

    return result;
}

/**
 * 获取升级状态
 * @param task_id    任务ID
 * @param *state     升级状态
 * @param *perc      升级进度
 * @param *errcode   错误码
 * @return >= 0:成功
 *         <  0:失败
 * @note:xxxxx
 */
Int32 MpuUpgradeResponse(Int32 *state, Int32 *perc, Int32 *errcode)
{
    if (state)
        *state = g_mpu_upgrade_.exec_state;

    if (perc)
        *perc = g_mpu_upgrade_.exec_perc;

    if (errcode)
        *errcode = g_mpu_upgrade_.exec_errcode;

    return 1;
}


///< Generated on "2024-12-23 17:00:29" by the tool "gen_hq_file.py >> V20231119" 

