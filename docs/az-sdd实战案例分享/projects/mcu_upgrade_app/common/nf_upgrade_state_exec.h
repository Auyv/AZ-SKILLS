/**
 * @addtogroup nf_upgrade_state_exec
 * nf_upgrade_state_exec
 * @{
 */
/**
 * @file        nf_upgrade_state_exec.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/28
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/28   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _NF_UPGRADE_STATE_EXEC_H_
#define _NF_UPGRADE_STATE_EXEC_H_

#include "nf_upgrade_app.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct TNfUpgradeStateExecTag TNfUpgradeStateExec;
// 状态进入函数
typedef void  (*TNfUpgradeStateEnterFunc)(TNfUpgradeApp *app);
// 状态执行函数, 返回下次执行需要等待的毫秒数
typedef Int32 (*TNfUpgradeStateRunOnceFunc)(TNfUpgradeApp *app);
// 状态切换函数, 返回需要切换的状态
typedef UInt8 (*TNfUpgradeStateGotoFunc)(TNfUpgradeApp *app);
struct TNfUpgradeStateExecTag
{
    char *name;
    TNfUpgradeStateEnterFunc    DoEnter;
    TNfUpgradeStateRunOnceFunc  DoRunOnce;
    TNfUpgradeStateGotoFunc     DoGoto;
};


extern const TNfUpgradeStateExec g_nfupgrade_state_exec[kNfUpgradeStateMax];



#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _NF_UPGRADE_STATE_EXEC_H_

/**
 * @}  Generated on "2024-12-28 12:18:14" by the tool "gen_hq_file.py >> V20231119" 
 */

