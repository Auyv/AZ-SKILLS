/**
 * @file        rte_rfop_upgrade_img.c
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
 
#include "rte_rfop_upgrade_img.h"
#include "rfop/rfop_common.h"
#include "img_upgrade_file.h"
#include "rte_rfop_upgrade_srt_adp.h"
#include "rte_rfop_upgrade.h"
#include "rte_rfop_upgrade_sys_opt.h"
#include "rte_rfop_server.h"

#define ALOG_TAG "RFOP"
#include "alog/a_log.h"

#define FILENAME_LEN_MAX (128)



static Int32 L_TransSoftVerNum(const char *in_str, UInt8 *out_val, Bool *is_valid)
{
    Int8 *p = (Int8 *)in_str;
    Int32 val;
    Int32 t = RfopStrToUInt32((const char *)p, 3, (UInt32 *)&val);

    if (t > 0 && val <= 255)
    {
        *out_val = (UInt8)val;
        *is_valid = True;
    }
    else
    {
        t = -1;
        *is_valid = False;
    }

    return t;
}

Bool  RteRfopUpgradeImgFilenameTokenizer(const char *filename, TRteRfopUpgradeImgFormat *ret)
{
    Bool result = False;
    int len;
    Int8 *p = (Int8 *)RfopFromPathFindFilename(filename, FILENAME_LEN_MAX, &len);
    Int8 *e = (Int8 *)p + len;

    ALOG_D("filename:%s", p);
    ALOG_D("filename_len:%d ", len);
    // 获取项目名
    ret->project_name = p;
    while (p < e && *p > 0)
    {
        if (*p == '_')
        {
            ret->project_name_len = p - ret->project_name;
            break;
        }
        p++;
    }

    // 判断是否为 'V'
    p++;
    if (p < e && *p == 'V')
    {
        p++;
        result = p < e;
    }

    // 获取版本号 soft_major
    if (result)
    {
        Int32 t = L_TransSoftVerNum((const char *)p, &ret->soft_major, &result);
        if (result)
        {
            p += t;
            result = p < e;
        }
    }

    if (result && *p == '.')
    {
        p++;
        result = p < e;
    }

    // 获取版本号 soft_minor
    if (result)
    {
        Int32 t = L_TransSoftVerNum((const char *)p, &ret->soft_minor, &result);
        if (result)
        {
            p += t;
            result = p < e;
        }
    }

    if (result && *p == '.')
    {
        p++;
        result = p < e;
    }
    
    // 获取版本号 revision
    if (result)
    {
        Int32 t = L_TransSoftVerNum((const char *)p, &ret->revision, &result);
        if (result)
        {
            p += t;
            result = p < e;
        }
    }

    if (result && *p == '.')
    {
        p++;
        result = p < e;
    }

    // 获取版本号 revision
    if (result)
    {
        Int32 t = L_TransSoftVerNum((const char *)p, &ret->internal, &result);
        if (result)
        {
            p += t;
            result = p < e;
        }
    }

    if (result && *p == '_')
    {
        p++;
        result = p < e;
    }

    // 获取MD5
    if (result)
    {
        result = RfopHexStringToBytes((const char *)p, (char *)ret->md5, sizeof(ret->md5)) == sizeof(ret->md5);
    }
    
    ret->is_valid = result;
    return result;
}

/*--------------------------------------------------------------*/

Int32 TRteRfopUpgradeImgStreamCreate(TRteRfopUpgradeImgStream *self, const char *filename)
{
    Int32 result = kRfopHccParamInvalid;
    
    if (self != NULL && filename != NULL)
    {
        #if (0) // <2024-10-16> Jpz todo : 忽略文件名检查
        if (RteRfopUpgradeImgFilenameTokenizer(filename, &self->filename_format))
        #else
        self->filename_format.is_valid = True;
        if (self->filename_format.is_valid)
        #endif
        {
            result = TRteRfopUpgradeSysOptFileStreamCreate(&self->ff, filename);
            if (result >= 0)
            {
                Int32 file_size = TRteRfopUpgradeSysOptFileStreamFileSize(&self->ff);

                #if (1) // <2024-11-01> Jpz todo : 这里要看 TImgUpgradeFileCreate() 接口选择
                if (TImgUpgradeFileCreate(&self->img_, 
                        TRteRfopUpgradeSysOptFileStreamFfcHandle(&self->ff), 
                        file_size,
                        (UInt8 *)RTE_RFOP_UPGRADE_COMMON_BUFFER_ADDR, 
                        RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE))
                #else
                TImgUpgradeFileCreate(&self->img_, TRteRfopUpgradeSysOptFileStreamFfcHandle(&self->ff), file_size);
                if (file_size > 0)
                #endif
                {
                    result = file_size;
                }
                else
                {
                    TRteRfopUpgradeSysOptFileStreamDestroy(&self->ff);
                    result = kRfopHccUnknown;
                }
            }
        }
        else
        {
            result = kRfopHccFilenameInvalid;
        }
    }

    return result;
}

void TRteRfopUpgradeImgStreamDestroy(TRteRfopUpgradeImgStream *self)
{
    if (self != NULL)
    {
        TRteRfopUpgradeImgStreamClose(self);
        
        TImgUpgradeFileDestroy(&self->img_);
        TRteRfopUpgradeSysOptFileStreamDestroy(&self->ff);
    }
}

Bool TRteRfopUpgradeImgStreamOpen(TRteRfopUpgradeImgStream *self)
{
    Bool result = False;
    
    if (self == NULL)
        ;
    else if (TImgUpgradeFileIsOpen(&self->img_))
        result = True;
    else
    {
        UInt32 hw_version;
        UInt32 *hw_version_ptr = NULL;

        if (RteRfopUpgradeSrtAdpGetHardwareVersion(&hw_version) >= 0)
        {
            hw_version_ptr = &hw_version;
        }
        result = TImgUpgradeFileOpen(&self->img_, hw_version_ptr);
    }

    return result;
}
Bool TRteRfopUpgradeImgStreamIsOpen(TRteRfopUpgradeImgStream *self)
{
    Bool result = False;

    if (self != NULL)
        result = TImgUpgradeFileIsOpen(&self->img_);
        
    return result;
}
void TRteRfopUpgradeImgStreamClose(TRteRfopUpgradeImgStream *self)
{
    if (self != NULL)
    {
        if (TImgUpgradeFileIsOpen(&self->img_))
        {
            TImgUpgradeFileClose(&self->img_);
        }
    }
}

Bool TRteRfopUpgradeImgStreamCheckMd5(TRteRfopUpgradeImgStream *self)
{
    Bool result = False;
    
    if (self != NULL)
    {
        UInt32 hw_version;
        UInt32 *hw_version_ptr = NULL;

        if (RteRfopUpgradeSrtAdpGetHardwareVersion(&hw_version) >= 0)
        {
            hw_version_ptr = &hw_version;
        }

        result = TImgUpgradeFileCheckMd5(&self->img_, self->filename_format.md5, hw_version_ptr);
    }

    return result;
}

Bool TRteRfopUpgradeImgStreamCheckAppItemHeader(TRteRfopUpgradeImgStream *self)
{
    Bool result = False;
    Int32 count = TImgUpgradeFileItemCount(&self->img_);
    TImageItemHeader *item_header = NULL;
    Int32 idx = 0;
    
    // <2024-11-20> Jpz : 找到mcu item header
    for (; idx < count; idx++)
    {
        item_header = TImgUpgradeFileItems(&self->img_, idx);

        if (item_header == NULL)
        {
            RFOP_UPGRADE_EXCEPTION(kRfopHccUnknown, "TImgUpgradeFileItems()");
            break;
        }
        else if (item_header->obj_dev != IXP_DST_ONCHIP_FLASH)
        {
            ALOG_W("item_header->obj_dev: %d != IXP_DST_ONCHIP_FLASH", item_header->obj_dev);
            continue;
        }
        else if (item_header->obj_addr != RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_ADDR)
        {
            ALOG_W("item_header->obj_addr:%d, RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_ADDR:%d", item_header->obj_addr, RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_ADDR);
            continue;
        }
        else if (item_header->size > RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_SIZE)
        {
            ALOG_W("item_header->size:%d, RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_SIZE:%d", item_header->size, RTE_RFOP_UPGRADE_APPBIN_IN_FLASH_SIZE);
            continue;
        }
        else
        {
            result = True;
            break;
        }
    }
    
    if (result)
    {
        self->app_item_header = item_header;
        self->app_item_header_idx = idx;
    }
    else
    {
        self->app_item_header = NULL;
        self->app_item_header_idx = -1;
    }

    ALOG_D("TRteRfopUpgradeImgStreamCheckAppItemHeader() result:%d, idx:%d, app_item_header:%p", result, idx, self->app_item_header);
    return result;
}


Int32 TRteRfopUpgradeImgStreamAppItemRead(TRteRfopUpgradeImgStream *self, Int32 pos, UInt8 *buffer, Int32 size)
{
    return TImgUpgradeFileReadItem(&self->img_, self->app_item_header_idx, pos, buffer, size);
}

///< Generated on "2024-07-23 17:06:55" by the tool "gen_hq_file.py >> V20231119" 

