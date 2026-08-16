/**
 * @file         rte_terminal_ota.h
 * @brief        终端升级
 * @note
 * @author       lwq(linwenqiang@hopechart.com,林文强)
 * @date         2023/04/03
 * @version      1.0
 *
 * @par          修改日志
 * <table>
 * <tr><th>Date        <th>Version               <th>Author          <th>Description
 * <tr><td>2023/04/03  <td>1.0                   <td>linwenqiang     <td>创建初始版本
 * @copyright    杭州鸿泉物联网技术股份有限公司
 */
#ifndef _RTE_TERMINAL_OTA_H_
#define _RTE_TERMINAL_OTA_H_

#include "ofc/ofc_types.h"
#include "sfc/sfc_task.h"

/**
 * OTA状态
 */
enum TOtaStatusTag
{
    kOtaStatusNone = 0,
    kOtaStatusDownload = 1,             ///< 下载中
    kOtaStatusCheckAndTrans = 2,        ///< 检查文件和传输中
    kOtaStatusTransFinish = 3,          ///< 传输成功
    kOtaStatusWaitConfirm = 4,          ///< 等待确认
    kOtaStatusTransCannel = 5,          ///< 传输取消
    kOtaStatusMax,
};
typedef enum TOtaStatusTag TOtaStatus;

typedef void (*TRteTerminalOtaTaskNotify)(void *user_param, UInt8 notify_status, Int32 error_code);

typedef struct TRteTerminalOtaTaskTag TRteTerminalOtaTask;
struct TRteTerminalOtaTaskTag
{
    UInt64  task_sequence;          ///< 鸿泉云网任务流水号
    UInt8   *uri;                   ///< 下载的Url地址
    UInt8   md5[16];                ///< md5值
    UInt32  file_size;              ///< 文件大小
    UInt8   target;                 ///< 升级类别(目标) | 0：终端
    UInt8   hmi;                    ///< 升级方式 | 0:开机升级，1:立即升级 2:静默升级,@ref 参考TJtMsg808Ec2HttpUpgradeTypeEnum
    UInt8   uri_length;             ///< url的地址长度 
};

/**
 * ota 初始化
 * @param notify: 升级通知回调
 * @param user_param: 用户参数
 * @return >=0:成功 <0:失败
 */
Int32 RteTerminalOtaInit(TRteTerminalOtaTaskNotify notify, void *user_param);

/**
 * 创建新的ota任务
 * @param task: 任务参数
 * @return >=0:成功 <0:失败
 */
Int32 RteTerminalOtaCreateTask(const TRteTerminalOtaTask *task);

/**
 * 取消升级
 * @return >=0:成功 <0:失败
 */
Int32 RteTerminalOtaCannel(void);

/**
 * 升级去初始化
 * @return
 */
void RteTerminalOtaDone(void);

/**
 * 升级周期运行
 * @return
 */
void RteTerminalOtaRunOnce(void);
#endif
