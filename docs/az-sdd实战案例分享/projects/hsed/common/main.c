/**
 * @file        main.c
 * @brief       HSED 服务主入口
 *              - 解析命令行参数（--test-spi / --cfg）
 *              - 从 INI 配置读取 vendor_so_path
 *              - 通过 RTE 加载 vendor 并注入 BSC HSE
 * @note        INI 配置节：[bsc_hse] vendor_so_path = /usr/lib/libbsc_hse_ciu98_ylb04_a6.so
 * @author      HSED
 * @date        2026/07/21
 * @version     2.0
 */

#include "ofc/ofc_object.h"
#include "ofc/ofc_time_codec.h"
#include "ofc/ofc_types.h"
#include "sfc/sfc_mutex.h"
#include "srt/srt.h"
#include "bsc_hse/bsc_hse.h"
#include "config.h"
#include "hsed_spc_service.h"
#include "rte_hse.h"

#define ALOG_TAG "HSED"

#include "alog/alog.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Bool g_is_request_close_ = False;
static TSfCriticalSection g_alog_lock = {0};
static TbscHseObject g_bsc_hse_object_pool_[BSC_HSE_OBJECT_POOL_SIZE] = {0};

/* ============ 命令行解析 ============ */

typedef enum THsedCommandModeTag THsedCommandMode;
enum THsedCommandModeTag
{
    kHsedCommandNormal = 0,
    kHsedCommandSpiSelfTest,
    kHsedCommandInvalid
};

static THsedCommandMode L_ParseCommandLine(int argc, char *argv[],
                                            const char **cfg_path)
{
    THsedCommandMode result = kHsedCommandInvalid;

    if (cfg_path != NULL)
    {
        *cfg_path = NULL;
        if (argc == 1)
        {
            result = kHsedCommandNormal;
        }
        else if (argc == 3 && strcmp(argv[1], "--cfg") == 0 &&
                 argv[2][0] != '\0')
        {
            *cfg_path = argv[2];
            result = kHsedCommandNormal;
        }
        else if (argc == 2 && strcmp(argv[1], "--test-spi") == 0)
        {
            result = kHsedCommandSpiSelfTest;
        }
        else if (argc == 4 && strcmp(argv[1], "--test-spi") == 0 &&
                 strcmp(argv[2], "--cfg") == 0 && argv[3][0] != '\0')
        {
            *cfg_path = argv[3];
            result = kHsedCommandSpiSelfTest;
        }
    }
    return result;
}

static void L_PrintUsage(const char *program_name)
{
    fprintf(stderr, "usage: %s [--cfg <ini-path>]\n", program_name);
    fprintf(stderr, "       %s --test-spi [--cfg <ini-path>]\n", program_name);
}

/* ============ RTE HSE 初始化 ============ */

static Int32 L_RunRteSelfTest(const char *cfg_path)
{
    Int32 result = -1;
    TRteHseConfig config = {0};

    if (HsedConfigInit(cfg_path) == False)
    {
        ALOG_E("HSED self test config error: vendor_so_path missing");
        result = kBscHseResultInvalidParam;
    }
    else if (HsedConfigLoadSpiDevicePath(cfg_path) == False)
    {
        ALOG_E("HSED self test config error: [spi] device_path missing");
        result = kBscHseResultInvalidParam;
    }
    else
    {
        config.vendor_so_path = g_hsed_config.vendor_so_path;
        config.spi_device_path = g_hsed_config.spi_device_path;
        ALOG_I("HSED SPI self test path: %s", config.spi_device_path);
        result = RteHseSelfTest(&config);
        if (result == kBscHseResultOk)
        {
            ALOG_I("CIU98 communication self test passed");
        }
        else
        {
            ALOG_E("CIU98 communication self test failed: %d", result);
        }
    }
    return result;
}

static Int32 L_SelfTestExitCode(Int32 result)
{
    Int32 exit_code = 5;

    if (result == kBscHseResultOk)
    {
        exit_code = 0;
    }
    else if (result == kBscHseResultInvalidParam)
    {
        exit_code = 3;
    }
    else if (result == kBscHseResultBusy)
    {
        exit_code = 4;
    }
    return exit_code;
}

/* ============ 信号 / 日志 ============ */

static void L_SignalHandler(int signal_number)
{
    if ((signal_number == SIGINT) || (signal_number == SIGTERM))
    {
        g_is_request_close_ = True;
    }
}

static void L_DoAlogLock(void)
{
    TSfCriticalSectionEnter(&g_alog_lock);
}

static void L_DoAlogUnlock(void)
{
    TSfCriticalSectionLeave(&g_alog_lock);
}

static Int32 L_AlogGenTimestamp(Int8 *buffer, UInt16 buffer_len)
{
    Int32 result = -1;
    Word year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    UInt32 tick = SfGetTickCount();
    TTime current_time = SfGetSystemTime() + 8 * 60 * 60;

    if (buffer_len >= 30U)
    {
        OfDecodeDateTime(current_time, &year, &month, &day, &hour, &minute, &second);
        result = snprintf(buffer, buffer_len, "%u %04d-%02d-%02d %02d:%02d:%02d",
                         tick, year, month, day, hour, minute, second);
    }
    return result;
}

static Int32 L_AlogOutput(IALog *sender, UInt32 level, const Int8 *tag, UInt32 ticks,
                          const Int8 *log, Int16 length)
{
    Int32 result = length;
    Int16 index = 0;
    (void)sender;
    (void)level;
    (void)tag;
    (void)ticks;
    while (index < length)
    {
        putchar(log[index]);
        index++;
    }
    return result;
}

static const IALog g_alog_output_ =
{
    .DoBackendFlush = NULL,
    .DoBackendOutput = L_AlogOutput,
};

/* ============ 主流程 ============ */

static Int32 L_RunNormalService(const char *cfg_path)
{
    Int32 result = -1;
    Bool rte_initialized = False;
    Bool bsc_initialized = False;
    Bool spc_started = False;
    TRteHseConfig config = {0};

    SrtInit();
    ALOG_I("=========== HSED Start. build_time %s %s", __TIME__, __DATE__);

    if (HsedConfigInit(cfg_path) == False)
    {
        ALOG_E("HSED vendor_so_path missing");
    }
    else if (HsedConfigLoadSpiDevicePath(
                 (cfg_path != NULL) ? cfg_path : HSED_CONFIG_INI_PATH) ==
             False)
    {
        ALOG_E("HSED spi device_path missing");
    }
    else
    {
        config.vendor_so_path = g_hsed_config.vendor_so_path;
        config.spi_device_path = g_hsed_config.spi_device_path;
        ALOG_I("HSED RTE loading vendor from: %s",
               g_hsed_config.vendor_so_path);
        result = RteHseInit(&config);
        if (result != 0)
        {
            ALOG_E("HSED RTE vendor load failed: %d", result);
        }
        else
        {
            rte_initialized = True;
            ALOG_I("HSED RTE vendor loaded OK");

            result = BscHseInit(RteHseVendorBinding(),
                                g_bsc_hse_object_pool_,
                                BSC_HSE_OBJECT_POOL_SIZE);
            if (result != 0)
            {
                ALOG_E("HSED BscHseInit failed: %d", result);
            }
            else
            {
                bsc_initialized = True;
                result = RteHseCheckAndRepairKeyPairs(g_hsed_config.debug);
                if (result != 0)
                {
                    ALOG_E("HSED key slot validation failed: %d debug=%d",
                           result, g_hsed_config.debug);
                }
                else
                {
                    result = HsedSpcServiceStart();
                    if (result != 0)
                    {
                        ALOG_E("HSED SPC service start failed: %d", result);
                    }
                    else
                    {
                        spc_started = True;
                        while (g_is_request_close_ == False)
                        {
                            SfSleep(10);
                        }
                    }
                }
            }
        }
    }

    if (spc_started == True)
    {
        HsedSpcServiceStop();
    }
    if (bsc_initialized == True)
    {
        BscHseDone();
    }
    if (rte_initialized == True)
    {
        RteHseDone();
    }
    SrtDone();
    return result;
}

int main(int argc, char *argv[])
{
    Int32 result = 0;
    const char *cfg_path = NULL;
    THsedCommandMode command_mode = L_ParseCommandLine(argc, argv, &cfg_path);

    g_is_request_close_ = False;
    signal(SIGINT, L_SignalHandler);
    signal(SIGTERM, L_SignalHandler);
    OfSetMallocProc(malloc);
    OfSetFreeProc(free);
    TSfCriticalSectionCreate(&g_alog_lock);
    ALogInit(L_DoAlogLock, L_DoAlogUnlock);
    ALogSetEvent(kAlogEventSetStrTime, (long)L_AlogGenTimestamp);
    ALogRegisterOutput(6, &g_alog_output_);

    if (command_mode == kHsedCommandSpiSelfTest)
    {
        result = L_SelfTestExitCode(L_RunRteSelfTest(cfg_path));
    }
    else if (command_mode == kHsedCommandNormal)
    {
        result = L_RunNormalService(cfg_path);
    }
    else
    {
        L_PrintUsage(argv[0]);
        result = 2;
    }

    ALogDone();
    OfDestroyObject((TOfObject *)&g_alog_lock);

    return result;
}
