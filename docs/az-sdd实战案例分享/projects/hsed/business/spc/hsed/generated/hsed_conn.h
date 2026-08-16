/**
 * @file hsed_conn.h
 * @brief 
 * @note 
 * @author spc_builder
 * @date 2026-07-24 21:07:57
 * @version 1.0

 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _HSED_CONN_H_
#define _HSED_CONN_H_

#include <ofc_types.h>
#include <SpanCall.h>

/**
 * 创建连接
 * @param owner: 用户自定义的参数,该参数会出现在回调函数的参数中
 * @return 返回连接Id,创建失败返回NULL
 */
void *HcHsedConn_Create(void *owner);

/**
 * 释放连接
 * @param conn_id: 连接Id
 * @return 
 */
void HcHsedConn_Free(void *conn_id);

/**
 * 打开连接
 * @param conn_id: 连接Id
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsedConn_Open(void *conn_id);

/**
 * 关闭连接
 * @param conn_id: 连接Id
 * @return 
 */
 void HcHsedConn_Close(void *conn_id);

/**
 * 获取连接状态
 * @param conn_id: 连接Id
 * @return 返回连接状态,参见 SC_TConnState
 */
int HcHsedConn_State(void *conn_id);


#endif // _HSED_CONN_H_
