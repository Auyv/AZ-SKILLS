/**
 * @addtogroup rte_rfop_upgrade_srt_adp
 * rte_rfop_upgrade_srt_adp
 * @{
 */
/**
 * @file        rte_rfop_upgrade_srt_adp.h
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

#ifndef _RTE_RFOP_UPGRADE_SRT_ADP_H_
#define _RTE_RFOP_UPGRADE_SRT_ADP_H_


#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

Int32 RteRfopUpgradeSrtAdpGetReqImgFilename(UInt8 *val, UInt32 size);
Int32 RteRfopUpgradeSrtAdpSetReqImgFilename(UInt8 *val, UInt32 size);


Int32 RteRfopUpgradeSrtAdpGetReqStartTaskId(UInt8 *val);
Int32 RteRfopUpgradeSrtAdpSetReqStartTaskId(UInt8 val);


Int32 RteRfopUpgradeSrtAdpGetReqCancelTaskId(UInt8 *val);
Int32 RteRfopUpgradeSrtAdpSetReqCancelTaskId(UInt8 val);


Int32 RteRfopUpgradeSrtAdpGetCurTaskId(UInt8 *val);
Int32 RteRfopUpgradeSrtAdpSetCurTaskId(UInt8 val);

Int32 RteRfopUpgradeSrtAdpGetCurTaskState(UInt8 *val);
Int32 RteRfopUpgradeSrtAdpSetCurTaskState(UInt8 val);

Int32 RteRfopUpgradeSrtAdpGetCurTaskPercent(Int8 *val);
Int32 RteRfopUpgradeSrtAdpSetCurTaskPercent(Int8 val);

Int32 RteRfopUpgradeSrtAdpGetCurTaskResult(Int32 *val);
Int32 RteRfopUpgradeSrtAdpSetCurTaskResult(Int32 val);

Int32 RteRfopUpgradeSrtAdpGetHardwareVersion(UInt32 *val);


void RteRfopUpgradeSrtShowAlldata(void);

// 通过属性通知开始升级; 禁用其他可能会被reboot的逻辑
void RteRfopUpgradeSrtNotifyUpgradeStart(void);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _RTE_RFOP_UPGRADE_SRT_ADP_H_

/**
 * @}  Generated on "2024-07-23 11:11:59" by the tool "gen_hq_file.py >> V20231119"
 */

