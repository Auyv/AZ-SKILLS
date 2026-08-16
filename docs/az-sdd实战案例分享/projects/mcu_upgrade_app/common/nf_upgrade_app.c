/**
 * @file        nf_upgrade_app.c
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
 
#include "nf_upgrade_app.h"
#include "nf_upgrade_state_exec.h"
#include "sfc/sfc_api.h"
#include "ffc/ffc_file_stream.h"
#include "ffc/ffc_file.h"
#include "img_upgrade_file.h"
#include "srt/srt.h"
#include "srt_pid_type.h"

#define ALOG_TAG "nfupgrade"
#include "alog/a_log.h"
#include "file_log.h"

#include "nf_upgrade_app.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

static TNfUpgradeApp g_nfupgrade_app_ = {0};

// -----------------------------------------------------------------------




Int32 NfUpgradeAppCreate(void)
{
    memset(&g_nfupgrade_app_, 0, sizeof(g_nfupgrade_app_));
    return 0;
}

void  NfUpgradeAppDestroy(void)
{

}

Int32 NfUpgradeAppInit(void)
{
    UInt8 curr_state = g_nfupgrade_app_.m_state;

    if (curr_state >= kNfUpgradeStateMax)
    {
        curr_state = kNfUpgradeStateInit;
        g_nfupgrade_app_.m_state = curr_state;
    }
        
    g_nfupgrade_state_exec[curr_state].DoEnter(&g_nfupgrade_app_);

    return 0;
}

void  NfUpgradeAppDone(void)
{
    // <2024-12-28> Jpz todo : 保存当前执行状态

}

Int32 NfUpgradeAppRunOnce(void)
{
    Int32 result = 0;
    UInt8 curr_state = g_nfupgrade_app_.m_state;
    UInt8 next_state;

    if (curr_state >= kNfUpgradeStateMax)
        curr_state = kNfUpgradeStateInit;

    result = g_nfupgrade_state_exec[curr_state].DoRunOnce(&g_nfupgrade_app_);

    next_state = g_nfupgrade_state_exec[curr_state].DoGoto(&g_nfupgrade_app_);

    if (next_state != curr_state)
    {
        g_nfupgrade_app_.m_prev_state = curr_state;
        g_nfupgrade_app_.m_state_enter_tick = SfGetTickCount();
        g_nfupgrade_app_.m_state = next_state;

        ALOG_I("NfUpgradeStateChg: %d-%s -> %d-%s", curr_state, g_nfupgrade_state_exec[curr_state].name, next_state, g_nfupgrade_state_exec[next_state].name);
        g_nfupgrade_state_exec[next_state].DoEnter(&g_nfupgrade_app_);

        FileLogFlush(); // 保证每次状态切换时，log数据都写入文件
    }

    
    NfUpgradeAppFlushStrData();

    return result;
}


Int32 NfUpgradeState()
{
    return g_nfupgrade_app_.m_state;
}


Int32 NfUpgradeRequest(Int32 req_id)
{
    Int32 result = -1;
    Int32 m_state = NfUpgradeState();
    
    if (g_nfupgrade_app_.req_id != req_id)
    {
        if (m_state == kNfUpgradeStateIdle
            || m_state == kNfUpgradeStateSuccess
            || m_state == kNfUpgradeStateFailed)
        {
            if (req_id >= 0)
                g_nfupgrade_app_.req_id = req_id;
            else
                g_nfupgrade_app_.req_id += 1;
            result = 1;
        }
    }
    // else  当前请求未执行完成

    return result;
}

Int32 NfUpgradeResponse(Int32 *state, Int32 *perc, Int32 *errcode)
{
    if (state != NULL)
        *state = g_nfupgrade_app_.m_state;
    if (perc != NULL)
        *perc = g_nfupgrade_app_.upgrade_precent;
    if (errcode != NULL)
        *errcode = g_nfupgrade_app_.m_errcode;
    return 0;
}


Bool NfUpgradeAppFlushStrData(void)
{
    SrtSetValueU08(545/*kSidUInt8NfUpgradeRspId*/, g_nfupgrade_app_.rsp_id);
    SrtSetValueU08(546/*kSidUInt8NfUpgradeState*/, g_nfupgrade_app_.m_state);
    SrtSetValueU08(547/*kSidUInt8NfUpgradePrecent*/, g_nfupgrade_app_.upgrade_precent);
    SrtSetValueU32(548/*kSidUInt32NfUpgradeErrCode*/, g_nfupgrade_app_.m_errcode);
    SrtSetValueU16(549/*kSidUInt16NfUpgradeFlag*/, g_nfupgrade_app_.m_flag);

    UInt8 tmp_id;
    Int32 ret;

    if (SrtGetValueU08(544/*kSidUInt8NfUpgradeReqId*/, &tmp_id) >= 0)
    {
        ret = NfUpgradeRequest(tmp_id);
        if (ret >= 0)
        {
            ALOG_I("SrtSet NfUpgradeRequest Success:%d, req_id:%d, state:%d", ret, g_nfupgrade_app_.req_id, g_nfupgrade_app_.m_state);
        }
        else
        {
            // ALOG_E("Srt NfUpgradeRequest Failed:%d, rsp_id:%d, state:%d", ret, g_nfupgrade_app_.rsp_id, g_nfupgrade_app_.m_state);
            // SrtSetValueU08(3351/*kSidUInt8NfUpgradeReqId*/, g_nfupgrade_app_.rsp_id);
        }
    }

    if (SrtGetValueU08(550/*kSidUInt8NfUpgradeCancelId*/, &tmp_id) >= 0)
    {
        if (tmp_id == g_nfupgrade_app_.rsp_id)
        {
            Int32 m_state = NfUpgradeState();
        
            // 取消升级
            if (m_state != kNfUpgradeStateIdle
                && m_state != kNfUpgradeStateSuccess
                && m_state != kNfUpgradeStateFailed)
            {
                g_nfupgrade_app_.m_prev_state = m_state;
                g_nfupgrade_app_.m_state_enter_tick = SfGetTickCount();
                g_nfupgrade_app_.m_state = kNfUpgradeStateFailed;

                ALOG_W("Srt Cancel Upgrade id:%d: %d-%s -> %d-%s", tmp_id, m_state, g_nfupgrade_state_exec[m_state].name, kNfUpgradeStateFailed, g_nfupgrade_state_exec[kNfUpgradeStateFailed].name);
                g_nfupgrade_state_exec[kNfUpgradeStateFailed].DoEnter(&g_nfupgrade_app_);
            }
        }
    }

    return True;
}

Bool NfUpgradeAppRecover(void)
{
    // Bool result = True;

    // if (SrtGetValueU08(kSidUInt8NfUpgradeReqId, &g_nfupgrade_app_.req_id) < 0)
    //     g_nfupgrade_app_.req_id = 0;

    if (SrtGetValueU08(545/*kSidUInt8NfUpgradeRspId*/, &g_nfupgrade_app_.rsp_id) < 0)
        g_nfupgrade_app_.rsp_id = 0;

    if (SrtGetValueU08(546/*kSidUInt8NfUpgradeState*/, &g_nfupgrade_app_.m_state) < 0)
        g_nfupgrade_app_.m_state = kNfUpgradeStateIdle;
    
    if (SrtGetValueU08(547/*kSidUInt8NfUpgradePrecent*/, &g_nfupgrade_app_.upgrade_precent) < 0)
        g_nfupgrade_app_.upgrade_precent = 0;
    
    if (SrtGetValueU32(548/*kSidUInt32NfUpgradeErrCode*/, &g_nfupgrade_app_.m_errcode) < 0)
        g_nfupgrade_app_.m_errcode = 0;
    
    if (SrtGetValueU16(549/*kSidUInt16NfUpgradeFlag*/, &g_nfupgrade_app_.m_flag) < 0)
        g_nfupgrade_app_.m_flag = 0;

    ALOG_I("NfUpgradeAppRecover: rsp_id:%d, state:%d, precent:%d, errcode:%d, flag:%d", 
        g_nfupgrade_app_.rsp_id, 
        g_nfupgrade_app_.m_state, 
        g_nfupgrade_app_.upgrade_precent, 
        g_nfupgrade_app_.m_errcode, 
        g_nfupgrade_app_.m_flag);

    return True;
}


