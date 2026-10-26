/**
 * @file config_header.h
 * @brief 配置向导示例；复制后按目标模块调整名称、默认值和真实约束。
 * @note Encoding: GBK, no BOM. 修改配置后需重新编译。
 * Keil：打开本文件，切换 Configuration Wizard 页签。
 */
#ifndef _APP_CONFIG_HEADER_H_
#define _APP_CONFIG_HEADER_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h> 模块配置
// <h> 功能与角色
// <q> 启用模块
// <i> 关闭后模块不处理业务数据；具体停用行为由实现决定。
#define APP_CONFIG_ENABLE (1)

// <o> 处理器角色
// <0=> MCU：连接本地外设
// <1=> MPU：使用远端外设
// <i> 角色与报文帧类型无关。默认 MCU。
#define APP_CONFIG_PROCESSOR_ROLE (0)
// </h>

// <h> 允许的通道（按代码编号）
// <i> 全部取消表示不接收通道数据；此处不会启用物理外设。
// <q.0> 通道 0
// <q.1> 通道 1
// <q.2> 通道 2
#define APP_CONFIG_CHANNEL_MASK (0x01)
// </h>

// <h> 缓存与调度
// <o> 数据缓存容量（字节） <64-1024>
// <i> 至少容纳一条最大记录；越大越占 RAM，协议上限为 1024 字节。
#define APP_CONFIG_BUFFER_SIZE (256)

// <o> 处理周期（毫秒） <1-60000>
// <i> 减小周期可降低调度等待，同时增加任务运行次数。
#define APP_CONFIG_PERIOD_MS (1000)
// </h>
// </h>

// <<< end of configuration section >>>

/* 内部常量与派生值不作为向导选项。以下协议数字仅供示例。 */
#define APP_CONFIG_PROCESSOR_ROLE_MCU (0)
#define APP_CONFIG_PROCESSOR_ROLE_MPU (1)
#define APP_CONFIG_RECORD_SIZE (64)
#define APP_CONFIG_PROTOCOL_LIMIT (1024)
#define APP_CONFIG_RECORD_COUNT (APP_CONFIG_BUFFER_SIZE / APP_CONFIG_RECORD_SIZE)

/* 约束必须与模块实现一致，不能仅依靠向导输入范围。 */
#if APP_CONFIG_ENABLE != 0 && APP_CONFIG_ENABLE != 1
#error "APP_CONFIG_ENABLE must be 0 or 1"
#endif
#if APP_CONFIG_PROCESSOR_ROLE != APP_CONFIG_PROCESSOR_ROLE_MCU \
    && APP_CONFIG_PROCESSOR_ROLE != APP_CONFIG_PROCESSOR_ROLE_MPU
#error "APP_CONFIG_PROCESSOR_ROLE must select MCU or MPU"
#endif
#if APP_CONFIG_CHANNEL_MASK < 0 || APP_CONFIG_CHANNEL_MASK > 0x07
#error "APP_CONFIG_CHANNEL_MASK uses bits 0..2 only"
#endif
#if APP_CONFIG_BUFFER_SIZE < APP_CONFIG_RECORD_SIZE \
    || APP_CONFIG_BUFFER_SIZE > APP_CONFIG_PROTOCOL_LIMIT
#error "APP_CONFIG_BUFFER_SIZE must be 64..1024 bytes"
#endif
#if APP_CONFIG_PERIOD_MS < 1 || APP_CONFIG_PERIOD_MS > 60000
#error "APP_CONFIG_PERIOD_MS must be 1..60000 milliseconds"
#endif

#endif
