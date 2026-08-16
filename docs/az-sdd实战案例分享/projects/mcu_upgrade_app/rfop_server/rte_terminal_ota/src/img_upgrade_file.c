#include "img_upgrade_file.h"
#include <string.h>
#include "mfc/mfc_md5.h"
#include "ofc/ofc_crc16.h"

#define ALOG_TAG ("ImgUpgrade")

#include "alog/alog.h"
#include <stdio.h>

// static Bool CheckHardwareVer(UInt32 pkg_hardware_ver)
// {
//     UInt32 hardware_ver;
//     Bool result;

//     if (ProductIntoGetHwVer((UInt8 *)&hardware_ver, sizeof(hardware_ver)) == sizeof(hardware_ver))
//     {
//         result = pkg_hardware_ver == hardware_ver;
//     }
//     else
//         result = True;     ///< 硬件版本号无法读取也返回成功

//     return result;
// }

static UInt16 ConvertWord(uint16_t val)
{
    return (val << 8) | (val >> 8);
}

Bool ImgUpgradeHeaderCheck(TImageHeader *header, UInt32 *hw_version)
{
    Bool result = False;
    UInt8 *head = (UInt8 *)header;

    // 比较硬件版本号和CRC等  保留位需要有X
    // 硬件版本号为空，或者值为0xFFFFFFFF都算通过
    if (header != NULL)
    {
        // result = header->fix_char == HEADER_FIX_CHAR
        //          && header->item_cnt <= IMAGE_MAX_ITEM
        //         && (hw_version == NULL || ((*hw_version == (header->hw_ver) || *hw_version == 0xFFFFFFFF)))
        //          && (OfCalcCrc16(&head[2], sizeof(TImageHeader) - sizeof(UInt16)) == ConvertWord(header->crc))
        //          && ((header->reserved[0] == 'X') && ((header->fix_char ^ header->reserved[0]) == header->reserved[1]));
        
        // 检查固定字符
        if (header->fix_char != HEADER_FIX_CHAR) {
            ALOG_E("Header check failed: fix_char mismatch, expected: %c, img_actual: %c", HEADER_FIX_CHAR, header->fix_char);
            result = False;
        }
        // 检查项目数量
        else if (header->item_cnt > IMAGE_MAX_ITEM) {
            ALOG_E("Header check failed: item count exceeds max:%d, img_count: %d", IMAGE_MAX_ITEM, header->item_cnt);
            result = False;
        }
        // 检查硬件版本号
        else if (hw_version != NULL && (*hw_version != header->hw_ver && *hw_version != 0xFFFFFFFF)) {
            ALOG_E("Header check failed: hardware version mismatch, expected: 0x%x(%d), img_actual: 0x%x(%d)", 
                   *hw_version, *hw_version, header->hw_ver, header->hw_ver);
            result = False;
        }
        // 检查CRC校验
        else if (OfCalcCrc16(&head[2], sizeof(TImageHeader) - sizeof(UInt16)) != ConvertWord(header->crc)) {
            UInt16 calculated_crc = OfCalcCrc16(&head[2], sizeof(TImageHeader) - sizeof(UInt16));
            UInt16 converted_header_crc = ConvertWord(header->crc);
            ALOG_E("Header check failed: CRC mismatch, img_calculated: 0x%x, img_header: 0x%x", 
                   calculated_crc, converted_header_crc);
            result = False;
        }
        // 检查保留字段
        else if (header->reserved[0] != 'X') {
            ALOG_E("Header check failed: reserved[0] should be 'X', img_actual: %c", header->reserved[0]);
            result = False;
        }
        else if ((header->fix_char ^ header->reserved[0]) != header->reserved[1]) {
            ALOG_E("Header check failed: reserved[1] validation failed, img_fix_char: %c, reserved[0]: %c, reserved[1]: %c, xor result: %c", 
                   header->fix_char, header->reserved[0], header->reserved[1], 
                   (char)(header->fix_char ^ header->reserved[0]));
            result = False;
        }
        else {
            result = True;
        }
    }

    return result;
}

Bool TImgUpgradeFileCreate(TImgUpgradeFile *self, TFfStream *ff_stream, Int32 file_size, UInt8 *buffer, UInt16 buffer_size)
{
    Bool result = False;
    
    if (ff_stream != NULL && file_size > 0 && buffer != NULL && buffer_size >= 512)
    {
        memset(self, 0, sizeof(*self));
        self->ff_stream_ = ff_stream;
        self->file_size_ = file_size;
        self->buffer = buffer;
        self->buffer_size = 512;
        result = True;
    }
    else
    {
        ALOG_E("%s invalid param, ff_stream:%p, file_size:%d, buffer:%p, buffer_size:%d", 
            __func__, ff_stream, file_size, buffer, buffer_size);
    }
    
    return result;
}

void TImgUpgradeFileDestroy(TImgUpgradeFile *self)
{

}

Bool TImgUpgradeFileCheckMd5(TImgUpgradeFile *self, UInt8 *md5_value, UInt32 *hw_version)
{
    Bool result = False;

    if (md5_value != NULL && TImgUpgradeFileCheckHeader(self, hw_version))
    {
        Int32 pos = TFfStreamPosition(self->ff_stream_);
        Int32 total_size = 0;
        UInt8 md5_calc_value[16];

        TFfStreamSetPosition(self->ff_stream_, total_size);
        TMfMd5Ctx md_ctx = {0};

        TMfMd5CtxInit(&md_ctx);

        while (total_size < self->file_size_)
        {
            Int32 signel_size = self->file_size_ - total_size;

            if (signel_size > self->buffer_size)
                signel_size = self->buffer_size;

            Int32 read_size = TFfStreamRead(self->ff_stream_, self->buffer, signel_size);

            if (read_size > 0)
            {
                TMfMd5CtxEncode(&md_ctx, self->buffer, read_size);
                total_size += read_size;
            }
            else
                break;
        }

        printf("total_size: %d, self->file_size_: %d\n", total_size, self->file_size_);
        if (total_size >= self->file_size_)
        {
            TMfMd5CtxFinal(&md_ctx, md5_calc_value);

            if (memcmp(md5_value, md5_calc_value, sizeof(md5_calc_value)) == 0)
            {
                result = True;
            }
            else
            {
                ALOG_HEX(kALogLevelTrace, 50, (UInt8 *)md5_calc_value, sizeof(md5_calc_value));
                ALOG_HEX(kALogLevelTrace, 50, (UInt8 *)md5_value, sizeof(md5_calc_value));

            }
        }

        TFfStreamSetPosition(self->ff_stream_, pos);
    }

    return result;
}

Bool TImgUpgradeFileCheckHeader(TImgUpgradeFile *self, UInt32 *hw_version)
{
    Bool result = False;
    Int32 pos = TFfStreamPosition(self->ff_stream_);
    
    ALOG_T("%s\n", __FUNCTION__);

    TFfStreamSetPosition(self->ff_stream_, 0);

    Int32 read_size = TFfStreamRead(self->ff_stream_, &self->img_header_, sizeof(self->img_header_));

    if (read_size != sizeof(self->img_header_))
        ALOG_E("%s, read_size=%d, head_size=%d", __FUNCTION__, read_size, sizeof(self->img_header_));
    else
    {
        //ALOG_HEX(kALogLevelTrace, 50, (UInt8 *)&self->img_header_, sizeof(self->img_header_));

        result = ImgUpgradeHeaderCheck(&self->img_header_, hw_version);
    }
    ALOG_T("%s, check_result=%d", __FUNCTION__, result);

    TFfStreamSetPosition(self->ff_stream_, pos);
    
    return result;
}

static Bool TImgUpgradeFileCheckSize(TImgUpgradeFile *self)
{
    Bool result = False;

    if (self->img_header_.fwr_size + sizeof(self->img_header_) == self->file_size_)
        result = True;

    if (result)
    {
        ALOG_T("%s, result=%d, fwr_size=%d, header_size=%d, file_size_=%d", __FUNCTION__, 
            result, self->img_header_.fwr_size, sizeof(self->img_header_), self->file_size_);
    }
    else
    {
        ALOG_E("%s, result=%d, fwr_size=%d, header_size=%d, file_size_=%d", __FUNCTION__, 
            result, self->img_header_.fwr_size, sizeof(self->img_header_), self->file_size_);
    }

    return result;
}
/*static Int32 PrinfLineData(UInt8 *data, Int32 data_size)
{
    const Int32 kLineSize = 16;
    
    if (data_size > kLineSize)
        data_size = kLineSize;

    for (int i = 0; i < kLineSize; i++)
    {
        AssistShellLogOut("%02x ", data[i]);
    }

    AssistShellLogOut("\r\n");

    return data_size;
}*/

static Int32 TImgUpgradeFileCalcItemCrc(TImgUpgradeFile *self, Int32 index, UInt16 *crc_value)
{
    TImageItemHeader *item_header = &self->img_header_.fw_items[index];
    Int32 pos = TFfStreamPosition(self->ff_stream_);
    Int32 total_size = 0;
    Int32 result = -1;
    Int32 signel_size;
    UInt16 crc_seed = 0xFFFF;

    ALOG_T("begin %s, index=%d, offset=%d, size=%d", __FUNCTION__, index, item_header->offset, item_header->size);

    TFfStreamSetPosition(self->ff_stream_, item_header->offset + sizeof(TImageHeader));

    while (total_size < item_header->size)
    {
        signel_size = item_header->size - total_size;

        if (signel_size > self->buffer_size)
            signel_size = self->buffer_size;

        Int32 read_size = TFfStreamRead(self->ff_stream_, self->buffer, signel_size);

        if (read_size > 0)
        {
            //ALOG_HEX(kALogLevelTrace, 50, self->buffer, signel_size);
            /*{
                Int32 print_read_size = read_size;
                UInt8 *buf = (UInt8 *)self->buffer;
                Int32 printf_pos = 0;
                Int32 printf_size = PrinfLineData(&buf[printf_pos], print_read_size);
                
                print_read_size -= printf_size;

                while (print_read_size > 0)
                {
                    printf_pos += printf_size;
                    printf_size = PrinfLineData(&buf[printf_pos], print_read_size);
                    print_read_size -= printf_size;
                }
            }*/
            
            total_size += read_size;
            crc_seed = OfCalcCrc16s(self->buffer, read_size, crc_seed);
        }
        else
        {
            ALOG_E("%s, read_size=%d", __FUNCTION__, read_size);
            result = -1;
            break;
        }
    }

    if (total_size >= item_header->size)
        result = 0;

    if (crc_value != NULL)
        *crc_value = crc_seed;

    TFfStreamSetPosition(self->ff_stream_, pos);
    ALOG_T("end %s, result=%d, total_size=%d", __FUNCTION__, result, total_size);

    return result;
}

static UInt16 ntohs(UInt16 n)
{
    return (n >> 8) | (n << 8);
}

Bool TImgUpgradeFileIsOpen(TImgUpgradeFile *self)
{
    return self->is_open_;
}

Bool TImgUpgradeFileOpen(TImgUpgradeFile *self, UInt32 *hw_version)
{
    Bool result = False;
    ALOG_T("begin %s", __FUNCTION__);

    do
    {
        if (!TImgUpgradeFileCheckHeader(self, hw_version))
        {
            ALOG_E("Img Check Header Fail");

            result = False;
            break;
        }
        ALOG_T("Img Check Header Ok");
        if (!TImgUpgradeFileCheckSize(self))
        {
            ALOG_E("Img Check Size Fail, item_cnt=%d", self->img_header_.item_cnt);
            result = False;
            break;            
        }
        ALOG_T("Img Check Size Ok, item_cnt=%d", self->img_header_.item_cnt);
        
        for (int index = 0; index < self->img_header_.item_cnt; index++)
        {
            UInt16 crc_value = 0;

            if (TImgUpgradeFileCalcItemCrc(self, index, &crc_value) == 0
                && (ntohs(crc_value) == self->img_header_.fw_items[index].crc))
            {
                self->is_open_ = True;
                result = True;
                ALOG_T("%s, result=%d, crc_value=%d, file_crc=%d", __FUNCTION__, result, crc_value, self->img_header_.fw_items[index].crc);

            }
            else
            {
                self->is_open_ = False;
                result = False;
                ALOG_E("%s, crc_value=%d, file_crc=%d", __FUNCTION__, crc_value, self->img_header_.fw_items[index].crc);
                break;
            }
        }
    } while (0);

    ALOG_T("end %s, result=%d", __FUNCTION__, result);


    return result;
}

void TImgUpgradeFileClose(TImgUpgradeFile *self)
{
    self->is_open_ = False;
}

Int32 TImgUpgradeFileItemCount(TImgUpgradeFile *self)
{
    return self->is_open_ ? self->img_header_.item_cnt : 0;
}

TImageItemHeader *TImgUpgradeFileItems(TImgUpgradeFile *self, Int32 index)
{
    if (self->is_open_)
        return (index >= 0 && index < self->img_header_.item_cnt) ? &self->img_header_.fw_items[index] : NULL;
    else
        return NULL;
}

Int32 TImgUpgradeFileDstDevIndex(TImgUpgradeFile *self, UInt8 dst_dev)
{
    Int32 result = -1;

    if (self->is_open_)
    {
        for (int i = TImgUpgradeFileItemCount(self) - 1; i >= 0; i--)
        {
            TImageItemHeader *item = TImgUpgradeFileItems(self, i);

            if (item->obj_dev == dst_dev)
            {
                result = i;
                break;
            }
        }
    }

    return result;
}

Int32 TImgUpgradeFileReadItem(TImgUpgradeFile *self, Int32 index, Int32 pos, UInt8 *buf, Int32 buf_size)
{
    Int32 result = -1;

    if (self->is_open_ && index >= 0 && pos >= 0 && buf != NULL && buf_size > 0)
    {
        Int32 position = TFfStreamPosition(self->ff_stream_);
        
        // ALOG_T("end %s, index=%d, pos=%d, buf_size=%d, size=%d\n", __FUNCTION__, 
        //     index, pos, buf_size, self->img_header_.fw_items[index].size);

        if (index < self->img_header_.item_cnt
            && pos < self->img_header_.fw_items[index].size)
        {
            TFfStreamSetPosition(self->ff_stream_, sizeof(TImageHeader) + self->img_header_.fw_items[index].offset + pos);

            if (buf_size > self->img_header_.fw_items[index].size - pos)
                buf_size = self->img_header_.fw_items[index].size - pos;

            result = TFfStreamRead(self->ff_stream_, buf, buf_size);
        }

        TFfStreamSetPosition(self->ff_stream_, position);
    }
    
    // ALOG_T("end %s, result=%d, pos=%d, size=%d\n", __FUNCTION__, result, pos, buf_size);

    return result;
}

Int32 TImgUpgradeFileItemSize(TImgUpgradeFile *self, Int32 index)
{
    if (self->is_open_)
    {
        return (index >= 0 && index < self->img_header_.item_cnt) ? 
            self->img_header_.fw_items[index].size : 0;
    }
    else
        return 0;
}


