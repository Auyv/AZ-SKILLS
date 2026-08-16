#include "bsc_hse/bsc_hse_vendor.h"

#include <string.h>

static Int32 L_Init(void *context)
{
    (void)context;
    return kBscHseResultOk;
}

static Int32 L_Done(void *context)
{
    (void)context;
    return kBscHseResultOk;
}

static Int32 L_Crypt(void *context, const TbscHseVendorCryptParam *param)
{
    Int32 result = kBscHseResultInvalidParam;

    (void)context;
    if (param != NULL && param->output_data != NULL &&
        param->output_len != NULL &&
        *(param->output_len) >= param->input_len)
    {
        memcpy(param->output_data, param->input_data, param->input_len);
        *(param->output_len) = param->input_len;
        result = kBscHseResultOk;
    }
    return result;
}

static Int32 L_Sign(void *context, const TbscHseVendorSignParam *param)
{
    (void)context;
    (void)param;
    return kBscHseResultUnsupported;
}

static Int32 L_Export(void *context, TbscHseVendorKeyParam *param)
{
    (void)context;
    (void)param;
    return kBscHseResultUnsupported;
}

static Int32 L_Import(void *context, const TbscHseVendorKeyParam *param)
{
    (void)context;
    (void)param;
    return kBscHseResultUnsupported;
}

static const TbscHseVendorOps g_ops_ = {
    BSC_HSE_VENDOR_ABI_VERSION,
    (UInt32)sizeof(TbscHseVendorOps),
    L_Init,
    L_Done,
    L_Crypt,
    L_Crypt,
    L_Sign,
    L_Export,
    L_Import
};

static const TbscHseVendorInfo g_info_ = {
    "rte_test", "1.0", "fake"
};

static const TbscHseVendorBinding g_binding_ = {
    &g_ops_, &g_info_, NULL
};

const TbscHseVendorBinding *BscHseVendorSdkApi(void)
{
    return &g_binding_;
}

Int32 BscHseVendorTest(void)
{
    return kBscHseResultOk;
}
