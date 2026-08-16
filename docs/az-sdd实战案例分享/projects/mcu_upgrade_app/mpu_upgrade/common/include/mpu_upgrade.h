/**
 * @addtogroup mpu_upgrade
 * mpu_upgrade
 * @{
 */
/**
 * @file        mpu_upgrade.h
 * @brief       用于统一调用mpu升级相关接口
 *              1.查看当前升级状态
 *              2.设置升级文件路径
 *              3.执行升级动作(开始执行不得取消, 阻塞执行)
 *              4.查看当前升级进度
 *              5.查看分区情况(非必须)
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/14
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/14   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _MPU_UPGRADE_H_
#define _MPU_UPGRADE_H_

#ifndef HQ_CMAKE_BUILD_MODEL_TYPE
    #define HQ_CMAKE_BUILD_MODEL_TYPE -1
    #error "Please define HQ_CMAKE_BUILD_MODEL_TYPE in your project"
#endif

// HQ_CMAKE_BUILD_MODEL_TYPE 值范围
#define HQ_CMAKE_BUILD_MODEL_AG35_ASR1806       0  // 0: ag35_eutda ag35_cetda : asr1806系列
#define HQ_CMAKE_BUILD_MODEL_AG35_CEVN          1  // 1: ag35_cevn : 高通
#define HQ_CMAKE_BUILD_MODEL_AR590              2  // 2: ar590 : 展锐
#define HQ_CMAKE_BUILD_MODEL_AG35_ASR1803       3  // 3: ag35_ceraa : asr1803系列
#define HQ_CMAKE_BUILD_MODEL_NEOWAY_N725A       4  // 4: n725a : 有方N725A系列
#define HQ_CMAKE_BUILD_MODEL_SIMCOM7805         10  // 10: sim7805 : 系列

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_ASR1806)
#elif (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_CEVN)
#elif (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AR590)
#elif (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_AG35_ASR1803)
#elif (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_NEOWAY_N725A)
#elif (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_SIMCOM7805)
#else
    #error "G_BUILD_MODEL is invalid"
#endif


#ifdef __cplusplus
extern "C"
{
#endif

enum TMpuUpgradeErrTag
{
    kMpuUpgradeErrNone = 0,   // 无错误
    kMpuUpgradeErrInvalidParam = -1,  // 参数错误
    kMpuUpgradeErrInvalidState = -2,  // 状态错误
    kMpuUpgradeErrInvalidPath = -3,  // 文件路径错误
    kMpuUpgradeErrTimeout = -4,     // 操作超时
    kMpuUpgradeErrOptFailed = -5,   // 操作失败
    kMpuUpgradeErrInvalidFile = -6,  // 文件错误
};
typedef enum TMpuUpgradeErrTag TMpuUpgradeErr;


enum TMpuUpgradeStateTag
{
    kMpuUpgradeStateInvalid = 0,    // 无效状态
    kMpuUpgradeStateIdle = 1,       // 空闲状态
    kMpuUpgradeStateUpgrading = 2,  // 升级中
    kMpuUpgradeStateBackuping = 3,  // 备份中
    kMpuUpgradeStateWaitReboot = 4, // 等待重启
    kMpuUpgradeStateWaitConfirm = 5,  // 等待程序确认
    kMpuUpgradeStateUpdateSuccess = 6,  // 程序升级成功
    kMpuUpgradeStateUpdateFailed = 7,  // 升级失败

    kMpuUpgradeStateMax
};
typedef enum TMpuUpgradeStateTag TMpuUpgradeState;


typedef struct TMpuPartitionInfoTag TMpuPartitionInfo;
struct TMpuPartitionInfoTag
{
    char active_part;   // ascii字符, 有效值 [A-B].
    char other_part_is_damage;  // 其他分区是否损坏. 有效值 [0,1]. 0-正常, 1-损坏
};


/**
 * 升级/备份进度回调函数
 * @param state     当前状态
 * @param percent   >= 进度百分比, 0-100; < 0, 升级失败,故障码 @ref TMpuUpgradeErrTag
 * @param *user_param   用户传参
 * @return 
 * @note:不同模组单独适配
 */
typedef void (*TMpuUpgradeCallback)(TMpuUpgradeState state, int percent, void *user_param);

/**
 * @brief 初始化MPU升级模块
 * @return 成功返回0，失败返回错误码
 */
int MpuUpgradeInit(void);

/**
 * @brief 循环执行MPU升级功能组件。
 * @return int 成功返回0，失败返回负错误码。
 * @note 部分模组可以不调用,可用于实时检测是否需要自动修复损坏分区
 */
void MpuUpgradeRunOnce(void);

/**
 * @brief 用于通知升级组件结束
 * @return 
 * @note:xxxxx
 */
void MpuUpgradeDone(void);

/**
 * @brief 获取当前升级状态
 * @return       当前升级状态
 * @note:xxxxx
 */
TMpuUpgradeState MpuUpgradeState(void);


/**
 * @brief 获取升级文件路径
 * @return 当前设置的升级文件路径
 * @note:xxxxx
 */
char *MpuUpgradeImgPath(void);
/**
 * @brief 设置升级文件路径
 * @param *img_path 升级文件路径
 * @return == 0, 成功; < 0, 失败
 * @note:xxxxx
 */
int MpuUpgradeSetImgPath(char *img_path);


/**
 * 执行升级更新分区的操作
 * 
 * @param timeout 操作超时时间，单位为秒
 * @param user_param 用户自定义参数，用于回调函数
 * @param callback 升级过程中的回调函数指针
 * 
 * @return 返回操作结果，0表示成功，非0表示失败
 * 
 * 此函数负责在指定的超时时间内完成升级更新分区的任务
 * 它会调用回调函数来通知用户升级过程中的状态
 */
int MpuUpgradeExecUpdate(int timeout_s, void *user_param, TMpuUpgradeCallback callback);

/**
 * 执行升级备份分区的操作
 * 
 * @param timeout 操作超时时间，单位为秒
 * @param user_param 用户自定义参数，用于回调函数
 * @param callback 升级过程中的回调函数指针
 * 
 * @return 返回操作结果，0表示成功，非0表示失败
 * 
 * 此函数负责在指定的超时时间内完成升级备份分区的任务
 * 它会调用回调函数来通知用户升级过程中的状态
 */
int MpuUpgradeExecBackup(int timeout_s, void *user_param, TMpuUpgradeCallback callback);

/**
 * 执行回滚分区的操作
 * 
 * @param timeout 操作超时时间，单位为秒
 * @param user_param 用户自定义参数，用于回调函数
 * @param callback 升级过程中的回调函数指针
 * 
 * @return 返回操作结果，0表示成功，非0表示失败
 * 
 * 此函数负责在指定的超时时间内完成系统主动回滚的任务
 * 它会调用回调函数来通知用户升级过程中的状态
 */
int MpuUpgradeExecRollback(int timeout_s, void *user_param, TMpuUpgradeCallback callback);

/**
 * 获取MPU的分区信息函数
 * 
 * @param partition_info 返回当前分区信息
 * 
 * @return == 0, 成功; < 0, 失败
 */
int MpuUpgradeGetPartitionInfo(TMpuPartitionInfo *partition_info);

#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _MPU_UPGRADE_H_


