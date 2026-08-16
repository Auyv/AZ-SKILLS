#include "hsed_srv.h"
#include "hsed_datatype.h"
#include "hsed_spc_adapter.h"

#define HSED_SPC_MAX_OUTPUT_DATA_LEN \
    (BSC_HSE_MAX_INPUT_LEN + BSC_HSE_MAX_SIGNATURE_LEN)

/**
 * 创建HSED对象并一次性装载全部46项属性默认值。KEY_ID固定使用CIU98槽位1~14：AES128=1、TDES128=2、SM1=3、SM4=4、RSA1024=5/6、RSA2048=7/8、ECC256=9/10、SM2=11/12、SM9=13/14；对象共享这些槽位引用。
 * THcHsedHsedResult HseCreate([OUT] UInt64 handle);
 * @param _ret_: 业务返回码；OK表示对象创建成功，其余值说明失败原因
 * @param _handle: 新对象句柄；仅属于当前SPC连接，不允许跨进程或跨连接共享
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_HseCreate(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// fields:
    TInt64 handle = 0;
    TbscHseHandle bsc_handle = 0;

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterCreate(conn_id, &bsc_handle);
    handle = (TInt64)bsc_handle;

    // ret-fields
    if (result == kBscHseResultOk)
    {
            SCRespFieldByInt64(0, handle);
    }

    // return
    SCRespRetByInt(result);
}

static void __stdcall L_Cancel_HseCreate(void *owner, int app_id, void *func_id, void *conn_id)
{
// fields:
    TInt64 handle = SCRespFieldAsInt64(0, NULL);

// return:
    int result = SCRespRetAsInt(NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((result == kBscHseResultOk) && (handle > 0))
    {
        (void)HsedSpcAdapterDestroy(conn_id, (TbscHseHandle)handle);
    }
}

static void __stdcall L_Confirm_HseCreate(void *owner, int app_id, void *func_id, void *conn_id)
{
    (void)owner;
    (void)app_id;
    (void)func_id;
    (void)conn_id;
}

static Bool L_Def_HseCreate(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "HseCreate", SC_ftNeedConfirm, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_HseCreate);
        SCFuncSetObj(func_id, SC_faOnRespCancel, (void *)&L_Cancel_HseCreate);
        SCFuncSetObj(func_id, SC_faOnRespConfirm, (void *)&L_Confirm_HseCreate);

        // fields:
        SCFuncAddField(func_id, "handle", SC_tInt64, 0);

        result = True;
   }

   return result;
}

/**
 * 销毁HSED对象并释放其内存属性；对象正在执行硬件操作时返回BUSY
 * THcHsedHsedResult HseDestroy([IN] UInt64 handle);
 * @param handle: HseCreate返回的对象句柄；销毁后不得继续使用
 * @param _ret_: 业务返回码；OK表示对象已销毁
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_HseDestroy(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterDestroy(conn_id, (TbscHseHandle)handle);

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_HseDestroy(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "HseDestroy", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_HseDestroy);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);

        result = True;
   }

   return result;
}

/**
 * 设置可写Int属性。所有KEY_ID、PUBLIC_KEY_ID、PRIVATE_KEY_ID均为固定只读属性，任何修改请求返回PERMISSION_DENIED；其他属性按对应枚举范围校验。
 * THcHsedHsedResult SetPropertyInt([IN] UInt64 handle, [IN] THcHsedHsedPropertyId property_id, [IN] Int32 value);
 * @param handle: HseCreate返回的对象句柄
 * @param property_id: 完整属性ID；KEY_ID、PUBLIC_KEY_ID、PRIVATE_KEY_ID只读，不能作为可修改目标。
 * @param value: 标准4字节有符号整数；仅用于可写的模式、填充、摘要、签名输入模式、公钥格式等属性。
 * @param _ret_: 业务返回码；KEY_ID类属性固定返回PERMISSION_DENIED，其他可写Int属性成功时返回OK。
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_SetPropertyInt(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int property_id = SCRespParamAsInt(1, NULL);
    int value = SCRespParamAsInt(2, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterSetPropertyInt(
        conn_id,
        (TbscHseHandle)handle,
        (TbscHsePropertyId)property_id,
        (Int32)value);

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_SetPropertyInt(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "SetPropertyInt", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_SetPropertyInt);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "property_id", SC_tInt32, 0);
        SCFuncAddParam(func_id, "value", SC_tInt32, 0);

        result = True;
   }

   return result;
}

/**
 * 设置字节数组属性：USER_ID保存到当前功能对象内存，可按业务需要更新且不访问HSE硬件；CHIP_ID首次写入HSE硬件后不可修改；算法IV保存在对象内存；数组长度由SPC动态参数携带
 * THcHsedHsedResult SetPropertyBytes([IN] UInt64 handle, [IN] THcHsedHsedPropertyId property_id, [IN] UInt8 value[]);
 * @param handle: HseCreate返回的对象句柄
 * @param property_id: 完整属性ID枚举；USER_ID为对象内存业务标记；CHIP_ID为一次性HSE硬件配置；算法IV使用对应的AES128_IV、SM4_IV等HsedPropertyId成员
 * @param value: 不定长二进制数组，可包含0x00；有效长度由SPC自动生成的count_value携带；USER_ID长度为1~128字节且仅保存在对象内存；SM2签名要求至少16字节，并仅使用前16字节参与ZA计算；CHIP_ID为1~128字节，首次写入硬件，后续写入返回KEY_OCCUPIED；IV按算法要求为8或16字节
 * @param count_value: value的元素个数
 * @param _ret_: 业务返回码；OK表示不定长二进制属性已保存并将在后续调用中复用
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_SetPropertyBytes(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int property_id = SCRespParamAsInt(1, NULL);
    int size_value = 0;
    UInt8 *value = (UInt8 *)SCRespParamAsStr(2, &size_value);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((value != NULL) && (size_value > 0))
    {
        result = HsedSpcAdapterSetPropertyBytes(
            conn_id,
            (TbscHseHandle)handle,
            (TbscHsePropertyId)property_id,
            value,
            (UInt32)size_value);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_SetPropertyBytes(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "SetPropertyBytes", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_SetPropertyBytes);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "property_id", SC_tInt32, 0);
        SCFuncAddParam(func_id, "value", SC_tString, 0);

        result = True;
   }

   return result;
}

/**
 * 读取对象当前Int属性。未修改属性返回HseCreate装载的确定默认值；KEY_ID固定返回槽位1~14中的对应值。
 * THcHsedHsedResult GetPropertyInt([IN] UInt64 handle, [IN] THcHsedHsedPropertyId property_id, [OUT] Int32 value);
 * @param handle: HseCreate返回的对象句柄
 * @param property_id: 完整属性ID枚举；必须与SetPropertyInt设置时使用的HsedPropertyId成员一致
 * @param _ret_: 业务返回码；OK表示成功返回4字节有符号整数属性
 * @param _value: 当前整数属性值；标准4字节有符号整数，HASH_ALGORITHM返回统一HsedAlgorithm标识
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_GetPropertyInt(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result;

// fields:
    int value = 0;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int property_id = SCRespParamAsInt(1, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterGetPropertyInt(
        conn_id,
        (TbscHseHandle)handle,
        (TbscHsePropertyId)property_id,
        (Int32 *)&value);

    // ret-fields
    if (result == kBscHseResultOk)
    {
            SCRespFieldByInt(0, value);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_GetPropertyInt(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "GetPropertyInt", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_GetPropertyInt);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "property_id", SC_tInt32, 0);

        // fields:
        SCFuncAddField(func_id, "value", SC_tInt32, 0);

        result = True;
   }

   return result;
}

/**
 * 读取字节数组属性：USER_ID和算法IV从当前功能对象内存读取；CHIP_ID从HSE硬件读取；数组容量和实际长度由SPC动态参数携带
 * THcHsedHsedResult GetPropertyBytes([IN] UInt64 handle, [IN] THcHsedHsedPropertyId property_id, [OUT] UInt8 value[]);
 * @param handle: HseCreate返回的对象句柄
 * @param property_id: 完整属性ID枚举；USER_ID读取对象内存业务标记；CHIP_ID读取HSE硬件一次性配置值；算法IV必须使用与SetPropertyBytes一致的HsedPropertyId成员
 * @param _ret_: 业务返回码；OK表示成功返回不定长二进制属性
 * @param _value: 返回的不定长二进制数组；调用方提供缓冲区，count_value调用前表示容量、调用后表示实际长度；USER_ID长度为1~128字节；SM2签名要求至少16字节，并仅使用前16字节参与ZA计算；CHIP_ID为已写入硬件的1~128字节配置值；IV为8或16字节
 * @param _count_value: 传入时为value能存放的元素个数,返回时为复制到value中的元素个数
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_GetPropertyBytes(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result;

// fields:
    UInt32 count_value = BSC_HSE_MAX_CHIP_ID_LEN;
    UInt8 value[BSC_HSE_MAX_CHIP_ID_LEN] = {0};

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int property_id = SCRespParamAsInt(1, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterGetPropertyBytes(
        conn_id,
        (TbscHseHandle)handle,
        (TbscHsePropertyId)property_id,
        value,
        &count_value);

    // ret-fields
    if (result == kBscHseResultOk)
    {
        SCRespFieldByStr(0, (const char *)value, (int)count_value);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_GetPropertyBytes(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "GetPropertyBytes", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_GetPropertyBytes);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "property_id", SC_tInt32, 0);

        // fields:
        SCFuncAddField(func_id, "value", SC_tString, 0);

        result = True;
   }

   return result;
}

/**
 * 使用algorithm参数指定算法：对称算法读取对应KEY_ID，非对称算法读取对应PUBLIC_KEY_ID；同时读取该算法的模式、补位和IV等属性，一次性加密完整二进制数据
 * THcHsedHsedResult Encrypt([IN] UInt64 handle, [IN] THcHsedHsedAlgorithm algorithm, [IN] UInt8 input_data[], [OUT] UInt8 output_data[]);
 * @param handle: HseCreate返回的对象句柄；对象已初始化全部算法的设备默认KID引用
 * @param algorithm: 本次加密使用的16位HsedAlgorithm；对称算法读取对应KEY_ID，非对称算法读取对应PUBLIC_KEY_ID，并校验槽位算法及公钥角色一致
 * @param input_data: 完整明文二进制数组，可包含0x00；由SPC动态数组一次传输，无需HSED分块，服务仍在调用芯片前校验生成代码、可用内存、算法和CIU98 SDK实际长度限制
 * @param count_input_data: input_data的元素个数
 * @param _ret_: 业务返回码；OK表示一次完整加密已经完成
 * @param _output_data: 完整密文二进制数组；SPC自动生成的count_output_data调用前表示缓冲区容量、调用后表示实际长度；长度可能因填充或算法封装而不同于输入
 * @param _count_output_data: 传入时为output_data能存放的元素个数,返回时为复制到output_data中的元素个数
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_Encrypt(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// fields:
    UInt32 count_output_data = HSED_SPC_MAX_OUTPUT_DATA_LEN;
    UInt8 output_data[HSED_SPC_MAX_OUTPUT_DATA_LEN] = {0};

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int algorithm = SCRespParamAsInt(1, NULL);
    int size_input_data = 0;
    UInt8 *input_data = (UInt8 *)SCRespParamAsStr(2, &size_input_data);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((input_data != NULL) && (size_input_data > 0) &&
        (size_input_data <= (int)BSC_HSE_MAX_INPUT_LEN))
    {
        result = HsedSpcAdapterEncrypt(
            conn_id,
            (TbscHseHandle)handle,
            (TbscHseAlgorithm)algorithm,
            input_data,
            (UInt32)size_input_data,
            output_data,
            &count_output_data);
    }

    // ret-fields
    if (result == kBscHseResultOk)
    {
        SCRespFieldByStr(0, (const char *)output_data,
                         (int)count_output_data);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_Encrypt(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "Encrypt", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_Encrypt);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "algorithm", SC_tInt32, 0);
        SCFuncAddParam(func_id, "input_data", SC_tString, 0);

        // fields:
        SCFuncAddField(func_id, "output_data", SC_tString, 0);

        result = True;
   }

   return result;
}

/**
 * 使用algorithm参数指定算法：对称算法读取对应KEY_ID，非对称算法读取对应PRIVATE_KEY_ID；同时读取该算法的模式、补位和IV等属性，一次性解密完整二进制数据
 * THcHsedHsedResult Decrypt([IN] UInt64 handle, [IN] THcHsedHsedAlgorithm algorithm, [IN] UInt8 input_data[], [OUT] UInt8 output_data[]);
 * @param handle: HseCreate返回的对象句柄；对象已初始化全部算法的设备默认KID引用
 * @param algorithm: 本次解密使用的16位HsedAlgorithm；对称算法读取对应KEY_ID，非对称算法读取对应PRIVATE_KEY_ID，并校验槽位算法及私钥角色一致
 * @param input_data: 完整密文二进制数组
 * @param count_input_data: input_data的元素个数
 * @param _ret_: 业务返回码；OK表示一次完整解密已经完成
 * @param _output_data: 完整明文二进制数组；SPC自动生成的count_output_data调用前表示缓冲区容量、调用后表示实际长度；长度可能因去填充而不同于输入
 * @param _count_output_data: 传入时为output_data能存放的元素个数,返回时为复制到output_data中的元素个数
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_Decrypt(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// fields:
    UInt32 count_output_data = HSED_SPC_MAX_OUTPUT_DATA_LEN;
    UInt8 output_data[HSED_SPC_MAX_OUTPUT_DATA_LEN] = {0};

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int algorithm = SCRespParamAsInt(1, NULL);
    int size_input_data = 0;
    UInt8 *input_data = (UInt8 *)SCRespParamAsStr(2, &size_input_data);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((input_data != NULL) && (size_input_data > 0) &&
        (size_input_data <= (int)BSC_HSE_MAX_INPUT_LEN))
    {
        result = HsedSpcAdapterDecrypt(
            conn_id,
            (TbscHseHandle)handle,
            (TbscHseAlgorithm)algorithm,
            input_data,
            (UInt32)size_input_data,
            output_data,
            &count_output_data);
    }

    // ret-fields
    if (result == kBscHseResultOk)
    {
        SCRespFieldByStr(0, (const char *)output_data,
                         (int)count_output_data);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_Decrypt(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "Decrypt", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_Decrypt);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "algorithm", SC_tInt32, 0);
        SCFuncAddParam(func_id, "input_data", SC_tString, 0);

        // fields:
        SCFuncAddField(func_id, "output_data", SC_tString, 0);

        result = True;
   }

   return result;
}

/**
 * 使用algorithm参数指定算法，读取对应PRIVATE_KEY_ID、HASH_ALGORITHM和SIGN_INPUT_MODE属性组，一次性签名完整原文或摘要；禁止使用公钥槽位签名
 * THcHsedHsedResult Sign([IN] UInt64 handle, [IN] THcHsedHsedAlgorithm algorithm, [IN] UInt8 input_data[], [OUT] UInt8 signature[], [OUT] UInt32 r_size, [OUT] UInt32 s_size);
 * @param handle: HseCreate返回的对象句柄；对象已初始化全部非对称算法的设备默认私钥KID引用
 * @param algorithm: 本次签名使用的16位HsedAlgorithm；仅允许RSA1024、RSA2048、ECC256、SM2或SM9；服务读取对应PRIVATE_KEY_ID并校验槽位算法及私钥角色一致
 * @param input_data: 完整原文二进制数组
 * @param count_input_data: input_data的元素个数
 * @param _ret_: 业务返回码；OK表示签名已经完成
 * @param _signature: 签名结果二进制数组；ECC256/SM2按r||s顺序返回；SPC自动生成的count_signature调用前表示缓冲区容量、调用后表示实际长度
 * @param _count_signature: 传入时为signature能存放的元素个数,返回时为复制到signature中的元素个数
 * @param _r_size: ECC256/SM2签名中r分量的字节数，r位于signature起始位置；RSA、SM9等非(r,s)格式返回0
 * @param _s_size: ECC256/SM2签名中s分量的字节数，s紧随r之后且r_size+s_size等于count_signature；RSA、SM9等非(r,s)格式返回0
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_Sign(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// fields:
    UInt32 count_signature = BSC_HSE_MAX_SIGNATURE_LEN;
    UInt8 signature[BSC_HSE_MAX_SIGNATURE_LEN] = {0};
    UInt32 r_size = 0;
    UInt32 s_size = 0;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int algorithm = SCRespParamAsInt(1, NULL);
    int size_input_data = 0;
    UInt8 *input_data = (UInt8 *)SCRespParamAsStr(2, &size_input_data);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((input_data != NULL) && (size_input_data > 0) &&
        (size_input_data <= (int)BSC_HSE_MAX_INPUT_LEN))
    {
        result = HsedSpcAdapterSign(
            conn_id,
            (TbscHseHandle)handle,
            (TbscHseAlgorithm)algorithm,
            input_data,
            (UInt32)size_input_data,
            signature,
            &count_signature,
            &r_size,
            &s_size);
    }

    // ret-fields
    if (result == kBscHseResultOk)
    {
        SCRespFieldByStr(0, (const char *)signature, (int)count_signature);
        SCRespFieldByInt(1, (int)r_size);
        SCRespFieldByInt(2, (int)s_size);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_Sign(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "Sign", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_Sign);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "algorithm", SC_tInt32, 0);
        SCFuncAddParam(func_id, "input_data", SC_tString, 0);

        // fields:
        SCFuncAddField(func_id, "signature", SC_tString, 0);
        SCFuncAddField(func_id, "r_size", SC_tInt32, 0);
        SCFuncAddField(func_id, "s_size", SC_tInt32, 0);

        result = True;
   }

   return result;
}

/**
 * 使用algorithm参数指定非对称算法并从对应PUBLIC_KEY_ID导出公钥；首版默认返回BIN二进制数据；不提供私钥导出
 * THcHsedHsedResult ExportPublicKey([IN] UInt64 handle, [IN] THcHsedHsedAlgorithm algorithm, [OUT] UInt8 public_key[]);
 * @param handle: HseCreate返回的对象句柄；对象已初始化全部非对称算法的设备默认公钥KID引用；PUBLIC_KEY_FORMAT可选，未设置时默认BIN
 * @param algorithm: 待导出公钥的16位HsedAlgorithm；服务读取对应PUBLIC_KEY_ID并校验槽位算法及公钥角色一致
 * @param _ret_: 业务返回码；OK表示公钥已导出
 * @param _public_key: 导出的公钥数据；首版为算法相关BIN原始二进制字节；SPC自动生成的count_public_key调用前表示容量、调用后表示实际长度；私钥永不导出
 * @param _count_public_key: 传入时为public_key能存放的元素个数,返回时为复制到public_key中的元素个数
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_ExportPublicKey(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result;

// fields:
    UInt32 count_public_key = BSC_HSE_MAX_PUBLIC_KEY_LEN;
    UInt8 public_key[BSC_HSE_MAX_PUBLIC_KEY_LEN] = {0};

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int algorithm = SCRespParamAsInt(1, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    result = HsedSpcAdapterExportPublicKey(
        conn_id,
        (TbscHseHandle)handle,
        (TbscHseAlgorithm)algorithm,
        public_key,
        &count_public_key);

    // ret-fields
    if (result == kBscHseResultOk)
    {
        SCRespFieldByStr(0, (const char *)public_key,
                         (int)count_public_key);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_ExportPublicKey(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "ExportPublicKey", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_ExportPublicKey);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "algorithm", SC_tInt32, 0);

        // fields:
        SCFuncAddField(func_id, "public_key", SC_tString, 0);

        result = True;
   }

   return result;
}

/**
 * 接口签名为兼容保留。当前固定密钥策略禁止客户端导入、更新或覆盖任何密钥对；合法句柄调用返回PERMISSION_DENIED，不访问CIU98。
 * THcHsedHsedResult ImportPublicKey([IN] UInt64 handle, [IN] THcHsedHsedAlgorithm algorithm, [IN] UInt8 public_key[], [IN] Bool replace_existing_key);
 * @param handle: HseCreate返回的对象句柄；对象仅引用固定设备密钥槽位。
 * @param algorithm: 待导入公钥的算法标识；当前策略下不会执行导入。
 * @param public_key: 待导入公钥数据；当前策略下不会写入芯片。
 * @param count_public_key: public_key的元素个数
 * @param replace_existing_key: 兼容保留参数；无论取值如何，当前策略均禁止覆盖固定槽位。
 * @param _ret_: 业务返回码；当前固定密钥策略返回PERMISSION_DENIED，不写入或覆盖目标槽位。
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */

static void __stdcall L_Exec_ImportPublicKey(void *owner, int app_id, void *func_id, void *conn_id)
{
// return:
    int result = kBscHseResultInvalidParam;

// params:
    TInt64 handle = SCRespParamAsInt64(0, NULL);
    int algorithm = SCRespParamAsInt(1, NULL);
    int size_public_key = 0;
    UInt8 *public_key = (UInt8 *)SCRespParamAsStr(2, &size_public_key);
    Bool replace_existing_key = SCRespParamAsBool(3, NULL);

    (void)owner;
    (void)app_id;
    (void)func_id;
    if ((public_key != NULL) && (size_public_key > 0) &&
        (size_public_key <= (int)BSC_HSE_MAX_PUBLIC_KEY_LEN))
    {
        result = HsedSpcAdapterImportPublicKey(
            conn_id,
            (TbscHseHandle)handle,
            (TbscHseAlgorithm)algorithm,
            public_key,
            (UInt32)size_public_key,
            replace_existing_key);
    }

    // return
    SCRespRetByInt(result);
}

static Bool L_Def_ImportPublicKey(void *owner, int app_id)
{
    Bool result = False;
    void *func_id = SCFuncCreate(app_id, "ImportPublicKey", SC_ftNormal, SC_tInt32, 0);
    
    if (func_id != NULL)
    {
        SCFuncSetObj(func_id, SC_faOwner, owner);
        SCFuncSetObj(func_id, SC_faOnExecResp, (void *)&L_Exec_ImportPublicKey);

        // params:
        SCFuncAddParam(func_id, "handle", SC_tInt64, 0);
        SCFuncAddParam(func_id, "algorithm", SC_tInt32, 0);
        SCFuncAddParam(func_id, "public_key", SC_tString, 0);
        SCFuncAddParam(func_id, "replace_existing_key", SC_tBool, 0);

        result = True;
   }

   return result;
}

//---------------------------------------------------------------------

// clear all functions
static void L_ClearAllFuncs(int app_id)
{
    void *func_id;
    int i;

    for (i = SCAppGetInt(app_id, SC_aaFuncCount, NULL) - 1; i >= 0; i--)
    {
        func_id = SCAppGetObj(app_id, SC_aaBase_FuncID + i, NULL);
        if (func_id != NULL)
           SCFuncFree(func_id);
    }
}

int HcHsedSrv_LoadDefs(void *owner, int app_id)
{
    int result = SC_rIsIllegal;

    if (app_id > 0)
    {
        SCAppClose(app_id);

        result = SCAppFuncBegin(app_id);
        if (result == SC_rSuccess)
        {
            L_ClearAllFuncs(app_id);

            // add function-defs
            L_Def_HseCreate(owner, app_id);
            L_Def_HseDestroy(owner, app_id);
            L_Def_SetPropertyInt(owner, app_id);
            L_Def_SetPropertyBytes(owner, app_id);
            L_Def_GetPropertyInt(owner, app_id);
            L_Def_GetPropertyBytes(owner, app_id);
            L_Def_Encrypt(owner, app_id);
            L_Def_Decrypt(owner, app_id);
            L_Def_Sign(owner, app_id);
            L_Def_ExportPublicKey(owner, app_id);
            L_Def_ImportPublicKey(owner, app_id);

            result = SCAppFuncEnd(app_id);
        }
    }

    return result;
}
