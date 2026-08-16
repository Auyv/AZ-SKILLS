/**
 * @file        hsed_spc_adapter_test.c
 * @brief       HSED SPC adapter ownership and forwarding tests
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "hsed_spc_adapter.h"

#include <stdio.h>
#include <string.h>

#define TEST_BYTES_SIZE 4

static TbscHseHandle g_next_handle_ = 1;
static Int32 g_destroy_count_ = 0;
static Int32 g_int_value_ = 0;
static UInt8 g_bytes_[TEST_BYTES_SIZE] = {0};
static UInt32 g_bytes_size_ = 0;

#define EXPECT_EQ(name, actual, expected)                                  \
    do                                                                     \
    {                                                                      \
        Int32 actual_value_ = (Int32)(actual);                              \
        Int32 expected_value_ = (Int32)(expected);                          \
        if (actual_value_ != expected_value_)                               \
        {                                                                  \
            (void)fprintf(stderr,                                           \
                          "[FAIL] %s actual=%d expected=%d\n",               \
                          name, actual_value_, expected_value_);             \
            failed++;                                                       \
        }                                                                  \
    } while (0)

Int32 BscHseCreate(TbscHseHandle *handle)
{
    if (handle == NULL)
    {
        return kBscHseResultInvalidParam;
    }
    *handle = g_next_handle_++;
    return kBscHseResultOk;
}

Int32 BscHseDestroy(TbscHseHandle handle)
{
    if (handle == 0)
    {
        return kBscHseResultInvalidParam;
    }
    g_destroy_count_++;
    return kBscHseResultOk;
}

Int32 BscHseSetPropertyInt(TbscHseHandle handle,
                           TbscHsePropertyId property_id,
                           Int32 value)
{
    (void)handle;
    (void)property_id;
    g_int_value_ = value;
    return kBscHseResultOk;
}

Int32 BscHseSetPropertyBytes(TbscHseHandle handle,
                             TbscHsePropertyId property_id,
                             const UInt8 *value,
                             UInt32 value_size)
{
    (void)handle;
    (void)property_id;
    if ((value == NULL) || (value_size != TEST_BYTES_SIZE))
    {
        return kBscHseResultInvalidParam;
    }
    memcpy(g_bytes_, value, value_size);
    g_bytes_size_ = value_size;
    return kBscHseResultOk;
}

Int32 BscHseGetPropertyInt(TbscHseHandle handle,
                           TbscHsePropertyId property_id,
                           Int32 *value)
{
    (void)handle;
    (void)property_id;
    if (value == NULL)
    {
        return kBscHseResultInvalidParam;
    }
    *value = g_int_value_;
    return kBscHseResultOk;
}

Int32 BscHseGetPropertyBytes(TbscHseHandle handle,
                             TbscHsePropertyId property_id,
                             UInt8 *value,
                             UInt32 *value_size)
{
    (void)handle;
    (void)property_id;
    if ((value == NULL) || (value_size == NULL) ||
        (*value_size < g_bytes_size_))
    {
        return kBscHseResultInvalidParam;
    }
    memcpy(value, g_bytes_, g_bytes_size_);
    *value_size = g_bytes_size_;
    return kBscHseResultOk;
}

static Int32 L_CopyOperation(const UInt8 *input_data,
                             UInt32 input_size,
                             UInt8 *output_data,
                             UInt32 *output_size)
{
    if ((input_data == NULL) || (output_data == NULL) ||
        (output_size == NULL) || (*output_size < input_size))
    {
        return kBscHseResultInvalidParam;
    }
    memcpy(output_data, input_data, input_size);
    *output_size = input_size;
    return kBscHseResultOk;
}

Int32 BscHseEncrypt(TbscHseHandle handle,
                    TbscHseAlgorithm algorithm,
                    const UInt8 *input_data,
                    UInt32 input_size,
                    UInt8 *output_data,
                    UInt32 *output_size)
{
    (void)handle;
    (void)algorithm;
    return L_CopyOperation(input_data, input_size, output_data, output_size);
}

Int32 BscHseDecrypt(TbscHseHandle handle,
                    TbscHseAlgorithm algorithm,
                    const UInt8 *input_data,
                    UInt32 input_size,
                    UInt8 *output_data,
                    UInt32 *output_size)
{
    (void)handle;
    (void)algorithm;
    return L_CopyOperation(input_data, input_size, output_data, output_size);
}

Int32 BscHseSign(TbscHseHandle handle,
                 TbscHseAlgorithm algorithm,
                 const UInt8 *input_data,
                 UInt32 input_size,
                 UInt8 *signature,
                 UInt32 *signature_size,
                 UInt32 *r_size,
                 UInt32 *s_size)
{
    (void)handle;
    (void)algorithm;
    (void)input_data;
    (void)input_size;
    if ((signature == NULL) || (signature_size == NULL) ||
        (r_size == NULL) || (s_size == NULL) || (*signature_size < 4))
    {
        return kBscHseResultInvalidParam;
    }
    signature[0] = 1;
    signature[1] = 2;
    signature[2] = 3;
    signature[3] = 4;
    *signature_size = 4;
    *r_size = 2;
    *s_size = 2;
    return kBscHseResultOk;
}

Int32 BscHseExportPublicKey(TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            UInt8 *public_key,
                            UInt32 *public_key_size)
{
    (void)handle;
    (void)algorithm;
    if ((public_key == NULL) || (public_key_size == NULL) ||
        (*public_key_size < 3))
    {
        return kBscHseResultInvalidParam;
    }
    public_key[0] = 0xA1;
    public_key[1] = 0xA2;
    public_key[2] = 0xA3;
    *public_key_size = 3;
    return kBscHseResultOk;
}

Int32 BscHseImportPublicKey(TbscHseHandle handle,
                            TbscHseAlgorithm algorithm,
                            const UInt8 *public_key,
                            UInt32 public_key_size,
                            Bool replace_existing_key)
{
    (void)handle;
    (void)algorithm;
    (void)replace_existing_key;
    return ((public_key != NULL) && (public_key_size == 3))
               ? kBscHseResultOk
               : kBscHseResultInvalidParam;
}

int main(void)
{
    Int32 failed = 0;
    Int32 value = 0;
    Int32 index;
    UInt32 size;
    UInt32 r_size = 0;
    UInt32 s_size = 0;
    UInt8 input[TEST_BYTES_SIZE] = {1, 2, 3, 4};
    UInt8 output[TEST_BYTES_SIZE] = {0};
    TbscHseHandle handle1 = 0;
    TbscHseHandle handle2 = 0;
    TbscHseHandle pool_handles[BSC_HSE_OBJECT_POOL_SIZE] = {0};
    void *connection1 = (void *)1;
    void *connection2 = (void *)2;

    EXPECT_EQ("init", HsedSpcAdapterInit(), True);
    EXPECT_EQ("duplicate init", HsedSpcAdapterInit(), True);
    EXPECT_EQ("accept null", HsedSpcAdapterAccept(NULL), False);
    EXPECT_EQ("accept conn1", HsedSpcAdapterAccept(connection1), True);
    EXPECT_EQ("accept conn2", HsedSpcAdapterAccept(connection2), True);
    EXPECT_EQ("create handle1",
              HsedSpcAdapterCreate(connection1, &handle1),
              kBscHseResultOk);
    EXPECT_EQ("create handle2",
              HsedSpcAdapterCreate(connection1, &handle2),
              kBscHseResultOk);
    EXPECT_EQ("cross connection rejected",
              HsedSpcAdapterDestroy(connection2, handle1),
              kBscHseResultObjectNotFound);

    EXPECT_EQ("set int",
              HsedSpcAdapterSetPropertyInt(
                  connection1, handle1,
                  kBscHsePropertyIdAes128CipherMode, 7),
              kBscHseResultOk);
    EXPECT_EQ("get int",
              HsedSpcAdapterGetPropertyInt(
                  connection1, handle1,
                  kBscHsePropertyIdAes128CipherMode, &value),
              kBscHseResultOk);
    EXPECT_EQ("int value", value, 7);
    EXPECT_EQ("set bytes",
              HsedSpcAdapterSetPropertyBytes(
                  connection1, handle1, kBscHsePropertyIdUserId,
                  input, TEST_BYTES_SIZE),
              kBscHseResultOk);
    size = sizeof(output);
    EXPECT_EQ("get bytes",
              HsedSpcAdapterGetPropertyBytes(
                  connection1, handle1, kBscHsePropertyIdUserId,
                  output, &size),
              kBscHseResultOk);
    EXPECT_EQ("get bytes size", size, TEST_BYTES_SIZE);
    EXPECT_EQ("get bytes data", memcmp(input, output, sizeof(input)), 0);

    size = sizeof(output);
    EXPECT_EQ("encrypt",
              HsedSpcAdapterEncrypt(
                  connection1, handle1, kBscHseAlgorithmAes128,
                  input, sizeof(input), output, &size),
              kBscHseResultOk);
    size = sizeof(output);
    EXPECT_EQ("decrypt",
              HsedSpcAdapterDecrypt(
                  connection1, handle1, kBscHseAlgorithmAes128,
                  input, sizeof(input), output, &size),
              kBscHseResultOk);
    size = sizeof(output);
    EXPECT_EQ("sign",
              HsedSpcAdapterSign(
                  connection1, handle1, kBscHseAlgorithmSm2,
                  input, sizeof(input), output, &size, &r_size, &s_size),
              kBscHseResultOk);
    EXPECT_EQ("signature size", size, 4);
    EXPECT_EQ("signature r size", r_size, 2);
    EXPECT_EQ("signature s size", s_size, 2);
    size = sizeof(output);
    EXPECT_EQ("export",
              HsedSpcAdapterExportPublicKey(
                  connection1, handle1, kBscHseAlgorithmSm2,
                  output, &size),
              kBscHseResultOk);
    EXPECT_EQ("export size", size, 3);
    EXPECT_EQ("import",
              HsedSpcAdapterImportPublicKey(
                  connection1, handle1, kBscHseAlgorithmSm2,
                  output, size, False),
              kBscHseResultOk);

    HsedSpcAdapterFinish(connection1);
    EXPECT_EQ("disconnect cleanup count", g_destroy_count_, 2);
    EXPECT_EQ("stale handle rejected",
              HsedSpcAdapterDestroy(connection1, handle1),
              kBscHseResultObjectNotFound);

    for (index = 0; index < BSC_HSE_OBJECT_POOL_SIZE; index++)
    {
        EXPECT_EQ("fill object pool",
                  HsedSpcAdapterCreate(
                      connection2, &pool_handles[index]),
                  kBscHseResultOk);
    }
    EXPECT_EQ("object pool exhausted",
              HsedSpcAdapterCreate(connection2, &handle1),
              kBscHseResultNoResource);
    HsedSpcAdapterDone();
    EXPECT_EQ("shutdown cleanup count",
              g_destroy_count_, 2 + BSC_HSE_OBJECT_POOL_SIZE);
    HsedSpcAdapterDone();

    if (failed == 0)
    {
        (void)printf("[PASS] all SPC adapter operations, ownership, "
                     "disconnect cleanup, and pool exhaustion\n");
    }
    return (failed == 0) ? 0 : 1;
}
