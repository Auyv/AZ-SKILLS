/**
 * @file        upgrade_app_cfg.c
 * @brief       升级应用配置管理实现 - 从 INI 文件加载配置
 * @note        当 INI 文件不存在时，使用编译时默认值
 * @author      AI
 * @date        2026/05/08
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2026/05/08   <td>1.0         <td>AI           <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#include <string.h>
#include "upgrade_app_cfg.h"
#include "cini.h"

// 全局配置实例
TUpgradeCfg g_upgrade_cfg;

/**
 * @brief 初始化配置，从 INI 文件加载
 * @param ini_file_path INI 文件路径，NULL 使用默认路径 UPGRADE_CFG_INI_PATH
 * @return 0 成功加载, -1 文件不存在或读取失败(使用默认值)
 */
Int32 UpgradeCfgInit(const char *ini_file_path)
{
    TCIni ini;
    const char *cfg_path = ini_file_path == NULL ? UPGRADE_CFG_INI_PATH : ini_file_path;
    char str_buf[256];
    Int32 int_val;

    // 1. 使用默认值初始化
    strncpy(g_upgrade_cfg.log_file_dir, DEFAULT_LOG_FILE_DIR, sizeof(g_upgrade_cfg.log_file_dir) - 1);
    strncpy(g_upgrade_cfg.log_file_tag, DEFAULT_LOG_FILE_TAG, sizeof(g_upgrade_cfg.log_file_tag) - 1);
    g_upgrade_cfg.log_file_size = DEFAULT_LOG_FILE_SIZE;
    g_upgrade_cfg.log_file_count = DEFAULT_LOG_FILE_COUNT;
    strncpy(g_upgrade_cfg.nf_upgrade_img_path, DEFAULT_NFCREATE_UPGRADE_IMG_PATH, sizeof(g_upgrade_cfg.nf_upgrade_img_path) - 1);
    strncpy(g_upgrade_cfg.nf_upgrade_se_img_path, DEFAULT_NFCREATE_UPGRADE_SE_IMG_PATH, sizeof(g_upgrade_cfg.nf_upgrade_se_img_path) - 1);
    strncpy(g_upgrade_cfg.mpu_upgrade_file_name, DEFAULT_MPU_UPGRADE_FILE_NAME, sizeof(g_upgrade_cfg.mpu_upgrade_file_name) - 1);
    strncpy(g_upgrade_cfg.mpu_partition_record_file, DEFAULT_MPU_UPGRADE_PARTITION_RECORD_FILE, sizeof(g_upgrade_cfg.mpu_partition_record_file) - 1);
    g_upgrade_cfg.net_upgrade_enable = DEFAULT_NET_UPGRADE_ENABLE;
    strncpy(g_upgrade_cfg.net_package_url_format, DEFAULT_NET_PACKAGE_URL_FORMAT, sizeof(g_upgrade_cfg.net_package_url_format) - 1);
    strncpy(g_upgrade_cfg.net_package_pem_keys_dir, DEFAULT_NET_PACKAGE_PEM_KEYS_DIR, sizeof(g_upgrade_cfg.net_package_pem_keys_dir) - 1);
    g_upgrade_cfg.net_package_srt_id_addr = DEFAULT_NET_PACKAGE_SRT_ID_ADDR;
    g_upgrade_cfg.net_package_srt_id_port = DEFAULT_NET_PACKAGE_SRT_ID_PORT;
    g_upgrade_cfg.net_package_srt_id_file = DEFAULT_NET_PACKAGE_SRT_ID_FILE;
    g_upgrade_cfg.mcu_upgrade_enable = (Bool)DEFAULT_MCU_UPGRADE_ENABLE;
    g_upgrade_cfg.mpu_upgrade_enable = (Bool)DEFAULT_MPU_UPGRADE_ENABLE;

    // 2. 尝试打开 INI 文件
    if (TCIniCreate(&ini, cfg_path) != 0) {
        return -1; // 文件不存在，使用默认值
    }

    // 3. 逐项读取
    // [log]
    if (TCIniGetKeyValueStringEx(&ini, "log", "file_dir",
        str_buf, sizeof(str_buf), g_upgrade_cfg.log_file_dir)) {
        strncpy(g_upgrade_cfg.log_file_dir, str_buf, sizeof(g_upgrade_cfg.log_file_dir) - 1);
    }
    if (TCIniGetKeyValueStringEx(&ini, "log", "file_tag",
        str_buf, sizeof(str_buf), g_upgrade_cfg.log_file_tag)) {
        strncpy(g_upgrade_cfg.log_file_tag, str_buf, sizeof(g_upgrade_cfg.log_file_tag) - 1);
    }
    if (TCIniGetKeyValueIntEx(&ini, "log", "file_size",
        &int_val, g_upgrade_cfg.log_file_size)) {
        g_upgrade_cfg.log_file_size = int_val;
    }
    if (TCIniGetKeyValueIntEx(&ini, "log", "file_count",
        &int_val, g_upgrade_cfg.log_file_count)) {
        g_upgrade_cfg.log_file_count = int_val;
    }

    // [nf_upgrade]
    if (TCIniGetKeyValueStringEx(&ini, "nf_upgrade", "img_file",
        str_buf, sizeof(str_buf), g_upgrade_cfg.nf_upgrade_img_path)) {
        strncpy(g_upgrade_cfg.nf_upgrade_img_path, str_buf, sizeof(g_upgrade_cfg.nf_upgrade_img_path) - 1);
    }
    if (TCIniGetKeyValueStringEx(&ini, "nf_upgrade", "se_img_file",
        str_buf, sizeof(str_buf), g_upgrade_cfg.nf_upgrade_se_img_path)) {
        strncpy(g_upgrade_cfg.nf_upgrade_se_img_path, str_buf, sizeof(g_upgrade_cfg.nf_upgrade_se_img_path) - 1);
    }

    // [mpu_upgrade]
    if (TCIniGetKeyValueStringEx(&ini, "mpu_upgrade", "upgrade_file",
        str_buf, sizeof(str_buf), g_upgrade_cfg.mpu_upgrade_file_name)) {
        strncpy(g_upgrade_cfg.mpu_upgrade_file_name, str_buf, sizeof(g_upgrade_cfg.mpu_upgrade_file_name) - 1);
    }
    if (TCIniGetKeyValueStringEx(&ini, "mpu_upgrade", "partition_record_file",
        str_buf, sizeof(str_buf), g_upgrade_cfg.mpu_partition_record_file)) {
        strncpy(g_upgrade_cfg.mpu_partition_record_file, str_buf, sizeof(g_upgrade_cfg.mpu_partition_record_file) - 1);
    }

    // [net_upgrade]
    if (TCIniGetKeyValueIntEx(&ini, "net_upgrade", "enable",
        &int_val, g_upgrade_cfg.net_upgrade_enable)) {
        g_upgrade_cfg.net_upgrade_enable = int_val;
    }
    if (TCIniGetKeyValueStringEx(&ini, "net_upgrade", "url_format",
        str_buf, sizeof(str_buf), g_upgrade_cfg.net_package_url_format)) {
        strncpy(g_upgrade_cfg.net_package_url_format, str_buf, sizeof(g_upgrade_cfg.net_package_url_format) - 1);
    }
    if (TCIniGetKeyValueStringEx(&ini, "net_upgrade", "pem_keys_dir",
        str_buf, sizeof(str_buf), g_upgrade_cfg.net_package_pem_keys_dir)) {
        strncpy(g_upgrade_cfg.net_package_pem_keys_dir, str_buf, sizeof(g_upgrade_cfg.net_package_pem_keys_dir) - 1);
    }
    if (TCIniGetKeyValueIntEx(&ini, "net_upgrade", "srt_id_addr",
        &int_val, g_upgrade_cfg.net_package_srt_id_addr)) {
        g_upgrade_cfg.net_package_srt_id_addr = int_val;
    }
    if (TCIniGetKeyValueIntEx(&ini, "net_upgrade", "srt_id_port",
        &int_val, g_upgrade_cfg.net_package_srt_id_port)) {
        g_upgrade_cfg.net_package_srt_id_port = int_val;
    }
    if (TCIniGetKeyValueIntEx(&ini, "net_upgrade", "srt_id_file",
        &int_val, g_upgrade_cfg.net_package_srt_id_file)) {
        g_upgrade_cfg.net_package_srt_id_file = int_val;
    }

    // [upgrade] - MCU/MPU upgrade enable
    if (TCIniGetKeyValueIntEx(&ini, "upgrade", "mcu_upgrade_enable",
        &int_val, (Int32)g_upgrade_cfg.mcu_upgrade_enable)) {
        g_upgrade_cfg.mcu_upgrade_enable = int_val != 0 ? True : False;
    }
    if (TCIniGetKeyValueIntEx(&ini, "upgrade", "mpu_upgrade_enable",
        &int_val, (Int32)g_upgrade_cfg.mpu_upgrade_enable)) {
        g_upgrade_cfg.mpu_upgrade_enable = int_val != 0 ? True : False;
    }

    // 4. 关闭文件
    TCIniDestroy(&ini);
    return 0;
}

/**
 * @brief 获取全局配置实例
 */
const TUpgradeCfg* UpgradeCfgGet(void)
{
    return &g_upgrade_cfg;
}
