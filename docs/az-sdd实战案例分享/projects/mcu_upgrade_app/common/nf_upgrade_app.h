/**
 * @addtogroup nf_upgrade_app
 * nf_upgrade_app
 * @{
 */
/**
 * @file        nf_upgrade_app.h
 * @brief       基于NFCreator.img包执行升级功能
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

#ifndef _NF_UPGRADE_APP_H_
#define _NF_UPGRADE_APP_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif



// 升级状态
enum TNfUpgradeStateTag
{
    kNfUpgradeStateInit = 0,   // 上电初始化
    kNfUpgradeStateIdle = 1,   // 空闲
    kNfUpgradeStatePreCheck = 2,   // 升级前预检查
    kNfUpgradeStateMpuUpgrade = 3,   // Mpu程序刷写
    kNfUpgradeStateMcuUpgrade = 4,   // Mcu程序刷写
    kNfUpgradeStateWaiteReboot = 5,   // 等待设备重启
    kNfUpgradeStateMpuConfirm = 6,   // Mpu程序确认
    kNfUpgradeStateMcuConfirm = 7,   // Mcu程序确认
    kNfUpgradeStateSuccess = 8,   // 成功
    kNfUpgradeStateFailed = 9,   // 失败
    kNfUpgradeStateMax
};

// 升级标记符
enum TNfUpgradeFlagTag
{
    kNfUpgradeFlagMpuReq =     (1 << 0),  // MPU请求升级
    kNfUpgradeFlagMpuUpgrade = (1 << 1),  // MPU执行刷写
    kNfUpgradeFlagMpuConfirm = (1 << 2),  // MPU确认成功

    kNfUpgradeFlagMcuReq =     (1 << 4),  // MPU请求升级
    kNfUpgradeFlagMcuUpgrade = (1 << 5),  // MCU执行刷写
    kNfUpgradeFlagMcuWaitConf = (1 << 6),  // MCU等待确认
};


// TErrCode 异常结果代码
enum TNfUpgradeErrCodeTag
{
    kNfUpgradeErrCodeSuccess = 1,
    kNfUpgradeErrCodeFailed = 0,  // 未知错误
    kNfUpgradeErrCodeMpuUpgrade = -1,  // MPU升级异常
    kNfUpgradeErrCodeMcuUpgrade = -2,  // MCU升级异常
    kNfUpgradeErrCodeMcuWaitConf = -4,  // MCU等待确认超时
    kNfUpgradeErrCodeMpuWaitConf = -5,  // MPU等待确认超时
    kNfUpgradeErrCodeImgInvalid = -6,  // 升级包无效
    kNfUpgradeErrCodeInitFailed = -7,  // 初始化失败
    kNfUpgradeErrCodeMcuState = -8,  // MCU状态异常
};

//--------------------------------------------------------------------------


typedef struct TNfUpgradeSubStateParamTag TNfUpgradeSubStateParam;
struct TNfUpgradeSubStateParamTag
{
    UInt8   reserve[4];  // 保留字段
};

typedef struct TNfUpgradeAppTag TNfUpgradeApp;
struct TNfUpgradeAppTag
{
    UInt8   req_id; // 升级请求ID
    UInt8   rsp_id; // 升级响应ID. 表示当前正在进行的任务id
    Int8    upgrade_precent;    // 升级进度百分比
    
    UInt32  m_state_enter_tick;  // 状态进入时间戳. 单位,ms
    Int32   m_errcode;
    UInt16  m_flag;
    UInt8   m_prev_state;  // 上一个状态
    UInt8   m_state;

    TNfUpgradeSubStateParam  m_sub_state_param;
};


// -----------------------------------------------


Int32 NfUpgradeAppCreate(void);
void  NfUpgradeAppDestroy(void);

Int32 NfUpgradeAppInit(void);
void  NfUpgradeAppDone(void);

Int32 NfUpgradeAppRunOnce(void);



// -----------------------------------------------


/**
 * 获取当前升级状态
 * @return 当前升级状态枚举值，对应 TNfUpgradeStateTag
 * @note 仅返回状态值，不附带进度、错误码等额外信息
 */
Int32 NfUpgradeState(void);

/**
 * 请求升级
 * @return > 0 升级任务ID
 *         <= 0  请求失败
 * @note:xxxxx
 */
Int32 NfUpgradeRequest(Int32 req_id);

/**
 * 获取升级状态
 * @param task_id    升级任务ID
 * @param *state     升级状态
 * @param *perc      升级进度百分比
 * @param *errcode   错误码
 * @return >= 0  成功, 结果有效
 *          < 0  失败
 * @note:xxxxx
 */
Int32 NfUpgradeResponse(Int32 *state, Int32 *perc, Int32 *errcode);



/**
 * 更新属性值
 * @return 
 * @note: NfUpgradeReqId 请求id             读写
 *        NfUpgradeRspId 响应id             只读. id不同时触发升级
 *        NfUpgradePrecent 升级进度百分比     只读
 *        NfUpgradeErrCode 错误码            只读
 *        NfUpgradeFlag 升级标记             只读
 *        NfUpgradeState 升级状态            只读
 */
Bool  NfUpgradeAppFlushStrData(void);


/**
 * 恢复上次掉电前的升级状态
 * @return 
 * @note:xxxxx
 */
Bool NfUpgradeAppRecover(void);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _NF_UPGRADE_APP_H_

/**
 * @}  Generated on "2024-12-23 17:03:30" by the tool "gen_hq_file.py >> V20231119" 
 */

