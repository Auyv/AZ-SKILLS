/**
 * @addtogroup a_log_backend_file_impl.h
 * a_log_backend_file_impl.h
 * @{
 */
/**
 * @file        a_log_backend_file_impl.h.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/23
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/23   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _A_LOG_BACKEND_FILE_IMPL_H_
#define _A_LOG_BACKEND_FILE_IMPL_H_

#include "alog/a_log_interface.h"
#include "ffc/ffc_file.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 *   
 */

typedef struct TDicInfoTag TDicInfo;
struct TDicInfoTag
{
    Int32 tag_file_cnt;
    
    Bool oldest_exist;
    Bool latest_exist;
    char oldest_name[128];
    char latest_name[128];
};  

typedef struct TAlogBackendFileParamTag TAlogBackendFileParam;
struct TAlogBackendFileParamTag
{
    char *file_dir;
    char *file_name;
    UInt32 file_size_max;
    UInt32 file_count_max;
};


typedef struct TAlogBackendFileImplTag TAlogBackendFileImpl;
struct TAlogBackendFileImplTag
{
    const char *file_dir;
    const char *file_name;
    UInt32 file_size_max;
    UInt32 file_count_max;
    
    THandle curr_fd_;
    TDicInfo dic_info_;

    IALog alog_interface_;
    Bool enable_;
};

void TAlogBackendFileImplCreate(TAlogBackendFileImpl *self, TAlogBackendFileParam *param);
void TAlogBackendFileImplDestroy(TAlogBackendFileImpl *self);
IALog *TAlogBackendFileImplInterface(TAlogBackendFileImpl *self);
void TAlogBackendFileImplSetEnable(TAlogBackendFileImpl *self, Bool enable);
Bool TAlogBackendFileImplEnable(TAlogBackendFileImpl *self);





#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _A_LOG_BACKEND_FILE_IMPL_H_

/**
 * @}  Generated on "2024-12-23 14:10:39" by the tool "gen_hq_file.py >> V20231119" 
 */

