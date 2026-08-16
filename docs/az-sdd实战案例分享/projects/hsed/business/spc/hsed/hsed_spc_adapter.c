/**
 * @file        hsed_spc_adapter.c
 * @brief       HSED SPC 连接隔离和 BSC HSE 调用适配
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "hsed_spc_adapter.h"

#include "ofc/ofc_object.h"
#include "sfc/sfc_mutex.h"

#define ALOG_TAG "HSED_SPC"
#include "alog/alog.h"

#include <string.h>

typedef struct THsedSpcHandleEntryTag THsedSpcHandleEntry;
struct THsedSpcHandleEntryTag
{
    Bool          used;
    void         *connection;
    TbscHseHandle handle;
};

typedef struct THsedSpcAdapterRuntimeTag THsedSpcAdapterRuntime;
struct THsedSpcAdapterRuntimeTag
{
    Bool                    initialized;
    TSfCriticalSection      lock;
    THsedSpcHandleEntry     entries[BSC_HSE_OBJECT_POOL_SIZE];
};

static THsedSpcAdapterRuntime g_spc_adapter_ = {0};

static Int32 L_FindEntry(void *connection, TbscHseHandle handle)
{
    Int32 result = -1;
    UInt32 index;

    for (index = 0; index < BSC_HSE_OBJECT_POOL_SIZE; index++)
    {
        if ((g_spc_adapter_.entries[index].used == True) &&
            (g_spc_adapter_.entries[index].connection == connection) &&
            (g_spc_adapter_.entries[index].handle == handle))
        {
            result = (Int32)index;
            break;
        }
    }
    return result;
}

static Int32 L_FindFreeEntry(void)
{
    Int32 result = -1;
    UInt32 index;

    for (index = 0; index < BSC_HSE_OBJECT_POOL_SIZE; index++)
    {
        if (g_spc_adapter_.entries[index].used == False)
        {
            result = (Int32)index;
            break;
        }
    }
    return result;
}

static void L_ClearEntry(UInt32 index)
{
    memset(&g_spc_adapter_.entries[index], 0,
           sizeof(g_spc_adapter_.entries[index]));
}

Bool HsedSpcAdapterInit(void)
{
    Bool result = False;

    if (g_spc_adapter_.initialized == True)
    {
        result = True;
    }
    else if (TSfCriticalSectionCreate(&g_spc_adapter_.lock) == True)
    {
        memset(g_spc_adapter_.entries, 0, sizeof(g_spc_adapter_.entries));
        g_spc_adapter_.initialized = True;
        result = True;
    }
    return result;
}

void HsedSpcAdapterDone(void)
{
    UInt32 index;

    if (g_spc_adapter_.initialized == True)
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        for (index = 0; index < BSC_HSE_OBJECT_POOL_SIZE; index++)
        {
            if (g_spc_adapter_.entries[index].used == True)
            {
                Int32 destroy_result =
                    BscHseDestroy(g_spc_adapter_.entries[index].handle);
                if (destroy_result != kBscHseResultOk)
                {
                    ALOG_E("SPC cleanup handle failed: handle=%llu result=%d",
                           (unsigned long long)g_spc_adapter_.entries[index].handle,
                           destroy_result);
                }
                L_ClearEntry(index);
            }
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
        g_spc_adapter_.initialized = False;
        OfDestroyObject((TOfObject *)&g_spc_adapter_.lock);
    }
}

Bool HsedSpcAdapterAccept(void *connection)
{
    Bool result = False;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        result = True;
        ALOG_I("SPC connection accepted: %p", connection);
    }
    return result;
}

void HsedSpcAdapterFinish(void *connection)
{
    UInt32 index;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        for (index = 0; index < BSC_HSE_OBJECT_POOL_SIZE; index++)
        {
            if ((g_spc_adapter_.entries[index].used == True) &&
                (g_spc_adapter_.entries[index].connection == connection))
            {
                Int32 destroy_result =
                    BscHseDestroy(g_spc_adapter_.entries[index].handle);
                if (destroy_result != kBscHseResultOk)
                {
                    ALOG_E("SPC disconnect cleanup failed: handle=%llu result=%d",
                           (unsigned long long)g_spc_adapter_.entries[index].handle,
                           destroy_result);
                }
                L_ClearEntry(index);
            }
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
        ALOG_I("SPC connection finished: %p", connection);
    }
}

Int32 HsedSpcAdapterCreate(void *connection, TbscHseHandle *handle)
{
    Int32 result = kBscHseResultInvalidParam;

    if ((g_spc_adapter_.initialized == True) &&
        (connection != NULL) && (handle != NULL))
    {
        Int32 entry_index;

        *handle = 0;
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        entry_index = L_FindFreeEntry();
        if (entry_index < 0)
        {
            result = kBscHseResultNoResource;
        }
        else
        {
            result = BscHseCreate(handle);
            if (result == kBscHseResultOk)
            {
                g_spc_adapter_.entries[entry_index].used = True;
                g_spc_adapter_.entries[entry_index].connection = connection;
                g_spc_adapter_.entries[entry_index].handle = *handle;
            }
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterDestroy(void *connection, TbscHseHandle handle)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        Int32 entry_index;

        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        entry_index = L_FindEntry(connection, handle);
        if (entry_index >= 0)
        {
            result = BscHseDestroy(handle);
            if (result == kBscHseResultOk)
            {
                L_ClearEntry((UInt32)entry_index);
            }
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterSetPropertyInt(void *connection,
                                   TbscHseHandle handle,
                                   TbscHsePropertyId property_id,
                                   Int32 value)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseSetPropertyInt(handle, property_id, value);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterSetPropertyBytes(void *connection,
                                     TbscHseHandle handle,
                                     TbscHsePropertyId property_id,
                                     const UInt8 *value,
                                     UInt32 value_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseSetPropertyBytes(handle, property_id,
                                             value, value_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterGetPropertyInt(void *connection,
                                   TbscHseHandle handle,
                                   TbscHsePropertyId property_id,
                                   Int32 *value)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseGetPropertyInt(handle, property_id, value);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterGetPropertyBytes(void *connection,
                                     TbscHseHandle handle,
                                     TbscHsePropertyId property_id,
                                     UInt8 *value,
                                     UInt32 *value_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseGetPropertyBytes(handle, property_id,
                                             value, value_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterEncrypt(void *connection,
                            TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            const UInt8 *input_data,
                            UInt32 input_size,
                            UInt8 *output_data,
                            UInt32 *output_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseEncrypt(handle, algorithm, input_data, input_size,
                                   output_data, output_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterDecrypt(void *connection,
                            TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            const UInt8 *input_data,
                            UInt32 input_size,
                            UInt8 *output_data,
                            UInt32 *output_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseDecrypt(handle, algorithm, input_data, input_size,
                                   output_data, output_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterSign(void *connection,
                         TbscHseHandle handle,
                         TbscHseAlgorithm algorithm,
                         const UInt8 *input_data,
                         UInt32 input_size,
                         UInt8 *signature,
                         UInt32 *signature_size,
                         UInt32 *r_size,
                         UInt32 *s_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseSign(handle, algorithm, input_data, input_size,
                                signature, signature_size, r_size, s_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterExportPublicKey(void *connection,
                                    TbscHseHandle handle,
                                    TbscHseAlgorithm algorithm,
                                    UInt8 *public_key,
                                    UInt32 *public_key_size)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseExportPublicKey(handle, algorithm,
                                           public_key, public_key_size);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}

Int32 HsedSpcAdapterImportPublicKey(void *connection,
                                    TbscHseHandle handle,
                                    TbscHseAlgorithm algorithm,
                                    const UInt8 *public_key,
                                    UInt32 public_key_size,
                                    Bool replace_existing_key)
{
    Int32 result = kBscHseResultObjectNotFound;

    if ((g_spc_adapter_.initialized == True) && (connection != NULL))
    {
        TSfCriticalSectionEnter(&g_spc_adapter_.lock);
        if (L_FindEntry(connection, handle) >= 0)
        {
            result = BscHseImportPublicKey(handle, algorithm,
                                           public_key, public_key_size,
                                           replace_existing_key);
        }
        TSfCriticalSectionLeave(&g_spc_adapter_.lock);
    }
    return result;
}
