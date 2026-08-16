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
#ifndef _RTE_TERMINAL_OTA_BG_H_
#define _RTE_TERMINAL_OTA_BG_H_

#include "ofc/ofc_types.h"

/**
 * ota 后台初始化
 * @return
 */
void RteTerminalOtaBgInit(void);

/**
 * ota 后台周期运行
 * @return
 */
void RteTerminalOtaBgRunOnce(void);

/**
 * ota 后台去初始化
 * @return
 */
void RteTerminalOtaBgDone(void);

#endif
