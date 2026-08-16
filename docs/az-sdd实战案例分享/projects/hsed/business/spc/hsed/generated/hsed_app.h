/**
 * @file hsed_app.h
 * @brief 
 * @note 
 * @author spc_builder
 * @date 2026-07-24 21:07:57
 * @version 1.0

 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _HSED_APP_H_
#define _HSED_APP_H_

#include <ofc_types.h>

typedef enum THcHsedAppStateTag
{
    kHsedAppState_Invalid = -1,  ///< 无效状态
    kHsedAppState_Inactive = 0,  ///< 未激活状态
    kHsedAppState_Closing = 1,  ///< 关闭中
    kHsedAppState_Opening = 2,  ///< 打开中
    kHsedAppState_Opened = 3,  ///< 已经打开
} THcHsedAppState;

/**
 * 应用初始化操作
 * @return 
 */
void HcHsedApp_Init(void);

/**
 * 应用去初始化操作
 * @param forced: 强制退出
 * @return 
 */
void HcHsedApp_Fini(Bool forced);

/**
 * 打开应用
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsedApp_Open(void);

/**
 * 关闭应用
 * @return 
 */
void HcHsedApp_Close(void);

/**
 * 获取应用当前状态
 * @return 应用的状态,参见: THcHsedAppState
 */
int HcHsedApp_State(void);

/**
 * 获取应用Id
 * @return 
 */
int HcHsedApp_AppId(void);

#endif // _HSED_APP_H_
