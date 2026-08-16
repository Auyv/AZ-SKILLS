#include "hsed_app.h"
#include "hsed_srv.h"
#include "hsed_spc_adapter.h"

// ---------------- const and variable ----------------
static int g_ref_count_ = 0;
static int g_state_ = kHsedAppState_Invalid;
static int g_app_id_ = 0;

// ---------------- topic function ----------------
static int __stdcall L_OnAccept(void *owner, int app_id, void *conn_id)
{
    int result = SC_rAccessIllegal;
    (void)owner;

    if ((app_id == g_app_id_) &&
        (HsedSpcAdapterAccept(conn_id) == True))
    {
        result = SC_rSuccess;
    }
    return result;
}

static void __stdcall L_OnFinish(void *owner, int app_id, void *conn_id)
{
    (void)owner;
    if (app_id == g_app_id_)
    {
        HsedSpcAdapterFinish(conn_id);
    }
}

// ---------------- inside function ----------------

static void L_SetAppAttrib(int app_id)
{
    // set property
    SCAppSetInt(app_id, SC_aaListenQueue, 8);
    SCAppSetInt(app_id, SC_aaMaxConnCount, 8);
    SCAppSetInt(app_id, SC_aaReqQueue, 16);
    SCAppSetInt(app_id, SC_aaRespThreads, 1);
    SCAppSetInt(app_id, SC_aaRecvThreads, 1);
    SCAppSetInt(app_id, SC_aaCacheThreads, 1);

    // set topic function
    SCAppSetObj(app_id, SC_aaOnAccept, (void*)&L_OnAccept);
    SCAppSetObj(app_id, SC_aaOnFinish, (void*)&L_OnFinish);
}

static void L_AppClose(int app_id)
{
    Bool next = False;

    if (app_id <= 0)
        ;
    else if (g_state_ == kHsedAppState_Opened)
    {
        g_state_ = kHsedAppState_Closing;
        next = True;
    }
    else if (g_state_ == kHsedAppState_Opening)
        g_state_ = kHsedAppState_Closing;

    if (next)
    {
        SCAppClose(app_id);
        if (app_id == g_app_id_ || g_app_id_ == 0)
            g_state_ = kHsedAppState_Invalid;
    }
}

// ---------------- outside function ----------------

void HcHsedApp_Init(void)
{
    if (g_ref_count_ == 0)
    {
        g_ref_count_ = 1;

        g_app_id_ = SCAppCreate("hsed", "");
        if (g_app_id_ > 0)
        {
            g_state_ = kHsedAppState_Inactive; 
            L_SetAppAttrib(g_app_id_);



            HcHsedSrv_LoadDefs(NULL, g_app_id_);
        }
        else
            g_state_ = kHsedAppState_Invalid;
    }
}

void HcHsedApp_Fini(Bool forced)
{
    Bool next = False;
    int  app_id = 0;

    if (g_ref_count_ <= 0)
        ;
    else if (--g_ref_count_ == 0 || forced)
    {
        g_ref_count_ = 0;
        if (g_app_id_ > 0)
        {
            next = True;
            app_id = g_app_id_;
            g_app_id_ = 0;
        }
    }

    if (next)
    {
        L_AppClose(app_id);

        SCAppFree(app_id);
    }
}

int HcHsedApp_Open(void)
{
    Bool next = False;
    int  app_id = 0;
    int  result = SC_rStateInvalid;

    if (g_app_id_ <= 0)
        result = SC_rHandleInvalid;
    else if (g_state_ == kHsedAppState_Inactive)
    {
        g_state_ = kHsedAppState_Opening;
        app_id = g_app_id_;
        next = True;
    }

    if (next)
    {
        next = False;
        result = SCAppOpen(app_id);

        if (g_state_ != kHsedAppState_Opening || app_id != g_app_id_)
            next = True;
        else if (result == SC_rSuccess)
            g_state_ = kHsedAppState_Opened;
        else
            g_state_ = kHsedAppState_Inactive;

        if (next)
        {
            if (result == SC_rSuccess)
            {
                SCAppClose(app_id);
                result = SC_rIsRefused;
            }

            if (app_id == g_app_id_ || g_app_id_ == 0)
                g_state_ = kHsedAppState_Inactive;
        }
    }

    return result;
}

void HcHsedApp_Close(void)
{
    L_AppClose(g_app_id_);
}

int HcHsedApp_State(void)
{
    return g_state_;
}

int HcHsedApp_AppId(void)
{
    return g_app_id_;
}
