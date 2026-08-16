/**
 * @file        config.c
 * @brief       HSED INI 配置管理实现
 * @note        使用 TCIni 统一加载配置值
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

#include "config.h"
#include "cini/cini.h"

#include <string.h>

THsedConfig g_hsed_config = {0};

void HsedConfigSetDefaults(void)
{
    g_hsed_config.vendor_so_path[0] = '\0';
    g_hsed_config.debug = False;
    strncpy(g_hsed_config.spi_device_path,
            HSED_CONFIG_SPI_DEVICE_PATH_DEFAULT,
            sizeof(g_hsed_config.spi_device_path) - 1U);
    g_hsed_config.spi_device_path[
        sizeof(g_hsed_config.spi_device_path) - 1U] = '\0';
}

Bool HsedConfigInit(const char *config_path)
{
    Bool result = False;
    const char *actual_path = config_path;
    TCIni ini = {0};
    Int32 debug = HSED_CONFIG_DEBUG_DEFAULT;

    HsedConfigSetDefaults();
    if (actual_path == NULL)
    {
        actual_path = HSED_CONFIG_INI_PATH;
    }

    if (TCIniCreate(&ini, actual_path) >= 0)
    {
        if ((TCIniGetKeyValueStringEx(
                 &ini,
                 "bsc_hse",
                 "vendor_so_path",
                 g_hsed_config.vendor_so_path,
                 sizeof(g_hsed_config.vendor_so_path),
                 HSED_CONFIG_VENDOR_SO_PATH_DEFAULT) == True) &&
            (g_hsed_config.vendor_so_path[0] != '\0'))
        {
            if (TCIniGetKeyValueIntEx(&ini, "bsc_hse", "debug", &debug,
                                      HSED_CONFIG_DEBUG_DEFAULT) == True)
            {
                g_hsed_config.debug = (debug == 1) ? True : False;
            }
            result = True;
        }
        TCIniDestroy(&ini);
    }

    return result;
}

Bool HsedConfigLoadSpiDevicePath(const char *config_path)
{
    Bool result = False;
    TCIni ini = {0};

    strncpy(g_hsed_config.spi_device_path,
            HSED_CONFIG_SPI_DEVICE_PATH_DEFAULT,
            sizeof(g_hsed_config.spi_device_path) - 1U);
    g_hsed_config.spi_device_path[
        sizeof(g_hsed_config.spi_device_path) - 1U] = '\0';

    if (config_path == NULL)
    {
        result = True;
    }
    else if (TCIniCreate(&ini, config_path) >= 0)
    {
        result = TCIniGetKeyValueStringEx(
            &ini,
            "spi",
            "device_path",
            g_hsed_config.spi_device_path,
            sizeof(g_hsed_config.spi_device_path),
            "");
        if (g_hsed_config.spi_device_path[0] == '\0')
        {
            result = False;
        }
        TCIniDestroy(&ini);
    }
    return result;
}
