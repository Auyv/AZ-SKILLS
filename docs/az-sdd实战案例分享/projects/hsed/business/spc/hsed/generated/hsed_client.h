/**
 * @file hsed_client.h
 * @brief
 * @note
 * @author spc_builder
 * @date 2026-07-27 21:27:08
 * @version 1.0

 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _HSED_CLIENT_H_
#define _HSED_CLIENT_H_

#include <ofc_types.h>
#include <SpanCall.h>
#include "hsed_datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux
#pragma GCC visibility push(default)
#endif

/**
 * 客户端初始化
 * @note: 其它客户端函数调用前,必须先调用本函数来设置连接id
 * @return 初始化结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsed_Init(void);

/**
 * 客户端去初始化操作
 * @return
 */
void HcHsed_Done(void);

/**
 * 打开连接
 * @return 打开的结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsed_Open(void);

/**
 * 关闭连接
 * @return
 */
void HcHsed_Close(void);

/**
 * 返回连接对象,可用于连接相关的操作
 * @note: 仅在初始化后有效
 * @return 连接对象
 */
void *HcHsed_Conn(void);

/**
 * 获取连接状态
 * @return 返回连接状态,参见 SC_TConnState
 */
int HcHsed_ConnState(void);

/**
 * 创建HSED对象并一次性装载全部46项属性默认值。KEY_ID固定使用CIU98槽位1~14：AES128=1、TDES128=2、SM1=3、SM4=4、RSA1024=5/6、RSA2048=7/8、ECC256=9/10、SM2=11/12、SM9=13/14；对象共享这些槽位引用。
 * THcHsedHsedResult HseCreate([OUT] UInt64 handle);
 * @param _ret_: 业务返回码；OK表示对象创建成功，其余值说明失败原因
 * @param _handle: 新对象句柄；仅属于当前SPC连接，不允许跨进程或跨连接共享
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsed_HseCreate(int *_ret_, TInt64 *_handle);

/**
 * 销毁HSED对象并释放其内存属性；对象正在执行硬件操作时返回BUSY
 * THcHsedHsedResult HseDestroy([IN] UInt64 handle);
 * @param handle: HseCreate返回的对象句柄；销毁后不得继续使用
 * @param _ret_: 业务返回码；OK表示对象已销毁
 * @return 操作结果
 *     SC_rSuccess: 操作成功
 *     <= 0: 操作失败的返回码, 参见 SC_TResult 定义
 */
int HcHsed_HseDestroy(TInt64 handle, int *_ret_);

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
int HcHsed_SetPropertyInt(TInt64 handle, int property_id, int value, int *_ret_);

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
int HcHsed_SetPropertyBytes(TInt64 handle, int property_id, unsigned char *value, int count_value, int *_ret_);

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
int HcHsed_GetPropertyInt(TInt64 handle, int property_id, int *_ret_, int *_value);

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
int HcHsed_GetPropertyBytes(TInt64 handle, int property_id, int *_ret_, unsigned char *_value, int *_count_value);

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
int HcHsed_Encrypt(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_output_data, int *_count_output_data);

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
int HcHsed_Decrypt(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_output_data, int *_count_output_data);

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
int HcHsed_Sign(TInt64 handle, int algorithm, unsigned char *input_data, int count_input_data, int *_ret_, unsigned char *_signature, int *_count_signature, unsigned int *_r_size, unsigned int *_s_size);

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
int HcHsed_ExportPublicKey(TInt64 handle, int algorithm, int *_ret_, unsigned char *_public_key, int *_count_public_key);

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
int HcHsed_ImportPublicKey(TInt64 handle, int algorithm, unsigned char *public_key, int count_public_key, Bool replace_existing_key, int *_ret_);

#ifdef __linux
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // _HSED_CLIENT_H_
