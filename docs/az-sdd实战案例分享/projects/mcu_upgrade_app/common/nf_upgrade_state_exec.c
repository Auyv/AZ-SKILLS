/**
 * @file        nf_upgrade_state_exec.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/12/28
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/12/28   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "nf_upgrade_state_exec.h"
#include "mpu_upgrade_app.h"
#include "rte_rfop_upgrade.h"
#include "ffc/ffc_file.h"
#include "ffc/ffc_file_utils.h"
#include "ffc/ffc_file_stream.h"
#include "img_upgrade_file.h"
#include "upgrade_app_cfg.h"
#include "mpu_upgrade_app.h"
#include "srt/srt.h"
#include "srt_pid_type.h"
#include "cmdline.h"
#include "sfc/sfc_api.h"
#include "se_img.h"
#include "rte_rfop_server.h"

#define ALOG_TAG "nfupgrade"
#include "alog/a_log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>


extern Int32 RteRfopUpgradeSrtAdpGetHardwareVersion(UInt32 *val);

static void L_ShowDevVersion(const char *tag)
{
    Bool load_ok = False;
    Int32 err = 0;
    UInt32 val32 = 0;
    UInt8 buff[27] = {0};
    UInt32 curr_tick = SfGetTickCount();
    const UInt32 timeout_ms = 30000;

    while (SfGetTickCount() - curr_tick <= timeout_ms)  // <2026-01-13> Jpz : 等待srt同步完成。 30s超时
    { 
        if (RteRfopUpgradeSrtAdpGetHardwareVersion(&val32) >= 0
            && SrtGetValueU32(kSidUInt32DevTerminalId, &val32) >= 0
            && SrtGetValueMem(kSidBinMcuSwVersio, buff, sizeof(buff)) > 0
            && SrtGetValueMem(kSidBinDevMcuProgramBuildDate, buff, sizeof(buff)) > 0)
            break;
        else
            SfSleep(1000);
    }

    err = RteRfopUpgradeSrtAdpGetHardwareVersion(&val32);
    if (err >= 0)
        ALOG_I("++%.16s++ Hardware Version: %d", tag, val32);

    err = SrtGetValueU32(kSidUInt32DevTerminalId, &val32);
    if (err >= 0)
        ALOG_I("++%.16s++ DevTerminalId: %d ", tag, val32);
    
    err = SrtGetValueMem(kSidBinMcuSwVersio, buff, sizeof(buff));
    if (err > 0)
        ALOG_I("++%.16s++ McuSwVersion: %.*s", tag, err, buff);

    err = SrtGetValueMem(kSidBinDevMcuProgramBuildDate, buff, sizeof(buff));
    if (err > 0)
        ALOG_I("++%.16s++ McuProgramBuildDate: %.*s", tag, err, buff);

    err = SrtGetValueMem(kSidBinMpuSwVersio, buff, sizeof(buff));
    if (err > 0)
        ALOG_I("++%.16s++ MpuSwVersion: %.*s", tag, err, buff);
            
    ALOG_I("++%.16s++ AppProgramBuildDate: %s %s", tag, __DATE__, __TIME__);
}

static void NfUpgradeStateInitEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
    L_ShowDevVersion("NowVersionInfo");
}
static Int32 NfUpgradeStateInitRunOnce(TNfUpgradeApp *app)
{
    ALOG_D("NfUpgradeStateInitEnter");
    return 100;
}
static UInt8 NfUpgradeStateInitGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;

    ALOG_D("NfUpgradeStateInitFrom %d ", result);

    if (NfUpgradeAppRecover())
    {
        result = app->m_state;
        if (result == kNfUpgradeStateInit)
            result = kNfUpgradeStateIdle;
        else if (result == kNfUpgradeStateWaiteReboot)
        {
            UInt16 m_flag = app->m_flag;

            if (m_flag & kNfUpgradeFlagMpuUpgrade)
                result = kNfUpgradeStateMpuConfirm;
            else if (m_flag & kNfUpgradeFlagMcuUpgrade)
                result = kNfUpgradeStateMcuConfirm;
            else
                result = kNfUpgradeStateIdle;
        }
    }
    else
    {
        app->m_sub_state_param.reserve[0] += 1;
        if (app->m_sub_state_param.reserve[0] > 10)
        {
            result = kNfUpgradeStateIdle;
            app->m_errcode = kNfUpgradeErrCodeInitFailed;
        }
    }

    ALOG_D("NfUpgradeStateInitGoto %d ", result);

    return result;
}



static void NfUpgradeStateIdleEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStateIdleRunOnce(TNfUpgradeApp *app)
{
    
    return 100;
}
static UInt8 NfUpgradeStateIdleGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 req_id = app->req_id;
    UInt8 rsp_id= app->rsp_id;

    if (req_id != rsp_id)
    {
        app->m_errcode = 0;
        app->m_flag = 0;
        result = kNfUpgradeStatePreCheck;   // <2024-12-28> Jpz : 升级前预检查
    }

    return result;
}

//------------------------------------------------------------------------------------------------------
static const char *L_NfUpgradeStatePreCheckGetSeFilesName(TSeImgFileId id, void *user_param)
{
    static char s_pem_path_buf[256];
    const char *result = NULL;

    switch (id)
    {
        case kSeImgFileIdContextAse256CbcKey:
        {
            snprintf(s_pem_path_buf, sizeof(s_pem_path_buf), "%s/aes256_key.hex", NET_PACKAGE_PEM_KEYS_DIR);
            result = s_pem_path_buf;
            break;
        }

        case kSeImgFileIdContextAse256CbcIv:
        {
            snprintf(s_pem_path_buf, sizeof(s_pem_path_buf), "%s/aes256_iv.hex", NET_PACKAGE_PEM_KEYS_DIR);
            result = s_pem_path_buf;
            break;
        }

        case kSeImgFileIdSignatureRsaPublicPem:
        {
            snprintf(s_pem_path_buf, sizeof(s_pem_path_buf), "%s/signature_public_key.pem", NET_PACKAGE_PEM_KEYS_DIR);
            result = s_pem_path_buf;
            break;
        }
        default: break;
    }

    return result;
}

static void NfUpgradeStatePreCheckEnter(TNfUpgradeApp *app)
{
    RteRfopUpgradeSrtShowAlldata();

    app->upgrade_precent = 0;
    app->rsp_id = app->req_id;
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStatePreCheckRunOnce(TNfUpgradeApp *app)
{
    // <2024-12-28> Jpz todo : 拆分Img包
    Bool go_on = False;
    UInt8 create_cnt = 0;
    UInt8 *check_result = &app->m_sub_state_param.reserve[0];
    TFfFileStream img = {0};
    TFfFileStream fota = {0};
    TImgUpgradeFile iuf = {0};
    UInt8 *buffer = NULL;

    do
    {
        // #if (NET_UPGRADE_ENABLE) <2026-05-09> Jpz : 改为运行时配置控制
        if (NET_UPGRADE_ENABLE)
        {
            // 拼接下载包
            char addr[64] = {0};
            UInt16 port = 0;
            char fname[64] = {0};

            if (SrtGetValueMem(NET_PACKAGE_SRT_ID_ADDR, addr, sizeof(addr)) > 1
                && SrtGetValueU16(NET_PACKAGE_SRT_ID_PORT, &port) >= 0 && port > 1
                && SrtGetValueMem(NET_PACKAGE_SRT_ID_FILE, fname, sizeof(fname)) > 1)
            {
                ALOG_I("NET_PACKAGE_SRT_ID_ADDR id:%d data:%s", NET_PACKAGE_SRT_ID_ADDR, addr);
                ALOG_I("NET_PACKAGE_SRT_ID_PORT id:%d data:%d", NET_PACKAGE_SRT_ID_PORT, port);
                ALOG_I("NET_PACKAGE_SRT_ID_FILE id:%d data:%s", NET_PACKAGE_SRT_ID_FILE, fname);

                char url[128] = {0};
                Int32 r = snprintf(url, sizeof(url) - 1, NET_PACKAGE_URL_FORMAT, addr, port, fname); 

                ALOG_D("r = %d", r);
                ALOG_I("NET_UPGRADE_DOWNLOAD_URL:%.64s", url);
                ALOG_I("NET_PACKAGE_PEM_KEYS_DIR:%.64s", NET_PACKAGE_PEM_KEYS_DIR);
                
                // 删掉上次的文件
                FfDeleteFile(NFCREATE_UPGRADE_IMG_PATH);
                FfDeleteFile(NFCREATE_UPGRADE_SE_IMG_PATH);

                if (FfDirectoryExists(NET_PACKAGE_PEM_KEYS_DIR))
                {
                    ALOG_I("Find keys dir: NET_PACKAGE_PEM_KEYS_DIR");
                    if (CmdDownloadImg(url, NET_PACKAGE_PEM_KEYS_DIR) < 0)
                        break;
                }
                else
                {
                    ALOG_I("Not find keys dir: NET_PACKAGE_PEM_KEYS_DIR");
                    if (CmdDownloadImg(url, NULL) < 0)
                        break;
                }
                
            }
        }
        // #endif
        
        if (FfFileExists(NFCREATE_UPGRADE_SE_IMG_PATH))
        {
            // 解包
            Int32 result = -2000;
            TSeImg *se_img = CreateTSeImg(NFCREATE_UPGRADE_SE_IMG_PATH);

            if (se_img != NULL && TSeImgIsValid(se_img))
            {
                TSeImgSetFileNameFunc(se_img, L_NfUpgradeStatePreCheckGetSeFilesName, NULL);
                result = TSeImgUnpackImgToFile(se_img, NFCREATE_UPGRADE_IMG_PATH, NULL);
                ALOG_I("TSeImgUnpackImgToFile result:%d", result);
                DestroyTSeImg(se_img);
            }

            if (result < 0)
            {
                ALOG_E("TSeImgUnpackImgToFile failed, result:%d", result);
                break;
            }
        }
        else
        {
            ALOG_D("    not find NFCREATE_UPGRADE_SE_IMG_PATH@%s", NFCREATE_UPGRADE_SE_IMG_PATH);
        }
        

        if (!FfFileExists(NFCREATE_UPGRADE_IMG_PATH))
        {
            ALOG_D("    not find NFCREATE_UPGRADE_IMG_PATH@%s", NFCREATE_UPGRADE_IMG_PATH);
            break;
        }

        if (TFfFileStreamCreate(&img, NFCREATE_UPGRADE_IMG_PATH, kFfFileModeOpenRead))
            create_cnt++;
        else
            break;

        buffer = (UInt8 *)malloc(512);
        if (buffer == NULL)
            break;
        else
            create_cnt++;

        if (TImgUpgradeFileCreate(&iuf, &img.parent_, TFfFileStreamSeek(&img, 0, kFfSeekOriginFromEnd), buffer, 512))
            create_cnt++;
        else
            break;


        UInt32 hw_version;
        UInt32 *hw_version_ptr = NULL;

        if (RteRfopUpgradeSrtAdpGetHardwareVersion(&hw_version) >= 0)
        {
            hw_version_ptr = &hw_version;
        }

        if (TImgUpgradeFileOpen(&iuf, hw_version_ptr))
        {
            create_cnt++;

            TImageItemHeader *item_header = NULL;
            Int32 item_cnt = TImgUpgradeFileItemCount(&iuf);
            Int32 mpu_idx = -1;
            Int32 mcu_idx = -1;

            for (UInt32 idx = 0; idx < item_cnt; idx++)
            {
                item_header = TImgUpgradeFileItems(&iuf, idx);

                if (item_header == NULL)
                {
                    ALOG_E("TImgUpgradeFileItems(&iuf, %d) == NULL", idx);
                    break;
                }
                else if (item_header->obj_dev == IXP_DST_MPU_ONCHIP_FLASH)  // 有mpu升级包
                {
                    mpu_idx = idx;
                    ALOG_I("item_header->obj_dev: %d == IXP_DST_MPU_ONCHIP_FLASH", item_header->obj_dev);
                }
                else if (item_header->obj_dev == IXP_DST_ONCHIP_FLASH)  // 有mcu升级包
                {
                    mcu_idx = idx;
                    ALOG_I("item_header->obj_dev: %d == IXP_DST_ONCHIP_FLASH", item_header->obj_dev);
                }
                else 
                {
                    ALOG_W("item_header->obj_dev:%d, item_header->obj_addr:%d, item_header->size:%d", item_header->obj_dev, item_header->obj_addr, item_header->size);
                }
            }

            if (mcu_idx >= 0)
            {
                app->m_flag |= kNfUpgradeFlagMcuReq;
                ALOG_I("Request mcu upgrade. mcu_idx:%d m_flag:0x%02x", mcu_idx, app->m_flag);
            }
                
            
            if (mpu_idx >= 0)
            {
                app->m_flag |= kNfUpgradeFlagMpuReq;
                ALOG_I("Request mpu upgrade. mpu_idx:%d m_flag:0x%02x", mpu_idx, app->m_flag);
                
                UInt8 read_buff[512] = {0};
                Int32 write_ret = 0;
                Int32 file_size = 0;
                Int32 read_ret = 0;
                
                FfDeleteFile(MPU_UPGRADE_FILE_NAME);
                if (TFfFileStreamCreate(&fota, MPU_UPGRADE_FILE_NAME, kFfFileModeOpenWrite|kFfFileModeAlways))
                    create_cnt++;
                else
                {
                    ALOG_I("Create mpu ota.bin:%s Failed", MPU_UPGRADE_FILE_NAME);
                    break;
                }
                    

                read_ret = TImgUpgradeFileReadItem(&iuf, mpu_idx, file_size, read_buff, sizeof(read_buff));
                        
                while (read_ret > 0)
                {
                    write_ret = TFfFileStreamWrite(&fota, read_buff, read_ret);
                    if (read_ret != write_ret)
                    {
                        ALOG_E("TFfFileStreamWrite(&fota) failed, errcode:%d", write_ret);
                        break;
                    }
                    file_size += write_ret;
                    read_ret = TImgUpgradeFileReadItem(&iuf, mpu_idx, file_size, read_buff, sizeof(read_buff));
                }

                if (file_size == item_header->size)
                {
                    ALOG_I("Unpack mpu ota.bin:%s, file_size: %d Byte", MPU_UPGRADE_FILE_NAME, file_size);
                }
                else
                {
                    ALOG_E("Unpack mpu ota.bin:%s, file_size: %d Byte, read_ret:%d, item_header->size:%d", MPU_UPGRADE_FILE_NAME, file_size, read_ret, item_header->size);
                    break;
                }
            }
        }

        go_on = True;
        RteRfopUpgradeSrtNotifyUpgradeStart();  // <2025-07-07> Jpz : 文件检查成功. 关闭其他进程中可能会执行的reboot操作
    } while (0);
    

    switch (create_cnt)
    {
        case 5: OfDestroyObject((TOfObject *)&fota);  // TFfFileStreamDestroy(&fota); 
        case 4: TImgUpgradeFileClose(&iuf);
        case 3: TImgUpgradeFileDestroy(&iuf);
        case 2: free(buffer);
        case 1: OfDestroyObject((TOfObject *)&img);  // TFfFileStreamDestroy(&img);
        default: break;
    }

    *check_result = go_on ? 1 : 2;

    return 0;
}
static UInt8 NfUpgradeStatePreCheckGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 check_result = app->m_sub_state_param.reserve[0];

    if (check_result == 1)
    {
        UInt16 m_flag = app->m_flag;

        ALOG_T("PreCheck m_flag:0x%02x", m_flag);

        if ((m_flag & kNfUpgradeFlagMpuReq) != 0)
            result = kNfUpgradeStateMpuUpgrade;
        else if ((m_flag & kNfUpgradeFlagMcuReq) != 0)
            result = kNfUpgradeStateMcuUpgrade;
        else// <2024-12-28> Jpz : Img包格式异常
        {
            if (app->m_errcode == 0)
                app->m_errcode = kNfUpgradeErrCodeImgInvalid;
            result = kNfUpgradeStateFailed;
            ALOG_E("PreCheck Failed errcode:%d", app->m_errcode);
        }
    }
    else if (check_result == 2)
    {
        if (app->m_errcode == 0)
            app->m_errcode = kNfUpgradeErrCodeImgInvalid;
        result = kNfUpgradeStateFailed;
        ALOG_E("PreCheck Failed errcode:%d", app->m_errcode);
    }
    else
    {}


    return result;
}


static void NfUpgradeStateMpuUpgradeEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStateMpuUpgradeRunOnce(TNfUpgradeApp *app)
{
    static UInt32 log_tick = 0;
    UInt32 curr_tick = SfGetTickCount();
    UInt8 *mpu_upgrade_result = &app->m_sub_state_param.reserve[0];
    UInt8 *mpu_upgrade_step = &app->m_sub_state_param.reserve[1];

    if (*mpu_upgrade_step == 0)
    {
        Int32 r = MpuUpgradeRequest();
        
        if (r >= 0)
            *mpu_upgrade_step = 1;
        else
            *mpu_upgrade_result = 2;
        
        ALOG_T("MpuUpgradeRequest() result:%d", r);
    }
    else if (*mpu_upgrade_step == 1)
    {
        Int32 state;
        Int32 perc;
        Int32 errcode;
        Int32 r = MpuUpgradeResponse(&state, &perc, &errcode);

        if (errcode < 0 || perc < 0)
            ALOG_I("MpuUpgradeResponse ret:%d, state:%d, perc:%d, errcode:%d", r, state, perc, errcode);

        if (r < 0 || errcode < 0 || perc < 0)
        {
            *mpu_upgrade_result = 2;
            app->m_errcode = errcode;
            ALOG_I("MpuUpgradeResponse ret:%d, state:%d, perc:%d, errcode:%d", r, state, perc, errcode);
        }
        else
        {
            if (curr_tick - log_tick >= 10*1000)
            {
                ALOG_I("MpuUpgradeResponse state:%d, perc:%d, errcode:%d", state, perc, errcode);
                log_tick = curr_tick;
            }

            if (perc >= 100 // 正常刷写完成
                || state == kMpuUpgradeStateWaitReboot) // 上一次刷写过程中如果异常掉电重启，会出现刷写成功但是进度为0
                *mpu_upgrade_result = 1;
            else if (perc < 0   // 刷写过程中出现错误
                || state == kMpuUpgradeStateUpdateFailed)
                *mpu_upgrade_result = 2;
            else
                *mpu_upgrade_result = 0;

            // 检查是否需要刷写Mcu程序
            if ((app->m_flag & kNfUpgradeFlagMcuReq) != 0)
                perc = perc / 2;
            
            if (app->upgrade_precent != perc)
            {
                ALOG_T("MpuUpgradeResponse state:%d, perc:%d, errcode:%d", state, perc, errcode);
                app->upgrade_precent = perc;
            }
        }

    }
    else
        *mpu_upgrade_step = 0;    // 不可能出现的异常流程

    return 0;
}
static UInt8 NfUpgradeStateMpuUpgradeGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 mpu_upgrade_result = app->m_sub_state_param.reserve[0];

    if (mpu_upgrade_result == 1)
    {
        UInt16 m_flag = app->m_flag;

        app->m_flag |= kNfUpgradeFlagMpuUpgrade;

        // 检查是否需要刷写Mcu程序
        if ((m_flag & kNfUpgradeFlagMcuReq) != 0)
            result = kNfUpgradeStateMcuUpgrade;
        else
            result = kNfUpgradeStateWaiteReboot;    // 直接等待重启
    }
    // 升级失败, 直接退出. 不做mcu升级
    else if (mpu_upgrade_result == 2)
    {
        if (app->m_errcode == 0)
            app->m_errcode = kNfUpgradeErrCodeMpuUpgrade;
        result = kNfUpgradeStateFailed;
    }
    else
    {}

    return result;
}


static void NfUpgradeStateMcuUpgradeEnter(TNfUpgradeApp *app)
{
    Int32 mcu_state = RteRfopUpgradeCurTaskState();
    UInt8 *mcu_upgrade_step = &app->m_sub_state_param.reserve[1];

    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
    
    if (mcu_state == kRteRfopUpgradeStateIdle
        || mcu_state == kRteRfopUpgradeStateFailed
        || mcu_state == kRteRfopUpgradeStateDone)
    {
        *mcu_upgrade_step = 0;
        ALOG_W("RteRfopUpgradeCurTaskState:%d is valid", mcu_state);
    }
    else
    {
        *mcu_upgrade_step = 100;    // mcu升级状态异常
        app->m_errcode = kNfUpgradeErrCodeMcuState;
        ALOG_E("RteRfopUpgradeCurTaskState:%d is invalid", mcu_state);
    }
}
static Int32 NfUpgradeStateMcuUpgradeRunOnce(TNfUpgradeApp *app)
{
    UInt8 *mcu_upgrade_result = &app->m_sub_state_param.reserve[0];
    UInt8 *mcu_upgrade_step = &app->m_sub_state_param.reserve[1];

    if (*mcu_upgrade_step == 0)
    {
        Int32 r = RteRfopUpgradeStartTask(NFCREATE_UPGRADE_IMG_PATH, 0);

        if (r >= 0)
            *mcu_upgrade_step = 1;
        else
            *mcu_upgrade_result = 2;
    }
    else if (*mcu_upgrade_step == 1)
    {
        Int32 state = RteRfopUpgradeCurTaskState();
        Int32 perc = RteRfopUpgradeCurTaskPercent();
        Int32 errcode = RteRfopUpgradeCurTaskResult();

        if (errcode < 0 || perc < 0 || state == kRteRfopUpgradeStateInvlalid)
        {
            *mcu_upgrade_result = 2;
            app->m_errcode = errcode;
        }
        else
        {
            // 检查是否需要刷写Mcu程序
            if ((app->m_flag & kNfUpgradeFlagMpuReq) != 0)
                app->upgrade_precent = 50 + perc / 2;
            else
                app->upgrade_precent = perc;

            if (state == kRteRfopUpgradeStateFailed)
            {
                app->m_errcode = errcode;
                *mcu_upgrade_result = 2;
            }
            else if (state == kRteRfopUpgradeStateWaitReboot)
                *mcu_upgrade_result = 1;
            else
                *mcu_upgrade_result = 0;
        }
    }
    else
    {
        *mcu_upgrade_result = 2;
    }
        


    return 0;
}
static UInt8 NfUpgradeStateMcuUpgradeGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 mcu_upgrade_result = app->m_sub_state_param.reserve[0];
    UInt16 m_flag = app->m_flag;

    if (mcu_upgrade_result == 1)
    {
        app->m_flag |= kNfUpgradeFlagMcuUpgrade;

        result = kNfUpgradeStateWaiteReboot;    // 直接等待重启
        
    }
    // 升级失败, 直接退出. 不做mcu升级
    else if (mcu_upgrade_result == 2)
    {
        if (m_flag & kNfUpgradeFlagMpuUpgrade)
        {
            // <2024-12-28> Jpz todo : mcu升级失败, mpu升级成功. 怎么操作?????
        }

        if (app->m_errcode == 0)
            app->m_errcode = kNfUpgradeErrCodeMcuUpgrade;

        result = kNfUpgradeStateFailed;
        
        // <2025-12-26> Jpz : 升级正常异常后强制执行清理打开的文件句柄，防止mcu端忘记关闭造成二次升级重试失败
        RteRfopUpgradeClear();
    }
    else
    {}

    return result;
}


static void NfUpgradeStateWaiteRebootEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
    app->upgrade_precent = 100;
}
static Int32 NfUpgradeStateWaiteRebootRunOnce(TNfUpgradeApp *app)
{
    // <2024-12-28> Jpz todo : 等待重启, 1min周期日志提示
    return 0;
}
static UInt8 NfUpgradeStateWaiteRebootGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;

    // 上次为初始化, 表示设备发生了重启
    if (app->m_prev_state == kNfUpgradeStateInit)
    {
        UInt16 m_flag = app->m_flag;

        if (m_flag & kNfUpgradeFlagMpuUpgrade)
            result = kNfUpgradeStateMpuConfirm;
        else if (m_flag & kNfUpgradeFlagMcuUpgrade)
            result = kNfUpgradeStateMcuConfirm;
        // 状态异常, 恢复到空闲
        else
        {
            result = kNfUpgradeStateIdle;
        }
    }

    return result;
}


static void NfUpgradeStateMpuConfirmEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStateMpuConfirmRunOnce(TNfUpgradeApp *app)
{

    UInt8 *mpu_confirm_result = &app->m_sub_state_param.reserve[0];

    if (*mpu_confirm_result == 0) // 1,成功; 2,失败;
    {
        Int32 state;
        Int32 perc;
        Int32 errcode;
        Int32 r = MpuUpgradeResponse(&state, &perc, &errcode);

        if (r < 0 || errcode < 0 || perc < 0)
        {
            *mpu_confirm_result = 2;
            app->m_errcode = errcode;
        }
        else if (SfGetTickCount() - app->m_state_enter_tick >= 1000 * 60 * 25)
        {
            ALOG_E("MpuWaitConf Timeout");
            app->m_errcode = kNfUpgradeErrCodeMpuWaitConf;
            *mpu_confirm_result = 2;
        }
        else 
        {
            if (state == kMpuUpgradeStateUpdateSuccess)
                *mpu_confirm_result = 1;
            else if (state == kMpuUpgradeStateInvalid
                    || state == kMpuUpgradeStateWaitConfirm)
                *mpu_confirm_result = 0;
            else
            {
                *mpu_confirm_result = 2;
                ALOG_E("MpuWaitConf Invalid state:%d", state);
                app->m_errcode = kNfUpgradeErrCodeMpuWaitConf;
            }
                
        }
    }

    return 0;
}
static UInt8 NfUpgradeStateMpuConfirmGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 mpu_confirm_result = app->m_sub_state_param.reserve[0];
    UInt16 m_flag = app->m_flag;
    
    if (mpu_confirm_result == 1)
    {
        app->m_flag |= kNfUpgradeFlagMpuConfirm;

        // 检查是否需要刷写Mcu程序
        if (m_flag & kNfUpgradeFlagMcuUpgrade)
            result = kNfUpgradeStateMcuConfirm;
        else
           result = kNfUpgradeStateSuccess;    
    }
    else if (mpu_confirm_result == 2)
    {
        app->m_errcode = kNfUpgradeErrCodeMpuWaitConf;
        result = kNfUpgradeStateFailed;
    }
    else
    {}


    return result;
}


static void NfUpgradeStateMcuConfirmEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStateMcuConfirmRunOnce(TNfUpgradeApp *app)
{
    UInt8 *mcu_confirm_result = &app->m_sub_state_param.reserve[0];

    // *mcu_confirm_result = 0; // 1,成功; 2,失败;

    Int32 state = RteRfopUpgradeCurTaskState();
    // Int32 perc = RteRfopUpgradeCurTaskPercent();
    Int32 errcode = RteRfopUpgradeCurTaskResult();

    if (state == kRteRfopUpgradeStateDone)
        *mcu_confirm_result = 1;
    else if (state == kRteRfopUpgradeStateInvlalid
             || state == kRteRfopUpgradeStateWaitReboot
             || state == kRteRfopUpgradeStateWaitConfirm)
    {
        *mcu_confirm_result = 0;

        if (SfGetTickCount() - app->m_state_enter_tick >= 1000 * 60 * 2)
        {
            ALOG_E("McuWaitConf Timeout");
            app->m_errcode = kNfUpgradeErrCodeMcuWaitConf;
            *mcu_confirm_result = 2;
        }
    }
    else
    {
        app->m_errcode = errcode;
        *mcu_confirm_result = 2;
    }

    return 0;
}
static UInt8 NfUpgradeStateMcuConfirmGoto(TNfUpgradeApp *app)
{
    UInt8 result = app->m_state;
    UInt8 mcu_confirm_result = app->m_sub_state_param.reserve[0];
    UInt16 m_flag = app->m_flag;

    if (mcu_confirm_result == 1)
    {
        app->m_flag |= kNfUpgradeFlagMcuWaitConf;

        result = kNfUpgradeStateSuccess;
    }
    else if (mcu_confirm_result == 2)
    {
        if (app->m_errcode == 0)
            app->m_errcode = kNfUpgradeErrCodeMcuWaitConf;
        result = kNfUpgradeStateFailed;
    }
    else
    {}

    return result;
}


static void NfUpgradeStateSuccessEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
}
static Int32 NfUpgradeStateSuccessRunOnce(TNfUpgradeApp *app)
{
    return 0;
}
static UInt8 NfUpgradeStateSuccessGoto(TNfUpgradeApp *app)
{
    // UInt8 result = app->m_state;
    return NfUpgradeStateIdleGoto(app);
}


static void NfUpgradeStateFailedEnter(TNfUpgradeApp *app)
{
    memset(&app->m_sub_state_param, 0, sizeof(app->m_sub_state_param));
    // 打印全部srt数据
    RteRfopUpgradeSrtShowAlldata();
}
static Int32 NfUpgradeStateFailedRunOnce(TNfUpgradeApp *app)
{
    return 0;
}
static UInt8 NfUpgradeStateFailedGoto(TNfUpgradeApp *app)
{
    // UInt8 result = app->m_state;
    return NfUpgradeStateIdleGoto(app);
}



const TNfUpgradeStateExec g_nfupgrade_state_exec[kNfUpgradeStateMax] = {
    {"kInit", NfUpgradeStateInitEnter, NfUpgradeStateInitRunOnce, NfUpgradeStateInitGoto },  // = 0,   // 上电初始化
    {"kIdle", NfUpgradeStateIdleEnter, NfUpgradeStateIdleRunOnce, NfUpgradeStateIdleGoto },  // = 1,   // 空闲
    {"kPreCheck", NfUpgradeStatePreCheckEnter, NfUpgradeStatePreCheckRunOnce, NfUpgradeStatePreCheckGoto },  // = 2,   // 升级前预检查
    {"kMpuUpgrade", NfUpgradeStateMpuUpgradeEnter, NfUpgradeStateMpuUpgradeRunOnce, NfUpgradeStateMpuUpgradeGoto },  // = 3,   // Mpu程序刷写
    {"kMcuUpgrade", NfUpgradeStateMcuUpgradeEnter, NfUpgradeStateMcuUpgradeRunOnce, NfUpgradeStateMcuUpgradeGoto },  // = 4,   // Mcu程序刷写
    {"kWaiteReboot", NfUpgradeStateWaiteRebootEnter, NfUpgradeStateWaiteRebootRunOnce, NfUpgradeStateWaiteRebootGoto },  // = 5,   // 等待设备重启
    {"kMpuConfirm", NfUpgradeStateMpuConfirmEnter, NfUpgradeStateMpuConfirmRunOnce, NfUpgradeStateMpuConfirmGoto },  // = 6,   // Mpu程序确认
    {"kMcuConfirm", NfUpgradeStateMcuConfirmEnter, NfUpgradeStateMcuConfirmRunOnce, NfUpgradeStateMcuConfirmGoto },  // = 7,   // Mcu程序确认
    {"kSuccess", NfUpgradeStateSuccessEnter, NfUpgradeStateSuccessRunOnce, NfUpgradeStateSuccessGoto },  // = 8,   // 成功
    {"kFailed", NfUpgradeStateFailedEnter, NfUpgradeStateFailedRunOnce, NfUpgradeStateFailedGoto },  // = 9,   // 失败
};


///< Generated on "2024-12-28 12:18:14" by the tool "gen_hq_file.py >> V20231119" 

