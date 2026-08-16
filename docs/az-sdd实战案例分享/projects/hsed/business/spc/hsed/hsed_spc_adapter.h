/**
 * @file        hsed_spc_adapter.h
 * @brief       HSED SPC 到 BSC HSE 的业务适配接口
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#ifndef _HSED_SPC_ADAPTER_H_
#define _HSED_SPC_ADAPTER_H_

#include "bsc_hse/bsc_hse.h"

#ifdef __cplusplus
extern "C"
{
#endif

Bool HsedSpcAdapterInit(void);
void HsedSpcAdapterDone(void);

Bool HsedSpcAdapterAccept(void *connection);
void HsedSpcAdapterFinish(void *connection);

Int32 HsedSpcAdapterCreate(void *connection, TbscHseHandle *handle);
Int32 HsedSpcAdapterDestroy(void *connection, TbscHseHandle handle);

Int32 HsedSpcAdapterSetPropertyInt(void *connection,
                                   TbscHseHandle handle,
                                   TbscHsePropertyId property_id,
                                   Int32 value);
Int32 HsedSpcAdapterSetPropertyBytes(void *connection,
                                     TbscHseHandle handle,
                                     TbscHsePropertyId property_id,
                                     const UInt8 *value,
                                     UInt32 value_size);
Int32 HsedSpcAdapterGetPropertyInt(void *connection,
                                   TbscHseHandle handle,
                                   TbscHsePropertyId property_id,
                                   Int32 *value);
Int32 HsedSpcAdapterGetPropertyBytes(void *connection,
                                     TbscHseHandle handle,
                                     TbscHsePropertyId property_id,
                                     UInt8 *value,
                                     UInt32 *value_size);

Int32 HsedSpcAdapterEncrypt(void *connection,
                            TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            const UInt8 *input_data,
                            UInt32 input_size,
                            UInt8 *output_data,
                            UInt32 *output_size);
Int32 HsedSpcAdapterDecrypt(void *connection,
                            TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            const UInt8 *input_data,
                            UInt32 input_size,
                            UInt8 *output_data,
                            UInt32 *output_size);
Int32 HsedSpcAdapterSign(void *connection,
                         TbscHseHandle handle,
                         TbscHseAlgorithm algorithm,
                         const UInt8 *input_data,
                         UInt32 input_size,
                         UInt8 *signature,
                         UInt32 *signature_size,
                         UInt32 *r_size,
                         UInt32 *s_size);
Int32 HsedSpcAdapterExportPublicKey(void *connection,
                                    TbscHseHandle handle,
                                    TbscHseAlgorithm algorithm,
                                    UInt8 *public_key,
                                    UInt32 *public_key_size);
Int32 HsedSpcAdapterImportPublicKey(void *connection,
                                    TbscHseHandle handle,
                                    TbscHseAlgorithm algorithm,
                                    const UInt8 *public_key,
                                    UInt32 public_key_size,
                                    Bool replace_existing_key);

#ifdef __cplusplus
}
#endif

#endif  ///< _HSED_SPC_ADAPTER_H_
