/**
 * @addtogroup se_img
 * se_img
 * @{
 */
/**
 * @file        se_img.h
 * @brief       针对se的img文件操作, 主要包括:
 *              1. 读取se_img文件头部信息
 *              2. 校验se_img文件是否合法有效
 *              3. 读取se_img文件中的签名数据
 *              4. 读取se_img文件中的加密数据
 *  
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/03/05
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/03/05   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _SE_IMG_H_
#define _SE_IMG_H_

#include "ofc/ofc_types.h"
#include "ota_packag_head_dpt_data.h"
#include "ffc/ffc_file.h"


enum TSeImgFileIdTag
{
    kSeImgFileIdContextAse256CbcKey = 1,
    kSeImgFileIdContextAse256CbcIv  = 2,

    kSeImgFileIdSignatureRsaPublicPem  = 10,
};
typedef enum TSeImgFileIdTag TSeImgFileId; 

typedef struct TSeImgHeaderTag TSeImg;

typedef void(*TSeImgReadContextCallback)(void *context, UInt32 length);
typedef const char *(*TSeImgFileNameFunc)(TSeImgFileId file_id, void *user_param);

struct TSeImgHeaderTag 
{
    const char *filename;
    THandle fp;
    Int32 fp_file_size;
    TOtaPackagHeadMsgOtaPackageHead head_info;
    Bool is_valid;

    TSeImgFileNameFunc file_name_func;
    void *user_param;
};


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 创建一个SE镜像对象
 * @param filename SE镜像文件名
 * @return 返回创建的SE镜像对象指针,如果创建失败返回NULL
 * @note 使用完毕后需要调用DestroyTSeImg释放资源
 */
TSeImg *CreateTSeImg(const char *filename);

/**
 * @brief 销毁SE镜像对象并释放相关资源
 * @param se_img 要销毁的SE镜像对象指针
 * @note 该函数用于释放由CreateTSeImg创建的SE镜像对象
 */
void DestroyTSeImg(TSeImg *self);

/**
 * @brief 校验SE镜像文件是否有效
 * @param se_img SE镜像对象指针
 * @return 如果SE镜像有效返回True，否则返回False
 * @note 该函数会检查SE镜像文件的完整性和合法性
 */
Bool  TSeImgIsValid(TSeImg *self);

/**
 * @brief 设置文件名获取函数
 * @param self SE镜像对象指针
 * @param method 文件名获取函数指针,用于根据文件ID获取对应的文件名
 * @return 设置成功返回True,失败返回False
 * @note 该函数用于设置一个回调函数,通过该函数可以根据文件ID获取对应的文件名
 */
Bool TSeImgSetFileNameFunc(TSeImg *self, TSeImgFileNameFunc method, void *user_param);

/**
 * @brief 根据文件ID获取文件名
 * @param self SE镜像对象指针
 * @param file_id 文件ID
 * @return 返回文件ID对应的文件名字符串,如果获取失败返回NULL
 * @note 该函数通过之前设置的文件名获取函数来获取指定ID的文件名
 */
const char *TSeImgFileName(TSeImg *self, TSeImgFileId file_id);


/**
 * @brief 将SE镜像文件解包到指定文件
 * @param self SE镜像对象指针
 * @param dest_filename 目标文件名
 * @param callback 解包过程中的回调函数,用于处理解包的数据
 * @return 成功返回解包的字节数,失败返回负值
 * @note 该函数将SE镜像文件解包并保存到指定的目标文件中,解包过程通过callback回调函数处理数据
 */
Int32 TSeImgUnpackImgToFile(TSeImg *self, const char *dest_filename, TSeImgReadContextCallback callback);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _SE_IMG_H_


