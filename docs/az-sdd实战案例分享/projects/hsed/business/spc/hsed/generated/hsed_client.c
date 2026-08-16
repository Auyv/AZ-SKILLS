#include "hsed_client.h"
#include "hsed_conn.h"
#include <string.h>

enum 
{
   kTimeout = 30000,
};

static void *g_conn_id_ = NULL;

int HcHsed_Init( void )
{
    int result;

    if (g_conn_id_ == NULL)
    {
        void *conn_id = HcHsedConn_Create(NULL);
        if (conn_id != NULL)
        {
            g_conn_id_ = conn_id;
            result = SC_rSuccess;
        }
        else
            result = SC_rFailure;
    }
    else
        result = SC_rIsExisted;

    return result;
}

void HcHsed_Done( void )
{
    if (g_conn_id_ != NULL)
    {
        HcHsed_Close();
        HcHsedConn_Free(g_conn_id_);
        g_conn_id_ = NULL;
    }
}

int HcHsed_Open( void )
{
    return g_conn_id_ != NULL ? HcHsedConn_Open(g_conn_id_) : SC_rNotExist;
}

void HcHsed_Close( void )
{
    if (g_conn_id_ != NULL)
        HcHsedConn_Close(g_conn_id_);
}

int HcHsed_ConnState( void )
{
    return g_conn_id_ != NULL ? HcHsedConn_State(g_conn_id_) : SC_rNotExist;
}

int HcHsed_HseCreate(int *_ret_, TInt64 *_handle)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "HseCreate");
        if (l_result == SC_rSuccess)
        {
            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_handle != NULL)
                    *_handle = SCCmdFieldAsInt64(l_cmd_id, 0, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_HseDestroy(TInt64 handle, int *_ret_)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "HseDestroy");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_SetPropertyInt(TInt64 handle, int property_id, int value, int *_ret_)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "SetPropertyInt");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, property_id);
            SCCmdParamByInt(l_cmd_id, 2, value);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_SetPropertyBytes(TInt64 handle, int property_id, unsigned char *value, int count_value, int *_ret_)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "SetPropertyBytes");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, property_id);
            SCCmdParamByStr(l_cmd_id, 2, (const char *)value, sizeof(*value) * count_value);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_GetPropertyInt(TInt64 handle, int property_id, int *_ret_, int *_value)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "GetPropertyInt");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, property_id);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_value != NULL)
                    *_value = SCCmdFieldAsInt(l_cmd_id, 0, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_GetPropertyBytes(TInt64 handle, int property_id, int *_ret_, unsigned char *_value, int *_count_value)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "GetPropertyBytes");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, property_id);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_value != NULL && _count_value != NULL)
                {
                    int n_size;
                    char *p_item = SCCmdFieldAsStr(l_cmd_id, 0, &n_size);
                    int n_count = n_size / sizeof(*_value);
                    if (p_item == NULL)
                        *_count_value = n_count;
                    else if (*_count_value > n_count)
                    {
                        memcpy(_value, p_item, n_count * sizeof(*_value));
                        *_count_value = n_count;
                    }
                    else if (*_count_value > 0)
                        memcpy(_value, p_item, *_count_value * sizeof(*_value));
                }
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_Encrypt(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_output_data, int *_count_output_data)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "Encrypt");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, algorithm);
            SCCmdParamByStr(l_cmd_id, 2, (const char *)input_data, sizeof(*input_data) * count_input_data);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_output_data != NULL && _count_output_data != NULL)
                {
                    int n_size;
                    char *p_item = SCCmdFieldAsStr(l_cmd_id, 0, &n_size);
                    int n_count = n_size / sizeof(*_output_data);
                    if (p_item == NULL)
                        *_count_output_data = n_count;
                    else if (*_count_output_data > n_count)
                    {
                        memcpy(_output_data, p_item, n_count * sizeof(*_output_data));
                        *_count_output_data = n_count;
                    }
                    else if (*_count_output_data > 0)
                        memcpy(_output_data, p_item, *_count_output_data * sizeof(*_output_data));
                }
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_Decrypt(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_output_data, int *_count_output_data)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "Decrypt");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, algorithm);
            SCCmdParamByStr(l_cmd_id, 2, (const char *)input_data, sizeof(*input_data) * count_input_data);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_output_data != NULL && _count_output_data != NULL)
                {
                    int n_size;
                    char *p_item = SCCmdFieldAsStr(l_cmd_id, 0, &n_size);
                    int n_count = n_size / sizeof(*_output_data);
                    if (p_item == NULL)
                        *_count_output_data = n_count;
                    else if (*_count_output_data > n_count)
                    {
                        memcpy(_output_data, p_item, n_count * sizeof(*_output_data));
                        *_count_output_data = n_count;
                    }
                    else if (*_count_output_data > 0)
                        memcpy(_output_data, p_item, *_count_output_data * sizeof(*_output_data));
                }
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_Sign(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_signature, int *_count_signature, unsigned int *_r_size, unsigned int *_s_size)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "Sign");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, algorithm);
            SCCmdParamByStr(l_cmd_id, 2, (const char *)input_data, sizeof(*input_data) * count_input_data);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_signature != NULL && _count_signature != NULL)
                {
                    int n_size;
                    char *p_item = SCCmdFieldAsStr(l_cmd_id, 0, &n_size);
                    int n_count = n_size / sizeof(*_signature);
                    if (p_item == NULL)
                        *_count_signature = n_count;
                    else if (*_count_signature > n_count)
                    {
                        memcpy(_signature, p_item, n_count * sizeof(*_signature));
                        *_count_signature = n_count;
                    }
                    else if (*_count_signature > 0)
                        memcpy(_signature, p_item, *_count_signature * sizeof(*_signature));
                }
                
                if (_r_size != NULL)
                    *_r_size = SCCmdFieldAsInt(l_cmd_id, 1, NULL);
                
                if (_s_size != NULL)
                    *_s_size = SCCmdFieldAsInt(l_cmd_id, 2, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_ExportPublicKey(TInt64 handle, int algorithm, int *_ret_, unsigned char *_public_key, int *_count_public_key)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "ExportPublicKey");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, algorithm);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                
                if (_public_key != NULL && _count_public_key != NULL)
                {
                    int n_size;
                    char *p_item = SCCmdFieldAsStr(l_cmd_id, 0, &n_size);
                    int n_count = n_size / sizeof(*_public_key);
                    if (p_item == NULL)
                        *_count_public_key = n_count;
                    else if (*_count_public_key > n_count)
                    {
                        memcpy(_public_key, p_item, n_count * sizeof(*_public_key));
                        *_count_public_key = n_count;
                    }
                    else if (*_count_public_key > 0)
                        memcpy(_public_key, p_item, *_count_public_key * sizeof(*_public_key));
                }
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}


int HcHsed_ImportPublicKey(TInt64 handle, int algorithm, unsigned char *public_key, int count_public_key, Bool replace_existing_key, int *_ret_)
{
    int l_result = SC_rHandleInvalid;
    void *l_cmd_id = g_conn_id_ != NULL ? SCCmdCreate() : NULL;

    if (l_cmd_id != NULL)
    {
        l_result = SCCmdBegin(l_cmd_id, g_conn_id_, "ImportPublicKey");
        if (l_result == SC_rSuccess)
        {
            SCCmdParamByInt64(l_cmd_id, 0, handle);
            SCCmdParamByInt(l_cmd_id, 1, algorithm);
            SCCmdParamByStr(l_cmd_id, 2, (const char *)public_key, sizeof(*public_key) * count_public_key);
            SCCmdParamByBool(l_cmd_id, 3, replace_existing_key);            

            l_result = SCCmdExecute(l_cmd_id, kTimeout);
         
            if (l_result == SC_rSuccess)
            {
                if (_ret_ != NULL)
                    *_ret_ = SCCmdRetAsInt(l_cmd_id, NULL);
                                
            }

            SCCmdEnd(l_cmd_id);
        }
        SCCmdFree(l_cmd_id);
   }

   return l_result;
}

