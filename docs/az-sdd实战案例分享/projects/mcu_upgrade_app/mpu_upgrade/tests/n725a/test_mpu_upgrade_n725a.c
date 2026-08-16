#include "mpu_upgrade.h"
#include "nwy_common.h"
#include "nwy_fota.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_IMAGE_PATH "/tmp/n725a_fota_test.bin"

static int g_sdk_init_result = NWY_RES_OK;
static int g_sdk_init_count = 0;
static int g_sdk_deinit_count = 0;
static int g_fota_result = FOTA_UPDATE_NULL;
static int g_fota_ua_result = FOTA_DOWNLOAD_SUCCESS;
static int g_fota_ua_count = 0;
static char g_fota_ua_path[256] = {0};
static int g_active_slot_result = 0;
static char g_active_slot = 'a';
static int g_damaged_state = 0;
static int g_callback_count = 0;
static TMpuUpgradeState g_callback_state = kMpuUpgradeStateInvalid;
static int g_callback_percent = -999;

int nwy_sdk_init(void)
{
    g_sdk_init_count++;
    return g_sdk_init_result;
}

void nwy_sdk_deinit(void)
{
    g_sdk_deinit_count++;
}

int nwy_fota_ua(char *fota_package_path)
{
    g_fota_ua_count++;
    snprintf(g_fota_ua_path, sizeof(g_fota_ua_path), "%s", fota_package_path);
    return g_fota_ua_result;
}

int nwy_get_fota_result(void)
{
    return g_fota_result;
}

int nwy_get_active_system_slot(char *active_slot, int timeout_msec)
{
    (void)timeout_msec;
    if (g_active_slot_result == 0)
    {
        *active_slot = g_active_slot;
    }
    return g_active_slot_result;
}

int nwy_get_damaged_state(void)
{
    return g_damaged_state;
}

static void TestCallback(TMpuUpgradeState state, int percent, void *user_param)
{
    int *marker = (int *)user_param;
    g_callback_count++;
    g_callback_state = state;
    g_callback_percent = percent;
    if (marker != NULL)
    {
        (*marker)++;
    }
}

static void ResetStubs(void)
{
    g_sdk_init_result = NWY_RES_OK;
    g_fota_result = FOTA_UPDATE_NULL;
    g_fota_ua_result = FOTA_DOWNLOAD_SUCCESS;
    g_fota_ua_count = 0;
    memset(g_fota_ua_path, 0, sizeof(g_fota_ua_path));
    g_active_slot_result = 0;
    g_active_slot = 'a';
    g_damaged_state = 0;
    g_callback_count = 0;
    g_callback_state = kMpuUpgradeStateInvalid;
    g_callback_percent = -999;
}

static int Expect(int condition, const char *message)
{
    int result = 0;
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        result = 1;
    }
    return result;
}

static int CreateTestImage(void)
{
    int result = 0;
    FILE *file = fopen(TEST_IMAGE_PATH, "wb");
    if (file == NULL)
    {
        result = 1;
    }
    else
    {
        fputs("n725a", file);
        fclose(file);
    }
    return result;
}

static int TestInitAndPaths(void)
{
    int failed = 0;
    char long_path[160] = {0};

    ResetStubs();
    failed += Expect(MpuUpgradeInit() == 0, "init should succeed");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateIdle,
                     "no prior FOTA should map to idle");
    failed += Expect(MpuUpgradeSetImgPath(NULL) == kMpuUpgradeErrInvalidParam,
                     "NULL path should be rejected");
    failed += Expect(MpuUpgradeSetImgPath("") == kMpuUpgradeErrInvalidPath,
                     "empty path should be rejected");
    failed += Expect(MpuUpgradeSetImgPath("/tmp/not-exist-n725a.bin") == 0,
                     "missing image may be configured before package arrival");
    failed += Expect(MpuUpgradeExecUpdate(30, NULL, NULL) ==
                     kMpuUpgradeErrInvalidFile,
                     "missing image should be rejected when update starts");
    failed += Expect(g_fota_ua_count == 0,
                     "missing image should not call vendor FOTA");
    memset(long_path, 'x', sizeof(long_path) - 1);
    failed += Expect(MpuUpgradeSetImgPath(long_path) == kMpuUpgradeErrInvalidPath,
                     "overlong path should be rejected");
    failed += Expect(MpuUpgradeSetImgPath(TEST_IMAGE_PATH) == 0,
                     "valid image path should be accepted");
    failed += Expect(strcmp(MpuUpgradeImgPath(), TEST_IMAGE_PATH) == 0,
                     "stored image path should match");
    MpuUpgradeDone();
    failed += Expect(g_sdk_deinit_count > 0, "SDK should be deinitialized");
    return failed;
}

static int TestUpdateSuccessAndFailure(void)
{
    int failed = 0;
    int marker = 0;

    ResetStubs();
    failed += Expect(MpuUpgradeInit() == 0, "update test init should succeed");
    failed += Expect(MpuUpgradeSetImgPath(TEST_IMAGE_PATH) == 0,
                     "update test image should be accepted");
    failed += Expect(MpuUpgradeExecUpdate(0, &marker, TestCallback) == 0,
                     "update request should be accepted");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateUpgrading,
                     "accepted update should enter upgrading");
    MpuUpgradeRunOnce();
    failed += Expect(g_fota_ua_count == 1, "vendor FOTA should be called once");
    failed += Expect(strcmp(g_fota_ua_path, TEST_IMAGE_PATH) == 0,
                     "vendor FOTA should receive configured image");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateWaitReboot,
                     "successful submit should wait for reboot");
    failed += Expect(g_callback_percent == 100 &&
                     g_callback_state == kMpuUpgradeStateWaitReboot,
                     "successful submit should callback 100 percent");
    MpuUpgradeDone();

    ResetStubs();
    g_fota_ua_result = FOTA_DOWNLOAD_FAIL;
    failed += Expect(MpuUpgradeInit() == 0, "failure test init should succeed");
    failed += Expect(MpuUpgradeSetImgPath(TEST_IMAGE_PATH) == 0,
                     "failure test image should be accepted");
    failed += Expect(MpuUpgradeExecUpdate(30, &marker, TestCallback) == 0,
                     "failure test request should be accepted");
    MpuUpgradeRunOnce();
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateUpdateFailed,
                     "vendor failure should enter failed state");
    failed += Expect(g_callback_percent == kMpuUpgradeErrOptFailed,
                     "vendor failure should callback unified error");
    MpuUpgradeDone();
    return failed;
}

static int TestStartupResultMapping(void)
{
    int failed = 0;

    ResetStubs();
    g_fota_result = FOTA_UPDATE_SUCCESS;
    failed += Expect(MpuUpgradeInit() == 0, "success-result init should succeed");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateUpdateSuccess,
                     "FOTA success should map to update success");
    MpuUpgradeDone();

    ResetStubs();
    g_fota_result = FOTA_UPDATE_FAIL;
    failed += Expect(MpuUpgradeInit() == 0, "failed-result init should succeed");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateUpdateFailed,
                     "FOTA failure should map to update failure");
    MpuUpgradeDone();

    ResetStubs();
    g_fota_result = FOTA_NEEDSYNC;
    failed += Expect(MpuUpgradeInit() == 0, "sync-result init should succeed");
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateWaitConfirm,
                     "syncing result should wait for confirmation");
    MpuUpgradeRunOnce();
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateWaitConfirm,
                     "transient result should remain waiting");
    g_fota_result = FOTA_UPDATE_SUCCESS;
    MpuUpgradeRunOnce();
    failed += Expect(MpuUpgradeState() == kMpuUpgradeStateUpdateSuccess,
                     "completed sync should become update success");
    MpuUpgradeDone();

    ResetStubs();
    g_sdk_init_result = -4;
    failed += Expect(MpuUpgradeInit() == kMpuUpgradeErrOptFailed,
                     "SDK init failure should be propagated");
    return failed;
}

static int TestCompatibilityAndPartitionInfo(void)
{
    int failed = 0;
    int marker = 0;
    TMpuPartitionInfo info = {0};

    ResetStubs();
    failed += Expect(MpuUpgradeInit() == 0, "compatibility test init should succeed");
    failed += Expect(MpuUpgradeExecBackup(30, &marker, TestCallback) == 0,
                     "backup compatibility call should succeed");
    failed += Expect(g_callback_percent == 100,
                     "backup compatibility callback should report 100");
    failed += Expect(MpuUpgradeExecRollback(30, &marker, TestCallback) == 0,
                     "rollback compatibility call should succeed");
    failed += Expect(g_callback_percent == 100,
                     "rollback compatibility callback should report 100");

    g_active_slot = 'b';
    g_damaged_state = 1;
    failed += Expect(MpuUpgradeGetPartitionInfo(&info) == 0,
                     "partition query should succeed");
    failed += Expect(info.active_part == 'B' && info.other_part_is_damage == 1,
                     "partition query should normalize real SDK data");
    failed += Expect(MpuUpgradeGetPartitionInfo(NULL) == kMpuUpgradeErrInvalidParam,
                     "NULL partition output should be rejected");

    g_active_slot_result = -21;
    failed += Expect(MpuUpgradeGetPartitionInfo(&info) == kMpuUpgradeErrOptFailed,
                     "active slot query failure should be propagated");
    g_active_slot_result = 0;
    g_active_slot = 'x';
    failed += Expect(MpuUpgradeGetPartitionInfo(&info) == kMpuUpgradeErrOptFailed,
                     "invalid active slot should be rejected");
    g_active_slot = 'a';
    g_damaged_state = -7;
    failed += Expect(MpuUpgradeGetPartitionInfo(&info) == kMpuUpgradeErrOptFailed,
                     "damaged-state query failure should be propagated");
    MpuUpgradeDone();
    return failed;
}

int main(void)
{
    int failed = 0;

    if (CreateTestImage() != 0)
    {
        fprintf(stderr, "FAILED: unable to create test image\n");
        failed = 1;
    }
    else
    {
        failed += TestInitAndPaths();
        failed += TestUpdateSuccessAndFailure();
        failed += TestStartupResultMapping();
        failed += TestCompatibilityAndPartitionInfo();
        unlink(TEST_IMAGE_PATH);
    }

    if (failed == 0)
    {
        printf("PASS: N725A MPU upgrade adapter tests\n");
    }
    else
    {
        fprintf(stderr, "FAIL: %d assertion(s) failed\n", failed);
    }
    return failed == 0 ? 0 : 1;
}
