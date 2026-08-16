/**
 * @addtogroup rte_rfop_server
 * rte_rfop_server
 * @{
 */
/**
 * @file        rte_rfop_server.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/10/31
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/10/31   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _RTE_RFOP_SERVER_H_
#define _RTE_RFOP_SERVER_H_

#include "ofc/ofc_types.h"


// <2024-12-19> Jpz : 模组升级数据传输链路选择
#define RTE_RFOP_SERVER_DATALINK_XCU  0
#define RTE_RFOP_SERVER_DATALINK_COM  1
#define RTE_RFOP_SERVER_DATALINK_TYPE (RTE_RFOP_SERVER_DATALINK_COM)


// 公共临时缓存区。需要考虑是否存在使用冲突问题
#define RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE (1024)
#define RTE_RFOP_UPGRADE_COMMON_BUFFER_ADDR (g_rte_rfop_upgrade_common_buffer)
extern UInt8 g_rte_rfop_upgrade_common_buffer[RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE];



#ifdef __cplusplus
extern "C"
{
#endif

void RteRfopServerCreate(void);
void RteRfopServerDestroy(void);

void RteRfopServerInit(void);
void RteRfopServerDone(void);

void RteRfopServerRunOnce(void);



extern void RteRfopUpgradeSrtShowAlldata(void);

// 通过属性通知开始升级; 禁用其他可能会被reboot的逻辑
extern void RteRfopUpgradeSrtNotifyUpgradeStart(void);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _RTE_RFOP_SERVER_H_

/**
 * @}  Generated on "2024-10-31 16:40:56" by the tool "gen_hq_file.py >> V20231119" 
 */

