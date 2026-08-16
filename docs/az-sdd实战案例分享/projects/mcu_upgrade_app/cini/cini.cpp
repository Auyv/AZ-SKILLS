/**
 * @file        cini.c
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
 
#include "cini.h"
#include "rfc/rf_ini_files.h"

// struct TCIniTag
// {
//     rfc::TMemIniFile *ini_handle;
// };



Int32 TCIniCreate(TCIni *self, const char *filename)
{
    self->ini_handle = new rfc::TMemIniFile(filename);
    return self->ini_handle != nullptr ? 0 : -1;
}

void  TCIniDestroy(TCIni *self)
{
    if (self != NULL && self->ini_handle)
    {
        delete (rfc::TMemIniFile *)self->ini_handle;
        self->ini_handle = nullptr;
    }
}

Int32 TCiniClear(TCIni *self)
{
    rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
    if (ini_handle != nullptr)
    {
        ini_handle->Clear();
    }
    return 0;
}

Int32 TCIniUpdateToFile(TCIni *self)
{
    rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
    if (ini_handle != nullptr)
    {
        ini_handle->UpdateFile();
    }
    return 0;   
}


Int32 TCIniGetSectionCount(TCIni *self)
{
    Int32 result = -1;
    if (self != NULL)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            rfc::TStringList section_strings;
            ini_handle->ReadSections(section_strings);
            Int32 count = section_strings.Count();
            section_strings.Clear(); // 确保释放内部资源
            result = count; 
        }
    }
    return result;
}

Int32 TCIniGetSectionName(TCIni *self, Int32 index, char *sectionName, Int32 sectionNameLen)
{
    Int32 result = -1;
    if (self != NULL && index >= 0 && sectionName != NULL && sectionNameLen > 1)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            rfc::TStringList section_strings;
            ini_handle->ReadSections(section_strings);

            if (index < section_strings.Count())
            {
                rfc::TString section_name = section_strings.Get(index);
                Int32 copy_len = section_name.Length() < sectionNameLen - 1 ? section_name.Length() : sectionNameLen - 1;
                char *section_name_c = section_name.c_str();
                strncpy(sectionName, section_name_c, copy_len);
                sectionName[copy_len] = '\0';   
                result = copy_len;
            }   
            else
            {
                result = -2;
            }
            section_strings.Clear(); // 确保释放内部资源
        }
        else
        {
            result = -3; // 使用整数值而不是布尔值False
        }
    }
    return result;
}


Bool TCIniGetKeyValueStringEx(TCIni *self, const char *sectionName, const char *keyName, char *value, Int32 valueLen, const char *defaultValue)
{
    Bool result = False;    
    if (self != NULL && sectionName != NULL && keyName != NULL && value != NULL && valueLen > 0)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            rfc::TString val = ini_handle->ReadString(
                rfc::TString(sectionName),
                rfc::TString(keyName),  
                rfc::TString(defaultValue));

            // 先保存Trim结果到临时变量，避免使用悬空指针
            rfc::TString trimmedVal = Trim(val);
            const char *v = trimmedVal.c_str();
            
            // 检查v是否为null
            int len = 0;
            if (v == NULL) 
            {
                value[0] = '\0';
                result = True; // 空值但处理成功
            }
            else
            {
                len = strlen(v);
                if (len >= valueLen)
                    len = valueLen - 1;
                memcpy(value, v, len);
                value[len] = '\0';
                result = True;
            }
        }
        else
        {
            result = False;
        }
    }
    return result;
}   

Bool TCIniGetKeyValueString(TCIni *self, const char *sectionName, const char *keyName, char *value, Int32 valueLen)
{
    return TCIniGetKeyValueStringEx(self, sectionName, keyName, value, valueLen, "");
}


Bool TCIniGetKeyValueIntEx(TCIni *self, const char *sectionName, const char *keyName, Int32 *value, Int32 defaultValue)
{
    Bool result = False;
    if (self != NULL && sectionName != NULL && keyName != NULL && value != NULL)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            int val = ini_handle->ReadInteger(
                rfc::TString(sectionName),
                rfc::TString(keyName),
                defaultValue);
            *value = val;
            result = True;  
        }
        else
        {
            result = False;
        }
    }
    return result;
}

Bool TCIniGetKeyValueInt(TCIni *self, const char *sectionName, const char *keyName, Int32 *value)
{
    return TCIniGetKeyValueIntEx(self, sectionName, keyName, value, 0);
}



Bool TCIniSetKeyValueString(TCIni *self, const char *sectionName, const char *keyName, const char *value)
{
    Bool result = False;
    if (self != NULL && sectionName != NULL && keyName != NULL && value != NULL)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            ini_handle->WriteString(
                rfc::TString(sectionName),
                rfc::TString(keyName),      
                rfc::TString(value));
            result = True;  
        }
    }
    return result;
}   

Bool TCIniSetKeyValueInt(TCIni *self, const char *sectionName, const char *keyName, Int32 value)
{
    Bool result = False;
    if (self != NULL && sectionName != NULL && keyName != NULL)
    {
        rfc::TMemIniFile *ini_handle = (rfc::TMemIniFile *)self->ini_handle;
        if (ini_handle != nullptr)
        {
            ini_handle->WriteInteger(
                rfc::TString(sectionName),
                rfc::TString(keyName),
                value);
            result = True;  
        }
        else
        {
            result = False;
        }
    }
    return result;
}










