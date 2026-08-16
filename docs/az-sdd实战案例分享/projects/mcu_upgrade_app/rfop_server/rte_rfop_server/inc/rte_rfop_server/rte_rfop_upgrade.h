/**
 * @addtogroup rte_local_upgrade
 * rte_local_upgrade
 * @{
 */
/**
 * @file        rte_local_upgrade.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/07/09
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/07/09   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _Rte_LOCAL_UPGRADE_H_
#define _Rte_LOCAL_UPGRADE_H_

#include "ofc/ofc_types.h"
#include "rfop/rfop_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ----------------------- img包基础信息校验 ----------------------------
#define RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_ADDR (0x446000)   // 升级文件起始地址 
#define RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_SIZE (0x150000)   // 升级文件大小
#define RTE_RFOP_UPGRADE_PROJECT_NAME "HQT401-K00"


// ---------------------- 升级步骤 ----------------------------
enum TRteRfopUpgradeStateTag                                        
{
    kRteRfopUpgradeStateInvlalid = 0, // 无效状态，表示初始化失败
    kRteRfopUpgradeStateIdle = 1,    // 空闲状态
    kRteRfopUpgradeStatePreCheck = 2,  // 请求下载，等待升级前条件检测   
    kRteRfopUpgradeStateDownload = 3,  // 下载升级包过程               
    kRteRfopUpgradeStateFileCheck = 4,  // 升级包完整性检测            
    kRteRfopUpgradeStateUpgrade = 5,  // 执行刷写                      
    kRteRfopUpgradeStateWaitReboot = 6,  // 等待重启
    kRteRfopUpgradeStateWaitConfirm = 7,  // 等待结果确认              
    kRteRfopUpgradeStateDone = 8,  // 升级结束                         
    kRteRfopUpgradeStateFailed = 9,  // 异常状态   
};
typedef enum TRteRfopUpgradeStateTag TRteRfopUpgradeState;

// ----------------------------------------------------------------

/**
 * 创建升级模块
 * @return 
 * @note:xxxxx
 */
Int32 RteRfopUpgradeCreate(void);

/**
 * 初始化
 * @return >= 0 表示成功，< 0 表示失败
 * @note:依赖 srt 服务
 */
Int32 RteRfopUpgradeInit(void);

/**
 * 循环执行
 * @return 
 * @note:
 */
void RteRfopUpgradeRunOnce(void);

/**
 * 主动清理资源
 * @return 
 * @note:xxxxx
 */
void RteRfopUpgradeClear(void);

/**
 * 反初始化
 * @return 
 * @note:xxxxx
 */
void RteRfopUpgradeDone(void);


/**
 * 析构升级模块
 * @return 
 * @note:xxxxx
 */
void RteRfopUpgradeDestroy(void);

// ---------------------------------------------------------------------------


/**
 * 获取后台操作接口集实现
 * @return 
 * @note:xxxxx
 */
const TRfopSrvBgo *RteRfopUpgradeBgoImpl(void);

/*---------------------------------------------------------*/

typedef Int32 (*RteRfopUpgradeDoSendDataMethod)(void *user_param, const Int8 *data, Int32 len);

/**
 * 设置数据传输发送方法
 * @param method                 发送数据方法
 * @param *user_param            用户参数
 * @return >= 0 表示成功；< 0 表示失败
 * @note:xxxxx
 */
Int32 RteRfopUpgradeSetSendDataMethod(RteRfopUpgradeDoSendDataMethod method, void *user_param);

/**
 * 交互数据发送方法
 * @param *data                  数据
 * @param len                    数据长度
 * @return >= 0 表示成功；< 0 表示失败
 * @note:xxxxx
 */
Int32 RteRfopUpgradeDoSendData(const Int8 *data, Int32 len);

// ---------------------------------------------------------------------------

/**
 * 开始升级
 * @param upgrade_filename      升级文件名. 长度不超过64Byte，包含结尾‘\0’
 * @param task_id               指定任务id。 0，自动分配任务id，否则使用指定id
 * @return > 0 表示请求成功的任务id；< 0 表示失败
 * @note:升级动作异步进行，通过回调通知状态
 */
Int32 RteRfopUpgradeStartTask(const char *upgrade_filename, UInt8 task_id);

/**
 * 主动取消升级
 * @param task_id
 * @return 
 * @note:>= 0 表示成功
 *       <  0 表示失败
 */
Int32 RteRfopUpgradeCancelTask(Int32 task_id);

// -------------------------------------------------------------------------

/**
 * 当前任务id
 * @return 0 无效值，1~255 有效值。
 * @note:每次Start 成功后，该值会增加1
 */
Int32 RteRfopUpgradeCurTaskId(void);

/**
 * 升级状态
 * @return >
 * @note:xxxxx
 */
Int32 RteRfopUpgradeCurTaskState(void);


/**
 * 当前下载进度
 * @return 
 * @note:xxxxx
 */
Int32 RteRfopUpgradeCurTaskPercent(void);


/**
 * 当前任务结果
 * @return 
 * @note:xxxxx
 */
Int32 RteRfopUpgradeCurTaskResult(void);




#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _Rte_LOCAL_UPGRADE_H_

/**
 * @}  Generated on "2024-07-09 15:14:42" by the tool "gen_Rte_file.py >> V20231119" 
 */


// #if (0)
// /********************************** srt属性定义 *******************************************/
// mpu->mcu
// 1. req_filename[128] 升级文件名+路径        mcu端保存   path/filename.img   filename格式：TXJ-BR4-HQ23_V1.0.0.119_MD5.img
// 2. req_start_task_id 请求任务开始序列号     不保存      0 无效值; [1~255] 有效;
// 3. req_cancle_task_id 请求任务取消序列号    不保存      0 无效值; [1~255] 有效;

// mcu->mpu
// 1. cur_task_id 当前任务序列号       mcu端保存 UInt8               0 无效值; [1~255] 有效;  
// 2. cur_task_state 当前任务状态      mcu端保存 UInt8
// 3. cur_task_percent 当前任务进度    mcu端保存 Int8               -1 无效值; [0~100] 有效;
// 4. cur_task_result 当前任务结果     mcu端保存 Int32
// // 6. cur_task_filename[128] 当前任务文件名  保存

// // 请求开始升级

// #endif
