/**
 * @file        hsed_spc_service.h
 * @brief       HSED SPC 服务生命周期接口
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#ifndef _HSED_SPC_SERVICE_H_
#define _HSED_SPC_SERVICE_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

Int32 HsedSpcServiceStart(void);
void HsedSpcServiceStop(void);

#ifdef __cplusplus
}
#endif

#endif  ///< _HSED_SPC_SERVICE_H_
