#include "hsed_conn.h"
// ---------------- topic function ----------------

static void __stdcall L_OnConnect(void *owner, void *conn_id)
{
    (void)owner;
    (void)conn_id;
}

static void __stdcall L_OnDisconnect(void *owner, void *conn_id)
{
    (void)owner;
    (void)conn_id;
}

static void __stdcall L_OnNotify(void *owner, void *conn_id, int topic, const void *info, int size)
{
    (void)owner;
    (void)conn_id;
    (void)topic;
    (void)info;
    (void)size;
}

static void __stdcall L_OnRecvData(void *owner, void *conn_id, const void *data, int size)
{
    (void)owner;
    (void)conn_id;
    (void)data;
    (void)size;
}

// ---------------- inside function ----------------

static void L_SetConnAttrib(void *owner, void *conn_id)
{
    // set property
    SCConnSetStr(conn_id, SC_caAppName, "hsed", -1);
    SCConnSetStr(conn_id, SC_caAppPass, "", -1);
    SCConnSetObj(conn_id, SC_caOwner, owner);


    // set topic function
    SCConnSetObj(conn_id, SC_caOnConnect, (void*)&L_OnConnect);
    SCConnSetObj(conn_id, SC_caOnDisconnect, (void*)&L_OnDisconnect);
    SCConnSetObj(conn_id, SC_caOnNotify, (void*)&L_OnNotify);
    SCConnSetObj(conn_id, SC_caOnRecvData, (void*)&L_OnRecvData);
}

// ---------------- outside function ----------------

void *HcHsedConn_Create(void *owner)
{
    void *result = SCConnCreate(1);
    if (result != NULL)
    {
        L_SetConnAttrib(owner, result);
    }
    return result;
}

void HcHsedConn_Free(void *conn_id)
{
    SCConnFree(conn_id);
}

int HcHsedConn_Open(void *conn_id)
{
    return SCConnOpen(conn_id);
}

void HcHsedConn_Close(void *conn_id)
{
    SCConnClose(conn_id);
}

int HcHsedConn_State(void *conn_id)
{
    return SCConnGetInt(conn_id, SC_caState, NULL);
}

