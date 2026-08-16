/**
 * @addtogroup config
 * config
 * @{
 */
/**
 * @file        config.h
 * @brief       HSED INI 配置管理
 * @note        统一管理 HSED 运行时配置值
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 *
 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th>Description
 * <tr><td>2026/07/24   <td>1.0         <td>HSED        <td>创建初始版本
 * </table>
 */

#ifndef _HSED_CONFIG_H_
#define _HSED_CONFIG_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HSED_CONFIG_INI_PATH                 "/etc/hsed.ini"
#define HSED_CONFIG_VENDOR_SO_PATH_DEFAULT   ""
#define HSED_CONFIG_VENDOR_SO_PATH_MAX_LEN   256
#define HSED_CONFIG_SPI_DEVICE_PATH_DEFAULT  "/dev/spidev1.0"
#define HSED_CONFIG_SPI_DEVICE_PATH_MAX_LEN  256
#define HSED_CONFIG_DEBUG_DEFAULT            0

typedef struct THsedConfigTag THsedConfig;
struct THsedConfigTag
{
    char vendor_so_path[HSED_CONFIG_VENDOR_SO_PATH_MAX_LEN];
    char spi_device_path[HSED_CONFIG_SPI_DEVICE_PATH_MAX_LEN];
    Bool debug;
};

extern THsedConfig g_hsed_config;

void HsedConfigSetDefaults(void);
Bool HsedConfigInit(const char *config_path);
Bool HsedConfigLoadSpiDevicePath(const char *config_path);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _HSED_CONFIG_H_

/** @} */
