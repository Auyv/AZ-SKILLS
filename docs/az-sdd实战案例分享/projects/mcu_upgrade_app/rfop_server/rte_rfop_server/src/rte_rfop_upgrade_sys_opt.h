/**
 * @addtogroup rte_rfop_upgrade_sys_opt
 * rte_rfop_upgrade_sys_opt
 * @{
 */
/**
 * @file        rte_rfop_upgrade_sys_opt.h
 * @brief       用户具体实现，rte_rfop_upgrade.c 中直接调用
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/07/23
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/07/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _RTE_RFOP_UPGRADE_SYS_OPT_H_
#define _RTE_RFOP_UPGRADE_SYS_OPT_H_

#include "ofc/ofc_types.h"
#include "ffc/ffc_stream.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RFOP_UPGRADE_EXCEPTION(code, info)  RteRfopUpgradeSysOptExceptionNotify(__func__, __LINE__, code, info)


/*---------------------------------------------------------*/

/**
 * 初始化相关资源
 * @return 
 * @note:xxxxx
 */
void RteRfopUpgradeSysOptInit(void);

void RteRfopUpgradeSysOptClear(void);

void RteRfopUpgradeSysOptDone(void);



/*---------------------------------------------------------*/


// 文件操作
/**
 * 文件打开接口
 * @param *self     rfop客户端对象指针
 * @param *path     远端文件名。长度不超过255Byte，包含结尾‘\0’
 * @param mode      打开模式。 参考 TRfopFom
 * @return >= 0     打开成功，文件句柄
 *         <  0     打开失败，返回错误码
 * @note:xxxxx
 */
Int32 RteRfopUpgradeSysOptDoFileOpen(const char *path, UInt8 mode);

/**
 * 文件数据读取
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param *buffer   存放读取数据的缓冲区指针
 * @param size      缓冲区的字节数
 * @return  >= 0     读取成功, 返回实际读取的字节数
 *          (其它值)  读取失败的返回码
 * @note:xxxxx
 */
Int32 RteRfopUpgradeSysOptDoFileRead(Int32 fd, char *buffer, Int32 size);

/**
 * 文件数据写入
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param *data     待写入的数据指针
 * @param size      待写入的数据字节数
 * @return  >= 0     写入成功, 返回实际写入的字节数
 *          (其它值)  写入失败的返回码
 * @note:xxxxx
 */
// Int32 RteRfopUpgradeSysOptDoFileWrite(Int32 fd, const char *data, Int32 size);

/**
 * 文件关闭
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @return  >= 0     关闭成功
 *          (其它值)  关闭失败的返回码
 * @note:xxxxx
 */
Bool RteRfopUpgradeSysOptDoFileClose(Int32 fd);


/**
 * 文件光标偏移量设置
 * @param *self     rfop客户端对象指针
 * @param fd        文件句柄
 * @param offset    偏移量
 * @param whence    参考 TRfopSeekWhence
 * @return 返回新的文件指针位置，单位通常是字节。如果发生错误，会返回-1
 * @note:xxxxx
 */
Int64 RteRfopUpgradeSysOptDoFileSeek(Int32 fd, Int64 offset, Int32 whence);

/*---------------------------------------------------------*/

/**
 * 升级过程中状态通知
 * @param state      当前升级流程步骤
 * @param percent   升级进度
 * @param result    升级结果。当 state == kRteRfopUpgradestateDone 时有效。 > 0 表示成功，0 表示无效值，< 0 表示失败.
 * @return 
 * @note:xxxxx
 */
void RteRfopUpgradeSysOptStateNotify(UInt8 task_id, UInt8 state, UInt8 percent, Int32 result);


void RteRfopUpgradeSysOptExceptionNotify(const char *func_name, UInt32 line, Int32 error_code, const char *error_info);

/*---------------------------------------------------------*/

// 适配FFC的流对象
typedef struct TRteRfopUpgradeSysOptFileStreamTag TRteRfopUpgradeSysOptFileStream;
struct TRteRfopUpgradeSysOptFileStreamTag
{
    TFfStream stream;
    Int32 fd_;
};

Int32 TRteRfopUpgradeSysOptFileStreamCreate(TRteRfopUpgradeSysOptFileStream *self, const char *filename);
void TRteRfopUpgradeSysOptFileStreamDestroy(TRteRfopUpgradeSysOptFileStream *self);
TFfStream *TRteRfopUpgradeSysOptFileStreamFfcHandle(TRteRfopUpgradeSysOptFileStream *self);
Int32 TRteRfopUpgradeSysOptFileStreamFileSize(TRteRfopUpgradeSysOptFileStream *self);

/*---------------------------------------------------------*/


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _RTE_RFOP_UPGRADE_SYS_OPT_H_

/**
 * @}  Generated on "2024-07-23 10:54:25" by the tool "gen_hq_file.py >> V20231119" 
 */

