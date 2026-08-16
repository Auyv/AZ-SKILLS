/**
* @addtogroup img_upgrade_file
* img_upgrade_file
* @{
*/
/**
* @file         img_upgrade_file.h
* @brief        升级的img文件类
* @note
* @author       Akee(shengkun.you@hopechart.com,尤胜坤)
*
* @par          修改日志
* <table>
* <tr><th>Date        <th>Version               <th>Author                                    <th>Description
* <tr><td>2023-04-17  <td>1.0                   <td>Akee(ysk@hopechart.com,尤胜坤)            <td>创建初始版本
* @copyright    杭州鸿泉物联网技术股份有限公司
*/
#ifndef _IMG_UPGRADE_FILE_H_
#define _IMG_UPGRADE_FILE_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#include "ffc/ffc_stream.h"

#define IMAGE_MAX_ITEM  15
#define HEADER_FIX_CHAR 0xAA

#define IXP_IXP_BIN     255  ///< IXP_X.BIN
#define IXP_IXP_PLUGIN  254  ///< IXP的插件

#define IXP_DST_ONCHIP_FLASH        0     ///< 片上Flash
#define IXP_DST_EXCHIP_FLASH        1     ///< 扩展Flash
#define IXP_DST_EXCHIP_FRAM         2     ///< 扩展Fram
#define IXP_DST_EXCHIP_E2PROM       3     ///< 扩展EEPROM
#define IXP_DST_EXCHIP_USBDISK      4     ///< USBDISK
#define IXP_DST_REMOTE_ANY          5     ///< REMOTE_ISP
#define IXP_DST_ONCHIP_EEPROM       6     ///< 片上EEPROM
#define IXP_DST_MPU_ONCHIP_FLASH    7     ///< 双mcu产品mpu的app

#pragma pack(push, 1)

/**
 * image项头
 */
typedef struct {
    uint16_t        crc;                           ///< 固件项 - 目标固件项数据部分校验码
    uint8_t         action;                        ///< 固件项 - 动作
    uint8_t         obj_dev;                       ///< 固件项 - 目标设备
    uint32_t        offset;                        ///< 固件项 - 包中偏移地址
    uint32_t        obj_addr;                      ///< 固件项 - 目标地址
    uint32_t        size;                          ///< 固件项 - 数据部分大小
    uint32_t        bak_src_addr;                  ///< 固件项 - 需备份的源地址
    uint32_t        bak_dst_addr;                  ///< 固件项 - 备份目标地址
    uint32_t        max_back_size;                 ///< 固件项 - 需备份数据最大尺寸
    uint32_t        reserved;                      ///< 固件项 - 保留
} TImageItemHeader;

/**
 * Img文件头
 */
typedef struct {
    uint16_t        crc;                           ///< 安装包 - CRC校验 （从head开始）
    uint8_t         fix_char;                      ///< 安装包 - 0xAA
    uint8_t         item_cnt;                      ///< 安装包 - 升级块数
    uint32_t        obj_position;                  ///< 安装包 - 包存储位置
    uint32_t        fwr_size;                      ///< 安装包 - 整个固件部分长度（不包含头部分）
    uint32_t        pkg_ver;                       ///< 安装包 - 包版本
    uint32_t        hw_ver;                        ///< 安装包 - 硬件版本
    uint32_t        release_time;                  ///< 安装包 - 发布时间
    uint8_t         reserved[8];                   ///< 安装包 - 保留
    TImageItemHeader fw_items[IMAGE_MAX_ITEM];     ///< 安装包 - 
} TImageHeader;

#pragma pack(pop)

/**
 * 检查文件头是否正常
 * True: 正常; False: 不正常
 */
Bool ImgUpgradeHeaderCheck(TImageHeader *header, UInt32 *hw_version);

/**
* TImgUpgradeFile
*/
typedef struct TImgUpgradeFileTag TImgUpgradeFile;
struct TImgUpgradeFileTag
{
    UInt8 *buffer;              ///< 临时buf
    TImageHeader img_header_;   ///< 文件头
    TFfStream *ff_stream_;      ///< 流对象指针
    Int32 file_size_;           ///< 文件大小
    UInt16 buffer_size;         ///< 临时buffer大小
    Bool is_open_;              ///< 是否已经打开
};
/**
 * 构造函数
 * @param self: 对象指针
 * @param ff_stream: 流对象指针
 * @param file_size: 文件大小
 * @param buffer: 临时buffer
 * @param buffer_size: buffer大小 大于等于512字节
 * @return 
 */
Bool TImgUpgradeFileCreate(TImgUpgradeFile *self, TFfStream *ff_stream, Int32 file_size, UInt8 *buffer, UInt16 buffer_size);

/**
 * 析构函数
 * @param self: 对象指针
 * @return 
 */
void TImgUpgradeFileDestroy(TImgUpgradeFile *self);

/**
 * 打开升级文件，内部会对整个文件校验，允许多次打开，每次都会数据进行校验
 * @param self: 对象指针
 * @param hw_version: 硬件版本号，如果传NULL，内部不做检测；如果传实际的硬件版本号，则会检查跟文件内部硬件版本号是否一致
 * @return True: 打开成功; False: 打开失败
 */
Bool TImgUpgradeFileOpen(TImgUpgradeFile *self, UInt32 *hw_version);

/**
 * 是否已经打开
 * @param self: 对象指针
 * @return 
 */
Bool TImgUpgradeFileIsOpen(TImgUpgradeFile *self);

/**
 * 关闭
 * @param self: 对象指针
 * @return 
 */
void TImgUpgradeFileClose(TImgUpgradeFile *self);

/**
 * 检查整个文件的Md5的值是否正确
 * @param self: 对象指针
 * @param md5_value: 文件计算后的md5值与此值进行计算，长度固定16字节
 * @return True: 正确; False: 不正确
 */
Bool TImgUpgradeFileCheckMd5(TImgUpgradeFile *self, UInt8 *md5_value, UInt32 *hw_version);

/**
 * 检查整个文件的文件头是否正确
 * @param self: 对象指针
 * @return True: 正确; False: 不正确
 */
Bool TImgUpgradeFileCheckHeader(TImgUpgradeFile *self, UInt32 *hw_version);

/**
 * 获取文件中存在多少个打包项
 * @param self: 对象指针
 * @return 打包项数
 */
Int32 TImgUpgradeFileItemCount(TImgUpgradeFile *self);

/**
 * 获取文件中存在多少个打包项
 * @param self: 对象指针
 * @return 打包项数
 */
TImageItemHeader *TImgUpgradeFileItems(TImgUpgradeFile *self, Int32 index);

/**
 * 获取目标类型在文件中的索引号
 * @param self: 对象指针
 * @param dst_dev: 目标类型，参考IXP_DST_ONCHIP_FLASH类似的宏
 * @return >=0: 索引号; <0: 不存在
 */
Int32 TImgUpgradeFileDstDevIndex(TImgUpgradeFile *self, UInt8 dst_dev);

/**
 * 读取目标类型索引的数据
 * @param self: 对象指针
 * @param index: 索引号
 * @param pos: 读取位置
 * @param buf: 读取的buf地址
 * @param buf_size: 读取的buf大小
 * @return <=0: 失败; >0: 实际读取的大小
 */
Int32 TImgUpgradeFileReadItem(TImgUpgradeFile *self, Int32 index, Int32 pos, UInt8 *buf, Int32 buf_size);

/**
 * 读取目标类型索引的总项大小
 * @param self: 对象指针
 * @param index: 索引号
 * @return <=0: 失败; >0: 实际项大小
 */
Int32 TImgUpgradeFileItemSize(TImgUpgradeFile *self, Int32 index);
#ifdef __cplusplus
} // extern "C" {
#endif
#endif
/**
 * @} 
 */
