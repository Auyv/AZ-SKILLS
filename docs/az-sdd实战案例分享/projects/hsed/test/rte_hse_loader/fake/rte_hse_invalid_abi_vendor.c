#include "bsc_hse/bsc_hse_vendor.h"

#include <stddef.h>

static const TbscHseVendorOps g_ops_ = {
    BSC_HSE_VENDOR_ABI_VERSION + 1U,
    (UInt32)sizeof(TbscHseVendorOps),
    NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const TbscHseVendorInfo g_info_ = {
    "rte_test_invalid", "1.0", "fake"
};

static const TbscHseVendorBinding g_binding_ = {
    &g_ops_, &g_info_, NULL
};

const TbscHseVendorBinding *BscHseVendorSdkApi(void)
{
    return &g_binding_;
}
