/**
 * @file        rte_hse.c
 * @brief       Linux HSE 动态库运行时适配
 * @note        平台加载逻辑仅位于 RTE，不进入 BSC HSE 组件
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "rte_hse.h"

#include "bsc_hse/bsc_hse_vendor.h"

#define ALOG_TAG "RTE_HSE"
#include "alog/alog.h"

#include <stddef.h>
#include <string.h>

#if defined(__linux__)
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#define RTE_HSE_CIU98_LOCK_PATH "/tmp/hsed-ciu98.lock"

typedef const TbscHseVendorBinding *(*TRteHseGetBindingFn)(void);
typedef Int32 (*TRteHseTestFn)(void);
typedef Int32 (*TRteHseConfigureFn)(const char *spi_device_path);

typedef struct TRteHseRuntimeTag TRteHseRuntime;
struct TRteHseRuntimeTag
{
    Bool                     initialized;
    void                    *library_handle;
    TbscHseVendorBinding     binding;
    TRteHseTestFn            test_fn;
    Int32                    lock_fd;
};

static TRteHseRuntime g_rte_hse_ = {0};

typedef struct TRteHseKeyPairPolicyTag TRteHseKeyPairPolicy;
struct TRteHseKeyPairPolicyTag
{
    TbscHseAlgorithm algorithm;
    UInt32 public_slot_id;
    UInt32 private_slot_id;
    UInt32 public_key_len;
};

static const TRteHseKeyPairPolicy g_key_pair_policy_[] = {
    { kBscHseAlgorithmRsa1024,
      BSC_HSE_RSA1024_DEFAULT_PUBLIC_KEY_ID,
      BSC_HSE_RSA1024_DEFAULT_PRIVATE_KEY_ID, 132U },
    { kBscHseAlgorithmRsa2048,
      BSC_HSE_RSA2048_DEFAULT_PUBLIC_KEY_ID,
      BSC_HSE_RSA2048_DEFAULT_PRIVATE_KEY_ID, 260U },
    { kBscHseAlgorithmEcc256,
      BSC_HSE_ECC256_DEFAULT_PUBLIC_KEY_ID,
      BSC_HSE_ECC256_DEFAULT_PRIVATE_KEY_ID, 64U },
    { kBscHseAlgorithmSm2,
      BSC_HSE_SM2_DEFAULT_PUBLIC_KEY_ID,
      BSC_HSE_SM2_DEFAULT_PRIVATE_KEY_ID, 64U }
};

#if defined(__linux__)
/* SFC provides process-local critical sections only.  The Linux RTE boundary
 * owns the required non-blocking inter-process CIU98 lock. */
static Int32 L_AcquireCiu98Lock(Int32 *out_lock_fd)
{
    Int32 result = kBscHseResultInvalidParam;
    Int32 lock_fd;

    if (out_lock_fd != NULL)
    {
        lock_fd = open(RTE_HSE_CIU98_LOCK_PATH, O_CREAT | O_RDWR, 0660);
        if (lock_fd < 0)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0)
        {
            (void)close(lock_fd);
            result = kBscHseResultBusy;
        }
        else
        {
            *out_lock_fd = lock_fd;
            result = kBscHseResultOk;
        }
    }
    return result;
}

static void L_ReleaseCiu98Lock(Int32 *lock_fd)
{
    if (lock_fd != NULL && *lock_fd >= 0)
    {
        (void)flock(*lock_fd, LOCK_UN);
        (void)close(*lock_fd);
        *lock_fd = -1;
    }
}

static void L_AssignSymbol(void *symbol, void *function_storage,
                           UInt32 function_storage_size)
{
    UInt32 copy_size = function_storage_size;

    if (copy_size > (UInt32)sizeof(symbol))
    {
        copy_size = (UInt32)sizeof(symbol);
    }
    memset(function_storage, 0, function_storage_size);
    memcpy(function_storage, &symbol, copy_size);
}

static Int32 L_LoadBinding(const char *so_path,
                           void **out_handle,
                           TbscHseVendorBinding *out_binding,
                           TRteHseTestFn *out_test_fn,
                           TRteHseConfigureFn *out_configure_fn)
{
    Int32 result = kBscHseResultInvalidParam;
    void *handle = NULL;
    void *symbol = NULL;
    TRteHseGetBindingFn get_binding = NULL;
    const TbscHseVendorBinding *binding = NULL;
    TRteHseTestFn test_fn = NULL;
    TRteHseConfigureFn configure_fn = NULL;

    if (so_path != NULL && so_path[0] != '\0' &&
        out_handle != NULL && out_binding != NULL && out_test_fn != NULL &&
        out_configure_fn != NULL)
    {
        handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL)
        {
            result = kBscHseResultDeviceUnavailable;
        }
        else
        {
            symbol = dlsym(handle, "BscHseVendorSdkApi");
            L_AssignSymbol(symbol, &get_binding, (UInt32)sizeof(get_binding));
            symbol = dlsym(handle, "BscHseVendorTest");
            L_AssignSymbol(symbol, &test_fn, (UInt32)sizeof(test_fn));
            symbol = dlsym(handle, "BscHseVendorConfigure");
            L_AssignSymbol(symbol, &configure_fn,
                           (UInt32)sizeof(configure_fn));

            if (get_binding == NULL)
            {
                result = kBscHseResultUnsupported;
            }
            else
            {
                binding = get_binding();
                if (binding == NULL)
                {
                    result = kBscHseResultUnsupported;
                }
                else
                {
                    *out_binding = *binding;
                    *out_handle = handle;
                    *out_test_fn = test_fn;
                    *out_configure_fn = configure_fn;
                    result = kBscHseResultOk;
                }
            }
        }
    }

    if (result != kBscHseResultOk && handle != NULL)
    {
        (void)dlclose(handle);
    }
    return result;
}
#endif

Int32 RteHseInit(const TRteHseConfig *config)
{
    Int32 result = kBscHseResultUnsupported;

#if defined(__linux__)
    void *handle = NULL;
    TbscHseVendorBinding binding = { NULL, NULL, NULL };
    TRteHseTestFn test_fn = NULL;
    TRteHseConfigureFn configure_fn = NULL;
    Int32 lock_fd = -1;

    if (config == NULL || config->vendor_so_path == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else if (g_rte_hse_.initialized == True)
    {
        result = kBscHseResultBusy;
    }
    else
    {
        if (config->spi_device_path != NULL)
        {
            result = L_AcquireCiu98Lock(&lock_fd);
        }
        else
        {
            result = kBscHseResultOk;
        }
        if (result != kBscHseResultOk)
        {
            return result;
        }
        result = L_LoadBinding(config->vendor_so_path, &handle,
                               &binding, &test_fn, &configure_fn);
        if (result == kBscHseResultOk && config->spi_device_path != NULL)
        {
            if (configure_fn == NULL)
            {
                result = kBscHseResultUnsupported;
            }
            else
            {
                result = configure_fn(config->spi_device_path);
            }
        }
        if (result == kBscHseResultOk)
        {
            g_rte_hse_.library_handle = handle;
            g_rte_hse_.binding = binding;
            g_rte_hse_.test_fn = test_fn;
            g_rte_hse_.lock_fd = lock_fd;
            g_rte_hse_.initialized = True;
        }
        else
        {
            if (handle != NULL)
            {
                (void)dlclose(handle);
            }
            L_ReleaseCiu98Lock(&lock_fd);
        }
    }
#else
    (void)config;
#endif
    return result;
}

Int32 RteHseSelfTest(const TRteHseConfig *config)
{
    Int32 result = kBscHseResultUnsupported;

#if defined(__linux__)
    void *handle = NULL;
    TbscHseVendorBinding binding = { NULL, NULL, NULL };
    TRteHseTestFn test_fn = NULL;
    TRteHseConfigureFn configure_fn = NULL;
    Int32 lock_fd = -1;

    if (config == NULL || config->vendor_so_path == NULL)
    {
        result = kBscHseResultInvalidParam;
    }
    else
    {
        if (config->spi_device_path != NULL)
        {
            result = L_AcquireCiu98Lock(&lock_fd);
        }
        else
        {
            result = kBscHseResultOk;
        }
        if (result != kBscHseResultOk)
        {
            return result;
        }
        result = L_LoadBinding(config->vendor_so_path, &handle,
                               &binding, &test_fn, &configure_fn);
        if (result == kBscHseResultOk)
        {
            if (config->spi_device_path != NULL && configure_fn == NULL)
            {
                result = kBscHseResultUnsupported;
            }
            else if (config->spi_device_path != NULL)
            {
                result = configure_fn(config->spi_device_path);
            }
            if (result == kBscHseResultOk && test_fn == NULL)
            {
                result = kBscHseResultUnsupported;
            }
            else if (result == kBscHseResultOk)
            {
                result = test_fn();
            }
            (void)dlclose(handle);
        }
        L_ReleaseCiu98Lock(&lock_fd);
    }
#else
    (void)config;
#endif
    return result;
}

const TbscHseVendorBinding *RteHseVendorBinding(void)
{
    const TbscHseVendorBinding *binding = NULL;

    if (g_rte_hse_.initialized == True)
    {
        binding = &g_rte_hse_.binding;
    }
    return binding;
}

Int32 RteHseCheckAndRepairKeyPairs(Bool allow_repair)
{
    Int32 result = kBscHseResultDeviceUnavailable;
    const TbscHseVendorOps *ops = g_rte_hse_.binding.ops;
    UInt32 index = 0;

    if (g_rte_hse_.initialized == True && ops != NULL &&
        ops->ExportPublicKey != NULL && ops->GenerateKeyPair != NULL)
    {
        result = kBscHseResultOk;
        while (index < (UInt32)(sizeof(g_key_pair_policy_) /
                                sizeof(g_key_pair_policy_[0])))
        {
            const TRteHseKeyPairPolicy *policy = &g_key_pair_policy_[index];
            TbscHseVendorKeyParam key_param;
            UInt8 public_key[BSC_HSE_MAX_PUBLIC_KEY_LEN] = {0};
            UInt32 public_key_len = sizeof(public_key);
            Int32 check_result;
            Int32 slot_result = kBscHseResultOk;

            memset(&key_param, 0, sizeof(key_param));
            key_param.slot_id = policy->public_slot_id;
            key_param.algorithm = policy->algorithm;
            key_param.out_key = public_key;
            key_param.out_key_len = &public_key_len;
            check_result = ops->ExportPublicKey(g_rte_hse_.binding.context,
                                                &key_param);
            if (check_result == kBscHseResultOk &&
                public_key_len == policy->public_key_len)
            {
                ALOG_I("key slot check OK alg=0x%08X public=%u private=%u len=%u",
                       (UInt32)policy->algorithm, policy->public_slot_id,
                       policy->private_slot_id, public_key_len);
            }
            else if (allow_repair == False)
            {
                ALOG_E("key slot check failed alg=0x%08X public=%u private=%u result=%d len=%u expected=%u debug=0",
                       (UInt32)policy->algorithm, policy->public_slot_id,
                       policy->private_slot_id, check_result, public_key_len,
                       policy->public_key_len);
                slot_result = (check_result == kBscHseResultOk) ?
                              kBscHseResultDeviceError : check_result;
            }
            else
            {
                TbscHseVendorGenerateKeyPairParam generate_param;

                memset(&generate_param, 0, sizeof(generate_param));
                generate_param.public_slot_id = policy->public_slot_id;
                generate_param.private_slot_id = policy->private_slot_id;
                generate_param.algorithm = policy->algorithm;
                generate_param.force_overwrite = True;
                ALOG_W("key slot repair alg=0x%08X public=%u private=%u result=%d len=%u expected=%u debug=1",
                       (UInt32)policy->algorithm, policy->public_slot_id,
                       policy->private_slot_id, check_result, public_key_len,
                       policy->public_key_len);
                slot_result = ops->GenerateKeyPair(g_rte_hse_.binding.context,
                                                   &generate_param);
                if (slot_result == kBscHseResultOk)
                {
                    public_key_len = sizeof(public_key);
                    slot_result = ops->ExportPublicKey(
                        g_rte_hse_.binding.context, &key_param);
                    if (slot_result == kBscHseResultOk &&
                        public_key_len != policy->public_key_len)
                    {
                        slot_result = kBscHseResultDeviceError;
                    }
                }
                if (slot_result != kBscHseResultOk)
                {
                    ALOG_E("key slot repair failed alg=0x%08X public=%u private=%u result=%d",
                           (UInt32)policy->algorithm,
                           policy->public_slot_id,
                           policy->private_slot_id, slot_result);
                }
                else
                {
                    ALOG_I("key slot repair OK alg=0x%08X public=%u private=%u len=%u",
                           (UInt32)policy->algorithm,
                           policy->public_slot_id,
                           policy->private_slot_id, public_key_len);
                }
            }
            if (slot_result != kBscHseResultOk &&
                result == kBscHseResultOk)
            {
                result = slot_result;
            }
            memset(public_key, 0, sizeof(public_key));
            index++;
        }
    }
    return result;
}

void RteHseDone(void)
{
#if defined(__linux__)
    if (g_rte_hse_.initialized == True)
    {
        if (g_rte_hse_.library_handle != NULL)
        {
            (void)dlclose(g_rte_hse_.library_handle);
        }
        L_ReleaseCiu98Lock(&g_rte_hse_.lock_fd);
        memset(&g_rte_hse_, 0, sizeof(g_rte_hse_));
    }
#endif
}
