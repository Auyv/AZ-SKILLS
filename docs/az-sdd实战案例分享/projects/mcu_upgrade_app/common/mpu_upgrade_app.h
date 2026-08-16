/**
 * @addtogroup mpu_upgrade_app
 * mpu_upgrade_app
 * @{
 */
/**
 * @file        mpu_upgrade_app.h
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

#ifndef _MPU_UPGRADE_APP_H_
#define _MPU_UPGRADE_APP_H_

#include "ofc/ofc_types.h"
#include "mpu_upgrade.h"

#ifdef __cplusplus
extern "C"
{
#endif


Int32 MpuUpgradeAppCreate(void);
void  MpuUpgradeAppDestroy(void);

Int32 MpuUpgradeAppInit(void);
void  MpuUpgradeAppDone(void);

Int32 MpuUpgradeAppRunOnce(void);


/**
 * 请求执行mpu自升级
 * @return >= 0:成功. 返回当前request_id
 *         <  0: 操作失败
 * @note:xxxxx
 */
Int32 MpuUpgradeRequest(void);

/**
 * 获取升级状态
 * @param task_id    任务ID
 * @param *state     升级状态   @ TMpuUpgradeState
 * @param *perc      升级进度
 * @param *errcode   错误码 @ TMpuUpgradeErr
 * @return >=  0:成功
 *         <   0:失败
 * @note:xxxxx
 */
Int32 MpuUpgradeResponse(Int32 *state, Int32 *perc, Int32 *errcode);

/**
 * 取消升级
 * @return 
 * @note:xxxxx
 */
void MpuUpgradeCancel(void);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _MPU_UPGRADE_APP_H_

/**
 * @}  Generated on "2024-12-23 17:00:29" by the tool "gen_hq_file.py >> V20231119" 
 */

