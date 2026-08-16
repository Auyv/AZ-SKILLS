/**
 * @addtogroup rte_rfop_upgrade_img
 * rte_rfop_upgrade_img
 * @{
 */
/**
 * @file        rte_rfop_upgrade_img.h
 * @brief       XXXX
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

#ifndef _RTE_RFOP_UPGRADE_IMG_H_
#define _RTE_RFOP_UPGRADE_IMG_H_

#include "ofc/ofc_types.h"
#include "ffc/ffc_stream.h"
#include "rte_rfop_upgrade_sys_opt.h"
#include "img_upgrade_file.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct TRteRfopUpgradeImgFormatTag TRteRfopUpgradeImgFormat;
struct TRteRfopUpgradeImgFormatTag
{
    Bool is_valid;  // 是否有效
    UInt8 project_name_len;
    const Int8 *project_name; // 项目名称
    
    UInt8 soft_major; ///< 主版本
    UInt8 soft_minor; ///< 次版本
    UInt8 revision;   ///< 修订版本
    UInt8 internal;   ///< 内部版本

    UInt8 md5[16];
};


/**
 * 对文件名进行解词操作
 * @param *filename
 * @param *ret
 * @return 
 * @note:xxxxx
 */
Bool  RteRfopUpgradeImgFilenameTokenizer(const char *filename, TRteRfopUpgradeImgFormat *ret);


/*------------------------------------------------------*/

typedef struct TRteRfopUpgradeImgStreamTag TRteRfopUpgradeImgStream;
struct TRteRfopUpgradeImgStreamTag
{
    TRteRfopUpgradeImgFormat filename_format;
    TRteRfopUpgradeSysOptFileStream ff;
    TImgUpgradeFile img_;
    TImageItemHeader *app_item_header;
    Int32 app_item_header_idx;
    UInt32 app_item_bin_pos;
};

Int32 TRteRfopUpgradeImgStreamCreate(TRteRfopUpgradeImgStream *self, const char *filename);

void TRteRfopUpgradeImgStreamDestroy(TRteRfopUpgradeImgStream *self);

Bool TRteRfopUpgradeImgStreamOpen(TRteRfopUpgradeImgStream *self);

Bool TRteRfopUpgradeImgStreamIsOpen(TRteRfopUpgradeImgStream *self);

void TRteRfopUpgradeImgStreamClose(TRteRfopUpgradeImgStream *self);

Bool TRteRfopUpgradeImgStreamCheckMd5(TRteRfopUpgradeImgStream *self);

Bool TRteRfopUpgradeImgStreamCheckAppItemHeader(TRteRfopUpgradeImgStream *self);

Int32 TRteRfopUpgradeImgStreamAppItemRead(TRteRfopUpgradeImgStream *self, Int32 pos, UInt8 *buffer, Int32 size);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _RTE_RFOP_UPGRADE_IMG_H_

/**
 * @}  Generated on "2024-07-23 17:06:55" by the tool "gen_hq_file.py >> V20231119" 
 */

