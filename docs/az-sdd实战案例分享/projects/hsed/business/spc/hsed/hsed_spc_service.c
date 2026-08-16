/**
 * @file        hsed_spc_service.c
 * @brief       HSED SPC 服务初始化、打开和关闭
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "hsed_spc_service.h"

#include "generated/hsed_app.h"
#include "hsed_spc_adapter.h"

#include "bsc_hse/bsc_hse_types.h"
#include "SpanCall.h"

#define ALOG_TAG "HSED_SPC"
#include "alog/alog.h"

static Bool g_spc_initialized_ = False;
static Bool g_spc_app_initialized_ = False;
static Bool g_spc_started_ = False;

Int32 HsedSpcServiceStart(void)
{
    Int32 result = kBscHseResultDeviceUnavailable;

    if (g_spc_started_ == True)
    {
        result = kBscHseResultOk;
    }
    else if (HsedSpcAdapterInit() == False)
    {
        result = kBscHseResultNoResource;
    }
    else
    {
        SCInit();
        g_spc_initialized_ = True;

        HcHsedApp_Init();
        if (HcHsedApp_AppId() <= 0)
        {
            ALOG_E("SPC application create failed");
        }
        else
        {
            g_spc_app_initialized_ = True;
            if (HcHsedApp_Open() != SC_rSuccess)
            {
                ALOG_E("SPC application open failed");
            }
            else
            {
                g_spc_started_ = True;
                result = kBscHseResultOk;
                ALOG_I("SPC service started: hsed");
            }
        }
    }

    if (result != kBscHseResultOk)
    {
        HsedSpcServiceStop();
    }
    return result;
}

void HsedSpcServiceStop(void)
{
    if (g_spc_started_ == True)
    {
        HcHsedApp_Close();
        g_spc_started_ = False;
    }
    if (g_spc_app_initialized_ == True)
    {
        HcHsedApp_Fini(False);
        g_spc_app_initialized_ = False;
    }
    if (g_spc_initialized_ == True)
    {
        SCFini();
        g_spc_initialized_ = False;
    }
    HsedSpcAdapterDone();
}
