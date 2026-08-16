/**
 * @file        rte_hse.h
 * @brief       HSED 项目运行时硬件密码适配接口
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#ifndef _RTE_HSE_H_
#define _RTE_HSE_H_

#include "bsc_hse/bsc_hse_vendor.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct TRteHseConfigTag TRteHseConfig;
struct TRteHseConfigTag
{
    const char *vendor_so_path;
    const char *spi_device_path;
};

/**
 * @brief 加载供应商动态库并向 BSC HSE 注入虚接口
 * @param config 项目运行时配置
 * @return 统一 BSC HSE 结果
 */
Int32 RteHseInit(const TRteHseConfig *config);

const TbscHseVendorBinding *RteHseVendorBinding(void);

/**
 * @brief 检查项目固定非对称密钥槽，并按调试策略修复异常槽位
 * @param allow_repair True 时允许强制覆盖并重新生成密钥对
 * @return 统一 BSC HSE 结果
 */
Int32 RteHseCheckAndRepairKeyPairs(Bool allow_repair);

/**
 * @brief 调用供应商动态库自检入口
 * @param config 项目运行时配置
 * @return 统一 BSC HSE 结果
 */
Int32 RteHseSelfTest(const TRteHseConfig *config);

/**
 * @brief 解除 BSC 注入并卸载供应商动态库
 */
void RteHseDone(void);

#ifdef __cplusplus
}
#endif

#endif  ///< _RTE_HSE_H_
