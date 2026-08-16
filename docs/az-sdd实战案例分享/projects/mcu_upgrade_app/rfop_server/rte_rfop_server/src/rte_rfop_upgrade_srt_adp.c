/**
 * @file        rte_rfop_upgrade_srt_adp.c
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

#include "rte_rfop_upgrade_srt_adp.h"
#include "srt_pid_type.h"
#include "srt/srt.h"
#include "sfc/sfc_api.h"
#include "rte_rfop_upgrade.h"

#define ALOG_TAG "RfopSrtAdp"
#include "alog/alog.h"

/****************************************************************************************************
 * <2024-07-25> Jpz  : 请求的升级文件名称
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetReqImgFilename(UInt8 *val, UInt32 size)
{
    Int32 result = SrtGetValueMem(kSidBinRfopUpgradeReqImgFilename, val, size);
    if (result < 0 )
    {
        ALOG_E("RteRfopUpgradeSrtAdpGetReqImgFilename result:%d", result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetReqImgFilename(UInt8 *val, UInt32 size)
{
    Int32 result = SrtSetValueMem(kSidBinRfopUpgradeReqImgFilename, (const UInt8 *)val, size);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 请求开始的升级任务ID
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetReqStartTaskId(UInt8 *val)
{
    Int32 result = SrtGetValueU08(kSidUInt8RfopUpgradeReqStartTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetReqStartTaskId(UInt8 val)
{
    Int32 result = SrtSetValueU08(kSidUInt8RfopUpgradeReqStartTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 请求取消的升级任务ID
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetReqCancelTaskId(UInt8 *val)
{
    Int32 result = SrtGetValueU08(kSidUInt8RfopUpgradeReqCancleTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetReqCancelTaskId(UInt8 val)
{
    Int32 result = SrtSetValueU08(kSidUInt8RfopUpgradeReqCancleTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 获取当前的升级任务ID
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetCurTaskId(UInt8 *val)
{
    Int32 result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetCurTaskId(UInt8 val)
{
    Int32 result = SrtSetValueU08(kSidUInt8RfopUpgradeCurTaskId, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 获取当前的升级任务状态
 ****************************************************************************************************/
static UInt8    g_rfop_upgrade_cur_task_state_ = 0;
static UInt8    g_rfop_upgrade_srt_task_state_ = 0;
static Bool     g_rfop_upgrade_check_state_flag_ = 0;
static UInt32   g_rfop_upgrade_check_state_tick_ = 0;

Int32 RteRfopUpgradeSrtAdpGetCurTaskState(UInt8 *val)
{
    Int32 result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskState, &g_rfop_upgrade_srt_task_state_);
    if (result < 0)
    {
        if (val != NULL)
            *val = g_rfop_upgrade_cur_task_state_;
        ALOG_E("%s result:%d", __func__, result);
    }
    else if (g_rfop_upgrade_srt_task_state_ == kRteRfopUpgradeStateFailed)
    {
        // 升级失败状态需要延时5s后上报, 保证日志信息打印完整
        if (g_rfop_upgrade_cur_task_state_ != g_rfop_upgrade_srt_task_state_ && !g_rfop_upgrade_check_state_flag_)
        {
            g_rfop_upgrade_check_state_flag_ = True;
            g_rfop_upgrade_check_state_tick_ = SfGetTickCount();
        }

        if (g_rfop_upgrade_check_state_flag_ && SfGetTickCount() - g_rfop_upgrade_check_state_tick_ > 5000)
        {
            g_rfop_upgrade_check_state_flag_ = False;
            g_rfop_upgrade_cur_task_state_ = g_rfop_upgrade_srt_task_state_;
        }
    }
    else
    {
        if (g_rfop_upgrade_check_state_flag_)
        {
            ALOG_E("rfop_upgrade_state jump: %d -> 9 -> %d", g_rfop_upgrade_cur_task_state_, g_rfop_upgrade_srt_task_state_);
        }

        g_rfop_upgrade_cur_task_state_ = g_rfop_upgrade_srt_task_state_;
        g_rfop_upgrade_check_state_flag_ = False;
    }

    if (val != NULL)
    {
        *val = g_rfop_upgrade_cur_task_state_;
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetCurTaskState(UInt8 val)
{
    Int32 result = SrtSetValueU08(kSidUInt8RfopUpgradeCurTaskState, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    else
        g_rfop_upgrade_cur_task_state_ = val;
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 获取当前的升级任务下载进度
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetCurTaskPercent(Int8 *val)
{
    Int32 result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskPercent, (UInt8 *)val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetCurTaskPercent(Int8 val)
{
    Int32 result = SrtSetValueU08(kSidUInt8RfopUpgradeCurTaskPercent, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2024-07-25> Jpz  : 获取当前的升级任务结果
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetCurTaskResult(Int32 *val)
{
    Int32 result = SrtGetValueU32(kSidUInt32RfopUpgradeCurTaskResult, (UInt32 *)val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}
Int32 RteRfopUpgradeSrtAdpSetCurTaskResult(Int32 val)
{
    Int32 result = SrtSetValueU32(kSidUInt32RfopUpgradeCurTaskResult, val);
    if (result < 0)
    {
        ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}



/****************************************************************************************************
 * <2024-07-25> Jpz  : 获取当前设备硬件版本号
 ****************************************************************************************************/
Int32 RteRfopUpgradeSrtAdpGetHardwareVersion(UInt32 *val)
{
    Int32 result = SrtGetValueU32(kSidUInt32DevHardwareVersion, val);
    if (result < 0)
    {
        // ALOG_E("%s result:%d", __func__, result);
    }
    return result;
}


/****************************************************************************************************
 * <2025-07-03> Jpz  : 通过日志输出全部相关属性值
 ****************************************************************************************************/
void RteRfopUpgradeSrtShowAlldata(void)
{
    char vbuf[128] = {0};
    UInt32 val32 = 0;
    UInt16 val16 = 0;
    UInt8 val8 = 0;
    Int32 result = 0;

    result = SrtGetValueU08(kSidUInt8NetModuleFotaState, &val8);
    ALOG_I("srtget result:%d >> NetModuleFotaState:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8NetModuleFotaConfirm, &val8);
    ALOG_I("srtget result:%d >> NetModuleFotaConfirm:%d", result, val8);

    result = SrtGetValueMem(kSidBinRfopUpgradeReqImgFilename, vbuf, sizeof(vbuf) - 1);
    if (result >= 0)
        vbuf[result] = 0;
    ALOG_I("srtget result:%d >> RfopUpgradeReqImgFilename:%s", result, vbuf);

    result = SrtGetValueU08(kSidUInt8RfopUpgradeReqStartTaskId, &val8);
    ALOG_I("srtget result:%d >> RfopUpgradeReqStartTaskId:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8RfopUpgradeReqCancleTaskId, &val8);
    ALOG_I("srtget result:%d >> RfopUpgradeReqCancleTaskId:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskId, &val8);
    ALOG_I("srtget result:%d >> RfopUpgradeCurTaskId:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskState, &val8);
    ALOG_I("srtget result:%d >> RfopUpgradeCurTaskState:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8RfopUpgradeCurTaskPercent, &val8);
    ALOG_I("srtget result:%d >> RfopUpgradeCurTaskPercent:%d", result, val8);

    result = SrtGetValueU32(kSidUInt32RfopUpgradeCurTaskResult, &val32);
    ALOG_I("srtget result:%d >> RfopUpgradeCurTaskResult:%d", result, val32);

    result = SrtGetValueU08(kSidUInt8NfUpgradeReqId, &val8);
    ALOG_I("srtget result:%d >> NfUpgradeReqId:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8NfUpgradeRspId, &val8);
    ALOG_I("srtget result:%d >> NfUpgradeRspId:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8NfUpgradeState, &val8);
    ALOG_I("srtget result:%d >> NfUpgradeState:%d", result, val8);

    result = SrtGetValueU08(kSidUInt8NfUpgradePrecent, &val8);
    ALOG_I("srtget result:%d >> NfUpgradePrecent:%d", result, val8);

    result = SrtGetValueU32(kSidUInt32NfUpgradeErrCode, &val32);
    ALOG_I("srtget result:%d >> NfUpgradeErrCode:%d", result, val32);

    result = SrtGetValueU16(kSidUInt16NfUpgradeFlag, &val16);
    ALOG_I("srtget result:%d >> NfUpgradeFlag:%d", result, val16);

    result = SrtGetValueU08(kSidUInt8NfUpgradeCancelId, &val8);
    ALOG_I("srtget result:%d >> NfUpgradeCancelId:%d", result, val8);
}


void RteRfopUpgradeSrtNotifyUpgradeStart(void)
{
    UInt8 v8 = 0;

    if (SrtGetValueU08(kSidUInt8GprsModuleFuncDisable, &v8) >= 0)
    {
        v8 |= 0x1;
        SrtSetValueU08(kSidUInt8GprsModuleFuncDisable, v8);
        ALOG_I("%s set GprsModuleFuncDisable:%d", __func__, v8);
    }

}



///< Generated on "2024-07-23 11:11:59" by the tool "gen_hq_file.py >> V20231119"

