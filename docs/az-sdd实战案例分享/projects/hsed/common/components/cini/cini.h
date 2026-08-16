/**
 * @addtogroup cini
 * cini
 * @{
 */
/**
 * @file        cini.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/11/12
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/11/12   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _CINI_H_
#define _CINI_H_

#include "ofc/ofc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct TCIniTag TCIni;
struct TCIniTag
{
    void *ini_handle;
};

Int32 TCIniCreate(TCIni *self, const char *filename);
void  TCIniDestroy(TCIni *self);


Int32 TCiniClear(TCIni *self);
Int32 TCIniUpdateToFile(TCIni *self);

Int32 TCIniGetSectionCount(TCIni *self);
Int32 TCIniGetSectionName(TCIni *self, Int32 index, char *sectionName, Int32 sectionNameLen);

Bool TCIniGetKeyValueString(TCIni *self, const char *sectionName, const char *keyName, char *value, Int32 valueLen);
Bool TCIniGetKeyValueStringEx(TCIni *self, const char *sectionName, const char *keyName, char *value, Int32 valueLen, const char *defaultValue);
Bool TCIniSetKeyValueString(TCIni *self, const char *sectionName, const char *keyName, const char *value);

Bool TCIniGetKeyValueInt(TCIni *self, const char *sectionName, const char *keyName, Int32 *value);
Bool TCIniGetKeyValueIntEx(TCIni *self, const char *sectionName, const char *keyName, Int32 *value, Int32 defaultValue);
Bool TCIniSetKeyValueInt(TCIni *self, const char *sectionName, const char *keyName, Int32 value);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _CINI_H_


