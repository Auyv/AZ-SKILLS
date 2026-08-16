#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "rfop/rfop_server.h"
#include "rfop/rfop_common.h"
#include "rte_rfop_upgrade.h"
#include "mfc/mfc_com.h"
#include "sfc/sfc_api.h"
#include "mfc/mfc_md5.h"
// #include "rfop/rfop_dpt_data.h"
#include "srt/srt.h"
#include "a_log_backend_file_impl.h"
#include "ofc/ofc_time_codec.h"

#include "rte_rfop_server.h"
#include "mpu_upgrade_app.h"
#include "nf_upgrade_app.h"
#include "upgrade_app_cfg.h"
#include "ffc/ffc_file_utils.h"

#define ALOG_TAG "rfop_server"
#include "alog/a_log.h"

#include "cmdline.h"
Bool g_is_request_close_ = False;

static void sigint_handler(int sig)
{
	switch (sig) {
    case SIGTERM:
    case SIGINT:
        printf("Received quit signal.\n");
        g_is_request_close_ = True;
        break;

    default:
        printf("Received unexpected signal %d\n", signal);
        break;
    }
}

void Run(int com_no, int baudrate)
{
    int size = 0;
    // static UInt32 last_tick = 0;
    // UInt32 now = 0;

    // 初始化rfop_server
    if (MCU_UPGRADE_ENABLE)
    {
        RteRfopServerCreate();
        RteRfopServerInit();
    }
    
    if (MPU_UPGRADE_ENABLE)
    {
        MpuUpgradeAppCreate();
        MpuUpgradeAppInit();
    }

    NfUpgradeAppCreate();
    NfUpgradeAppInit();

    ALOG_E("Run\n");

    while (!g_is_request_close_)
    {
        if (MPU_UPGRADE_ENABLE)
            MpuUpgradeAppRunOnce();
        
        NfUpgradeAppRunOnce();

        if (MCU_UPGRADE_ENABLE)
            RteRfopServerRunOnce();

        SfSleep(20);
    }

    NfUpgradeAppDone();
    NfUpgradeAppDestroy();

    if (MPU_UPGRADE_ENABLE)
    {
        MpuUpgradeAppDone();
        MpuUpgradeAppDestroy();
    }

    if (MCU_UPGRADE_ENABLE)
    {
        RteRfopServerDone();
        RteRfopServerDestroy();
    }
}

static int g_lock_signal1 = 0;
static int g_lock_signal2 = 0;

static void L_TDoAlogLock(void)
{
    while (g_lock_signal1 != g_lock_signal2)
        ;
    g_lock_signal1++;
}

static void L_DoAlogUnLock(void)
{
    g_lock_signal2 = g_lock_signal1;
}

static Int32 L_ALogGenStrTimeStamp(Int8 *buff, UInt16 buff_len)
{
    static UInt32 last_time = 0;
    static Word year = 0;
    static Word month = 0;
    static Word day = 0;
    static Word hour = 0;
    static Word minute = 0;
    static Word second = 0;

    UInt32 curr_tick = SfGetTickCount();
    TTime curr_time = SfGetSystemTime() + 8 * 60 * 60;

    if (curr_time != last_time)
    {
        OfDecodeDateTime(curr_time, &year, &month, &day, &hour, &minute, &second);
        last_time = curr_time;
    }
    
    return buff_len >= 30 ? snprintf(buff, buff_len, "%u %04d-%02d-%02d %02d:%02d:%02d", curr_tick, year, month, day, hour, minute, second) : -1;
}

static Int32 L_ALogDoBackendOutput(IALog *sender, UInt32 level, const Int8 *tag, UInt32 ticks, const Int8 *log, Int16 len)
{
    char *pos = (char *)log;
    char *end = pos + len;
    while (pos < end)
    {
        putchar(*pos++);
    }
}

const IALog g_alog_if =
{
    .DoBackendFlush = NULL,
    .DoBackendOutput = L_ALogDoBackendOutput,
};

static TAlogBackendFileImpl g_alog_file_impl = {0};

TMfMd5Ctx test_md5;

static void L_LogUpgradeCfg(Int32 load_result)
{
    const TUpgradeCfg *cfg = UpgradeCfgGet();

    if (load_result >= 0)
    {
        ALOG_I("UpgradeCfg load success, path:%s", UPGRADE_CFG_INI_PATH);
    }
    else
    {
        ALOG_W("UpgradeCfg load failed, use defaults, path:%s, result:%d",
            UPGRADE_CFG_INI_PATH, load_result);
    }

    ALOG_I("UpgradeCfg [upgrade] mcu_upgrade_enable:%d, mpu_upgrade_enable:%d",
        cfg->mcu_upgrade_enable, cfg->mpu_upgrade_enable);
    ALOG_I("UpgradeCfg [log] file_dir:%s, file_tag:%s, file_size:%d, file_count:%d",
        cfg->log_file_dir, cfg->log_file_tag, cfg->log_file_size, cfg->log_file_count);
    ALOG_I("UpgradeCfg [nf_upgrade] img_file:%s, se_img_file:%s",
        cfg->nf_upgrade_img_path, cfg->nf_upgrade_se_img_path);
    ALOG_I("UpgradeCfg [mpu_upgrade] upgrade_file:%s, partition_record_file:%s",
        cfg->mpu_upgrade_file_name, cfg->mpu_partition_record_file);
    ALOG_I("UpgradeCfg [net_upgrade] enable:%d, url_format:%s, pem_keys_dir:%s",
        cfg->net_upgrade_enable, cfg->net_package_url_format, cfg->net_package_pem_keys_dir);
    ALOG_I("UpgradeCfg [net_upgrade] srt_id_addr:%d, srt_id_port:%d, srt_id_file:%d",
        cfg->net_package_srt_id_addr, cfg->net_package_srt_id_port,
        cfg->net_package_srt_id_file);
}

int main(int argc, char *argv[])
{
    Int32 upgrade_cfg_result;

    g_is_request_close_ = False;
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // 初始化配置 (从 INI 文件加载，默认路径 /vendor/app/hq/etc/nfupgrade_cfg.ini)
    upgrade_cfg_result = UpgradeCfgInit(NULL);

    // 使用运行时配置初始化日志参数
    TAlogBackendFileParam alog_file_param = {0};
    alog_file_param.file_count_max = LOG_FILE_COUNT;
    alog_file_param.file_size_max = LOG_FILE_SIZE;
    alog_file_param.file_dir = LOG_FILE_DIR;
    alog_file_param.file_name = LOG_FILE_TAG;

    ALogInit(L_TDoAlogLock, L_DoAlogUnLock);
    
    ALogSetEvent(kAlogEventSetStrTime, (long)L_ALogGenStrTimeStamp);
    
    ALogRegisterOutput(6, &g_alog_if);
    
    TAlogBackendFileImplCreate(&g_alog_file_impl, &alog_file_param);
    
    ALogRegisterOutput(6, TAlogBackendFileImplInterface(&g_alog_file_impl));

    L_LogUpgradeCfg(upgrade_cfg_result);

    SrtInit();
    
    ALOG_W("***mcu_upgrade_app start***");
    
    Run(0, 0);
    ALOG_W("***mcu_upgrade_app exit***");

    TAlogBackendFileImplDestroy(&g_alog_file_impl);

    SrtDone();
    ALogDone();
    return 0;
}
