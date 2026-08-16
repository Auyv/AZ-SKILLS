/**
 * @addtogroup upgrade_app_cfg
 * upgrade_app_cfg
 * @{
 */
/**
 * @file        upgrade_app_cfg.h
 * @brief       升级应用配置管理 - 支持 INI 文件加载
 * @note        当 INI 文件不存在时，使用编译时默认值
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/23
 * @version     2.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * <tr><td>2026/05/08   <td>2.0         <td>AI           <td> 新增 INI 配置加载支持
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _UPGRADE_APP_CFG_H_
#define _UPGRADE_APP_CFG_H_

#include "ofc/ofc_types.h"

// ============================================================
// 默认值宏定义 (INI 文件不存在时使用)
// ============================================================

// 基础路径
#define DEFAULT_BASE_PATH               "/media/card/"

// 日志文件
#define DEFAULT_LOG_FILE_DIR            DEFAULT_BASE_PATH"/log/upgrade/"
#define DEFAULT_LOG_FILE_TAG            "nfupgrade"
#define DEFAULT_LOG_FILE_SIZE           (1024 * 1024 * 1)
#define DEFAULT_LOG_FILE_COUNT          5

// NFCREATE.IMG 文件名称
#define DEFAULT_IMG_FILE_DIR            DEFAULT_BASE_PATH
#define DEFAULT_NFCREATE_UPGRADE_IMG_PATH       DEFAULT_IMG_FILE_DIR"/nfxx_upgrade.img"
#define DEFAULT_NFCREATE_UPGRADE_SE_IMG_PATH    DEFAULT_IMG_FILE_DIR"/se_nfxx_upgrade.img"

// MPU升级文件名称
#define DEFAULT_MPU_UPGRADE_FILE_NAME           DEFAULT_IMG_FILE_DIR"/quectel_AB_OTA.img"

// MPU 分区标志文件
#define DEFAULT_MPU_UPGRADE_PARTITION_RECORD_FILE   DEFAULT_LOG_FILE_DIR"before_upgrade_partition"

// 自动下载网络升级包功能. 0:关闭, 1:开启
#define DEFAULT_NET_UPGRADE_ENABLE              1
#define DEFAULT_NET_PACKAGE_URL_FORMAT          "http://%s:%d/%s"
#define DEFAULT_NET_PACKAGE_PEM_KEYS_DIR        "/vendor/data/tee/upgrade/key/"
#define DEFAULT_NET_PACKAGE_SRT_ID_ADDR         471
#define DEFAULT_NET_PACKAGE_SRT_ID_PORT         472
#define DEFAULT_NET_PACKAGE_SRT_ID_FILE         473


// 为支持配置单cpu模式增加配置项. 默认 mcu 和 mpu 升级都使能. 0:关闭, 1:开启
#define DEFAULT_MCU_UPGRADE_ENABLE              1
#define DEFAULT_MPU_UPGRADE_ENABLE              1

// ============================================================
// INI 文件路径
// ============================================================
#define UPGRADE_CFG_INI_PATH            "/vendor/app/hq/etc/nfupgrade_cfg.ini"



#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================
// 运行时配置结构体
// ============================================================
typedef struct TUpgradeCfgTag {
    // 日志配置
    char log_file_dir[256];
    char log_file_tag[64];
    Int32 log_file_size;
    Int32 log_file_count;

    // NF 升级文件
    char nf_upgrade_img_path[256];
    char nf_upgrade_se_img_path[256];

    // MPU 升级文件
    char mpu_upgrade_file_name[256];
    char mpu_partition_record_file[256];

    // 网络升级配置
    Int32 net_upgrade_enable;
    char net_package_url_format[128];
    char net_package_pem_keys_dir[256];
    Int32 net_package_srt_id_addr;
    Int32 net_package_srt_id_port;
    Int32 net_package_srt_id_file;

    // MCU/MPU 升级使能配置
    Bool mcu_upgrade_enable;
    Bool mpu_upgrade_enable;
} TUpgradeCfg;

// ============================================================
// 兼容性宏 (保持原有代码编译通过)
// ============================================================
#define BASE_PATH                       DEFAULT_BASE_PATH
#define LOG_FILE_DIR                    g_upgrade_cfg.log_file_dir
#define LOG_FILE_TAG                    g_upgrade_cfg.log_file_tag
#define LOG_FILE_SIZE                   g_upgrade_cfg.log_file_size
#define LOG_FILE_COUNT                  g_upgrade_cfg.log_file_count
#define IMG_FILE_DIR                    DEFAULT_BASE_PATH
#define NFCREATE_UPGRADE_IMG_PATH       g_upgrade_cfg.nf_upgrade_img_path
#define NFCREATE_UPGRADE_SE_IMG_PATH    g_upgrade_cfg.nf_upgrade_se_img_path
#define MPU_UPGRADE_FILE_NAME           g_upgrade_cfg.mpu_upgrade_file_name
#define MPU_UPGRADE_PARTITION_RECORD_FILE   g_upgrade_cfg.mpu_partition_record_file
#define NET_UPGRADE_ENABLE              g_upgrade_cfg.net_upgrade_enable
#define NET_PACKAGE_URL_FORMAT          g_upgrade_cfg.net_package_url_format
#define NET_PACKAGE_PEM_KEYS_DIR        g_upgrade_cfg.net_package_pem_keys_dir
#define NET_PACKAGE_SRT_ID_ADDR         g_upgrade_cfg.net_package_srt_id_addr
#define NET_PACKAGE_SRT_ID_PORT         g_upgrade_cfg.net_package_srt_id_port
#define NET_PACKAGE_SRT_ID_FILE         g_upgrade_cfg.net_package_srt_id_file
#define MCU_UPGRADE_ENABLE              g_upgrade_cfg.mcu_upgrade_enable
#define MPU_UPGRADE_ENABLE              g_upgrade_cfg.mpu_upgrade_enable

// ============================================================
// 外部配置实例声明
// ============================================================
extern TUpgradeCfg g_upgrade_cfg;

// ============================================================
// 配置管理接口
// ============================================================

/**
 * @brief 初始化配置，从 INI 文件加载
 * @param ini_file_path INI 文件路径，NULL 使用默认路径 UPGRADE_CFG_INI_PATH
 * @return 0 成功加载, -1 文件不存在或读取失败(使用默认值)
 */
Int32 UpgradeCfgInit(const char *ini_file_path);

/**
 * @brief 获取全局配置实例
 */
const TUpgradeCfg* UpgradeCfgGet(void);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _UPGRADE_APP_CFG_H_

/**
 * @}  Generated on "2024-12-23 16:45:27" by the tool "gen_hq_file.py >> V20231119"
 */
