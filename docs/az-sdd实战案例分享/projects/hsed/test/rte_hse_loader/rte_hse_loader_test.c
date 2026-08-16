#include "rte_hse.h"

#include "bsc_hse/bsc_hse.h"
#include "bsc_hse/bsc_hse_vendor.h"

#include <stdio.h>

static Int32 g_failed_ = 0;

static void L_Expect(const char *name, Int32 actual, Int32 expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "[FAIL] %s actual=%d expected=%d\n",
                name, (int)actual, (int)expected);
        g_failed_++;
    }
}

int main(int argc, char *argv[])
{
    Int32 result = 1;
    TRteHseConfig config = {0};
    TbscHseObject object_pool[BSC_HSE_OBJECT_POOL_SIZE] = {0};

    if (argc == 4)
    {
        config.vendor_so_path = "/path/that/does/not/exist.so";
        L_Expect("missing file", RteHseInit(&config),
                 kBscHseResultDeviceUnavailable);
        if (RteHseVendorBinding() != NULL)
        {
            g_failed_++;
        }

        config.vendor_so_path = argv[2];
        L_Expect("missing symbols", RteHseInit(&config),
                 kBscHseResultUnsupported);

        config.vendor_so_path = argv[3];
        L_Expect("invalid ABI load", RteHseInit(&config),
                 kBscHseResultOk);
        L_Expect("invalid ABI rejected by BSC",
                 BscHseInit(RteHseVendorBinding(), object_pool,
                            BSC_HSE_OBJECT_POOL_SIZE),
                 kBscHseResultUnsupported);
        RteHseDone();

        config.vendor_so_path = argv[1];
        L_Expect("valid load", RteHseInit(&config),
                 kBscHseResultOk);
        L_Expect("BSC init",
                 BscHseInit(RteHseVendorBinding(), object_pool,
                            BSC_HSE_OBJECT_POOL_SIZE),
                 kBscHseResultOk);
        BscHseDone();
        RteHseDone();

        L_Expect("self test", RteHseSelfTest(&config),
                 kBscHseResultOk);
        result = (g_failed_ == 0) ? 0 : 1;
    }
    else
    {
        fprintf(stderr, "usage: rte_hse_loader_test VALID MISSING INVALID_ABI\n");
    }
    return result;
}
