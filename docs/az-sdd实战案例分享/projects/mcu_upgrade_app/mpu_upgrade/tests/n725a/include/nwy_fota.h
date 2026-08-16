#ifndef _TEST_NWY_FOTA_H_
#define _TEST_NWY_FOTA_H_

enum
{
    FOTA_DOWNLOAD_SUCCESS = 0,
    FOTA_DOWNLOAD_FAIL = 1,
};

enum
{
    FOTA_UPDATE_SUCCESS = 0,
    FOTA_UPDATE_FAIL = 1,
    FOTA_UPDATE_NULL = 2,
    FOTA_UPDATING = 3,
    FOTA_NEEDSYNC = 4,
    FOTA_SYNCHING = 5,
    FOTA_UPDATING_OBM = 6,
    FOTA_WRITEDOWNE = 7,
};

int nwy_fota_ua(char *fota_package_path);
int nwy_get_fota_result(void);
int nwy_get_active_system_slot(char *active_slot, int timeout_msec);
int nwy_get_damaged_state(void);

#endif  ///< _TEST_NWY_FOTA_H_
