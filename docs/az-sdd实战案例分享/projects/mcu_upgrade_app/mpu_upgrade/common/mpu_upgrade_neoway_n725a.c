/**
 * @file        mpu_upgrade_neoway_n725a.c
 * @brief       Neoway N725A MPU FOTA adapter
 * @note        Backup and rollback are compatibility no-op operations.
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2026/08/11
 * @version     1.0
 *
 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th>Description
 * <tr><td>2026/08/11   <td>1.0         <td>靳普诏       <td>新增N725A适配
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
#include "mpu_upgrade.h"

#if (HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_NEOWAY_N725A)

#include "nwy_common.h"
#include "nwy_fota.h"

#define ALOG_TAG "N725aUpgrade"
#include "alog/a_log.h"

#include <string.h>
#include <unistd.h>

#define N725A_IMAGE_PATH_SIZE          128
#define N725A_DEFAULT_TIMEOUT_S        (10 * 60)
#define N725A_SLOT_QUERY_TIMEOUT_MS    3000

typedef struct TN725aMpuUpgradeTag TN725aMpuUpgrade;
struct TN725aMpuUpgradeTag
{
    TMpuUpgradeState state;
    char img_path[N725A_IMAGE_PATH_SIZE];
    int sdk_initialized;
    int req_upgrade;
    int upgrade_timeout_s;
    int last_vendor_result;
    void *upgrade_user_param;
    TMpuUpgradeCallback upgrade_callback;
};

static TN725aMpuUpgrade g_mpu_upgrade_ = {0};

static TMpuUpgradeState L_N725aFotaResultToState(int fota_result)
{
    TMpuUpgradeState state = kMpuUpgradeStateUpdateFailed;

    switch (fota_result)
    {
        case FOTA_UPDATE_SUCCESS:
            state = kMpuUpgradeStateUpdateSuccess;
            break;

        case FOTA_UPDATE_FAIL:
            state = kMpuUpgradeStateUpdateFailed;
            break;

        case FOTA_UPDATE_NULL:
            state = kMpuUpgradeStateIdle;
            break;

        case FOTA_UPDATING:
        case FOTA_NEEDSYNC:
        case FOTA_SYNCHING:
        case FOTA_UPDATING_OBM:
        case FOTA_WRITEDOWNE:
            state = kMpuUpgradeStateWaitConfirm;
            break;

        default:
            state = kMpuUpgradeStateUpdateFailed;
            break;
    }
    return state;
}

static void L_N725aUpgradeCallback(int percent)
{
    TMpuUpgradeCallback callback = g_mpu_upgrade_.upgrade_callback;

    if (callback != NULL)
    {
        callback(g_mpu_upgrade_.state, percent,
                 g_mpu_upgrade_.upgrade_user_param);
    }
}

int MpuUpgradeInit(void)
{
    int result = kMpuUpgradeErrOptFailed;
    int fota_result = FOTA_UPDATE_NULL;

    memset(&g_mpu_upgrade_, 0, sizeof(g_mpu_upgrade_));
    g_mpu_upgrade_.state = kMpuUpgradeStateInvalid;

    if (nwy_sdk_init() == NWY_RES_OK)
    {
        g_mpu_upgrade_.sdk_initialized = 1;
        fota_result = nwy_get_fota_result();
        g_mpu_upgrade_.last_vendor_result = fota_result;
        g_mpu_upgrade_.state = L_N725aFotaResultToState(fota_result);
        result = kMpuUpgradeErrNone;
        ALOG_I("N725A SDK initialized, FOTA result: %d, state: %d",
               fota_result, g_mpu_upgrade_.state);
    }
    else
    {
        ALOG_E("nwy_sdk_init failed");
    }
    return result;
}

void MpuUpgradeRunOnce(void)
{
    int vendor_result = 0;

    if (g_mpu_upgrade_.req_upgrade &&
        g_mpu_upgrade_.state == kMpuUpgradeStateUpgrading)
    {
        g_mpu_upgrade_.req_upgrade = 0;
        vendor_result = nwy_fota_ua(g_mpu_upgrade_.img_path);
        g_mpu_upgrade_.last_vendor_result = vendor_result;
        if (vendor_result == FOTA_DOWNLOAD_SUCCESS)
        {
            g_mpu_upgrade_.state = kMpuUpgradeStateWaitReboot;
            L_N725aUpgradeCallback(100);
            ALOG_I("nwy_fota_ua accepted, wait for reboot");
        }
        else
        {
            g_mpu_upgrade_.state = kMpuUpgradeStateUpdateFailed;
            L_N725aUpgradeCallback(kMpuUpgradeErrOptFailed);
            ALOG_E("nwy_fota_ua failed: %d", vendor_result);
        }
    }
    else if (g_mpu_upgrade_.state == kMpuUpgradeStateWaitConfirm)
    {
        vendor_result = nwy_get_fota_result();
        g_mpu_upgrade_.last_vendor_result = vendor_result;
        if (vendor_result == FOTA_UPDATE_SUCCESS ||
            vendor_result == FOTA_UPDATE_FAIL)
        {
            g_mpu_upgrade_.state = L_N725aFotaResultToState(vendor_result);
            ALOG_I("N725A FOTA confirm result: %d, state: %d",
                   vendor_result, g_mpu_upgrade_.state);
        }
        else if (vendor_result < 0)
        {
            g_mpu_upgrade_.state = kMpuUpgradeStateUpdateFailed;
            ALOG_E("nwy_get_fota_result failed: %d", vendor_result);
        }
        else
        {
        }
    }
    else
    {
    }
}

void MpuUpgradeDone(void)
{
    if (g_mpu_upgrade_.sdk_initialized)
    {
        nwy_sdk_deinit();
    }
    memset(&g_mpu_upgrade_, 0, sizeof(g_mpu_upgrade_));
    g_mpu_upgrade_.state = kMpuUpgradeStateInvalid;
}

TMpuUpgradeState MpuUpgradeState(void)
{
    return g_mpu_upgrade_.state;
}

char *MpuUpgradeImgPath(void)
{
    return g_mpu_upgrade_.img_path;
}

int MpuUpgradeSetImgPath(char *img_path)
{
    int result = kMpuUpgradeErrInvalidParam;
    size_t path_len = 0;

    if (img_path != NULL)
    {
        path_len = strlen(img_path);
        if (path_len == 0)
        {
            result = kMpuUpgradeErrInvalidPath;
        }
        else if (path_len >= sizeof(g_mpu_upgrade_.img_path))
        {
            result = kMpuUpgradeErrInvalidPath;
            ALOG_E("N725A image path is too long");
        }
        else
        {
            memcpy(g_mpu_upgrade_.img_path, img_path, path_len);
            g_mpu_upgrade_.img_path[path_len] = '\0';
            result = kMpuUpgradeErrNone;
        }
    }
    return result;
}

int MpuUpgradeExecUpdate(int timeout_s, void *user_param,
                         TMpuUpgradeCallback callback)
{
    int result = kMpuUpgradeErrInvalidState;
    TMpuUpgradeState state = g_mpu_upgrade_.state;

    if (!g_mpu_upgrade_.sdk_initialized)
    {
        result = kMpuUpgradeErrInvalidState;
    }
    else if (state != kMpuUpgradeStateIdle &&
             state != kMpuUpgradeStateUpdateSuccess &&
             state != kMpuUpgradeStateUpdateFailed)
    {
        result = kMpuUpgradeErrInvalidState;
    }
    else if (g_mpu_upgrade_.img_path[0] == '\0')
    {
        g_mpu_upgrade_.state = kMpuUpgradeStateUpdateFailed;
        result = kMpuUpgradeErrInvalidPath;
    }
    else if (access(g_mpu_upgrade_.img_path, F_OK) != 0)
    {
        g_mpu_upgrade_.state = kMpuUpgradeStateUpdateFailed;
        result = kMpuUpgradeErrInvalidFile;
    }
    else
    {
        g_mpu_upgrade_.upgrade_timeout_s = timeout_s > 0 ?
                                          timeout_s :
                                          N725A_DEFAULT_TIMEOUT_S;
        g_mpu_upgrade_.upgrade_user_param = user_param;
        g_mpu_upgrade_.upgrade_callback = callback;
        g_mpu_upgrade_.req_upgrade = 1;
        g_mpu_upgrade_.state = kMpuUpgradeStateUpgrading;
        g_mpu_upgrade_.last_vendor_result = 0;
        result = kMpuUpgradeErrNone;
        L_N725aUpgradeCallback(0);
    }
    return result;
}

int MpuUpgradeExecBackup(int timeout_s, void *user_param,
                         TMpuUpgradeCallback callback)
{
    int result = kMpuUpgradeErrNone;

    (void)timeout_s;
    ALOG_I("N725A backup compatibility no-op");
    if (callback != NULL)
    {
        callback(kMpuUpgradeStateBackuping, 100, user_param);
    }
    return result;
}

int MpuUpgradeExecRollback(int timeout_s, void *user_param,
                           TMpuUpgradeCallback callback)
{
    int result = kMpuUpgradeErrNone;

    (void)timeout_s;
    ALOG_I("N725A rollback compatibility no-op");
    if (callback != NULL)
    {
        callback(g_mpu_upgrade_.state, 100, user_param);
    }
    return result;
}

int MpuUpgradeGetPartitionInfo(TMpuPartitionInfo *partition_info)
{
    int result = kMpuUpgradeErrInvalidParam;
    int vendor_result = 0;
    int damaged_state = 0;
    char active_slot = 0;

    if (partition_info != NULL)
    {
        if (!g_mpu_upgrade_.sdk_initialized)
        {
            result = kMpuUpgradeErrInvalidState;
        }
        else
        {
            vendor_result = nwy_get_active_system_slot(
                                &active_slot,
                                N725A_SLOT_QUERY_TIMEOUT_MS);
            if (vendor_result != NWY_RES_OK)
            {
                result = kMpuUpgradeErrOptFailed;
                ALOG_E("nwy_get_active_system_slot failed: %d",
                       vendor_result);
            }
            else if (active_slot != 'a' && active_slot != 'A' &&
                     active_slot != 'b' && active_slot != 'B')
            {
                result = kMpuUpgradeErrOptFailed;
                ALOG_E("invalid N725A active slot: %d", active_slot);
            }
            else
            {
                damaged_state = nwy_get_damaged_state();
                if (damaged_state < 0)
                {
                    result = kMpuUpgradeErrOptFailed;
                    ALOG_E("nwy_get_damaged_state failed: %d",
                           damaged_state);
                }
                else
                {
                    partition_info->active_part =
                        (active_slot == 'a' || active_slot == 'A') ?
                        'A' : 'B';
                    partition_info->other_part_is_damage =
                        damaged_state > 0 ? 1 : 0;
                    result = kMpuUpgradeErrNone;
                }
            }
        }
    }
    return result;
}

#endif  // HQ_CMAKE_BUILD_MODEL_TYPE == HQ_CMAKE_BUILD_MODEL_NEOWAY_N725A
