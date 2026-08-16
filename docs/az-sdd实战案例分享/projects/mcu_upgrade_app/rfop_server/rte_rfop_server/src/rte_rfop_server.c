/**
 * @file        rte_rfop_server.c
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2024/10/31
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2024/10/31   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */
 
#include "rte_rfop_server.h"
#include "rfop/rfop_server.h"
#include "rte_rfop_upgrade.h"

#include "sfc/sfc_api.h"

#define ALOG_TAG "RteRfopServer"
#include "alog/a_log.h"



#if (RTE_RFOP_SERVER_DATALINK_TYPE == RTE_RFOP_SERVER_DATALINK_XCU)
#include "bsc_master_xcu_dev.h"
#define RTE_RFOP_XCU_DEV_NAME        "/dev/vmcuRfop"  ///< 用于获取xcu设备. 

static char g_rfop_xcu_recv_buffer_[1024] = {0};
static TBsMasterXcuVirBusImpl g_rfop_xcu_impl_ = {0};
static TBsMasterXcuDev g_rfop_xcu_dev_ = {0};
#elif (RTE_RFOP_SERVER_DATALINK_TYPE == RTE_RFOP_SERVER_DATALINK_COM)
#include "mfc/mfc_serial_vir_bus_mfc_com_impl.h"

#define RTE_RFOP_COM_DEV_NAME        "/dev/vmcuRfop"  ///< 用于获取串口设备. 
#define RTE_RFOP_COM_BAUDRATE        115200

static TMfSerialVirBusMfcComImpl g_rfop_com_virbus = {0};

#else
#error "RTE_RFOP_SERVER_DATALINK_TYPE is not defined!"
#endif

static UInt8 g_rfop_datalink_recv_buffer_[1024] = {0};

// ----------------------------------------------------------------

UInt8 g_rte_rfop_upgrade_rx_buffer[1024];
UInt8 g_rte_rfop_upgrade_common_buffer[RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE];

static const TRfopSrvCfg g_rte_rfop_cfg = {
    .rx_process_buffer_size = sizeof(g_rte_rfop_upgrade_rx_buffer), // sizeof(g_rfop_upgrade_clt_blk_.rfop_tx_and_rx_buffer),
    .rx_process_buffer = (Int8 *)g_rte_rfop_upgrade_rx_buffer,
    .tx_process_buffer_size = RTE_RFOP_UPGRADE_COMMON_BUFFER_SIZE,
    .tx_process_buffer = (Int8 *)RTE_RFOP_UPGRADE_COMMON_BUFFER_ADDR,
};
// ----------------------------------------------------------------

static TRfopSrv g_rfop_srv_; // 升级文件服务端


/// 创建用于数据同步的虚拟总线
#if (RTE_RFOP_SERVER_DATALINK_TYPE == RTE_RFOP_SERVER_DATALINK_XCU)

static TMfSerialVirBus *L_RfopDatalinkVirBusCreate(void)
{
    TMfSerialVirBus *result = NULL;
    Int32 err_code = 0;

    err_code = TBsMasterXcuDevCreate(&g_rfop_xcu_dev_, RTE_RFOP_XCU_DEV_NAME);

    if (err_code < 0)
    {
        ALOG_E("TBsMaster Xcu Dev Create Failed! error:%d", err_code);
    }
    else if (!TBsMasterXcuVirBusImplCreate(&g_rfop_xcu_impl_, &g_rfop_xcu_dev_))
    {
        ALOG_E("TBsMaster Xcu VirBusImpl Create Failed!");
    }
    else 
    {
        result = TBsMasterXcuVirBusImplInterface(&g_rfop_xcu_impl_);
    }

    return result;
}

/// 释放数据同步的虚拟总线
static void L_RfopDatalinkVirBusDestroy(void)
{
    OfDestroyObject((TOfObject *)&g_rfop_xcu_impl_);
}

/// 获取数据同步的虚拟总线
static TMfSerialVirBus *L_RfopDatalinkVirBusHandle(void)
{
    return TBsMasterXcuVirBusImplInterface(&g_rfop_xcu_impl_);
}

#elif (RTE_RFOP_SERVER_DATALINK_TYPE == RTE_RFOP_SERVER_DATALINK_COM)

static TMfSerialVirBus *L_RfopDatalinkVirBusCreate(void)
{
    TMfSerialVirBus *result = NULL;
    Int32 err_code = 0;
    TMfSerialVirBusMfcComImplCreateParam param = {
        .baud_rate = RTE_RFOP_COM_BAUDRATE,
        .com_name = RTE_RFOP_COM_DEV_NAME,
        .rx_buf = g_rfop_datalink_recv_buffer_,
        .rx_buf_len = sizeof(g_rfop_datalink_recv_buffer_),
    };

    err_code = TMfSerialVirBusMfcComImplCreate(&g_rfop_com_virbus, &param);

    ALOG_I("Create COM Virtual Bus, baud:%d, name:%s, err_code:%d", RTE_RFOP_COM_BAUDRATE, RTE_RFOP_COM_DEV_NAME, err_code);

    return err_code < 0 ? NULL : TMfSerialVirBusMfcComImplInterface(&g_rfop_com_virbus);
}

/// 释放数据同步的虚拟总线
static void L_RfopDatalinkVirBusDestroy(void)
{
    TMfSerialVirBusMfcComImplDestroy(&g_rfop_com_virbus);
}

/// 获取数据同步的虚拟总线
static TMfSerialVirBus *L_RfopDatalinkVirBusHandle(void)
{
    return TMfSerialVirBusMfcComImplInterface(&g_rfop_com_virbus);
}

#else
    #error "RTE_RFOP_SERVER_DATALINK_TYPE is not defined!"
#endif
//------------------------------------------------------------------------------



static Int32 L_RfopUpgradeSysOptSendDataMethod(void *user_param, const Int8 *data, Int32 len)
{
    Int32 result = 0;
    TMfSerialVirBus *vir = L_RfopDatalinkVirBusHandle();

    if (TMfSerialVirBusConnected(vir) && TMfSerialVirBusWriteAble(vir) > 0)
    {
        result = TMfSerialVirBusSend(vir, (const char *)data, len);
        if (result > 0)
        {
            // ALOG_I("Datalink Send Data, len:%d", result);
            // ALOG_HEX(kALogLevelInfo, 32, data, len);
        }
        else if (result < 0)
        {
            ALOG_E("Datalink Send Data Failed, len:%d", result);
        }
        else
        {}
    }

    return result;
}

//------------------------------------------------------------------------------


void RteRfopServerCreate(void)
{
    L_RfopDatalinkVirBusCreate();
    RteRfopUpgradeCreate();

    TRfopSrvCreate(&g_rfop_srv_, RteRfopUpgradeBgoImpl(), &g_rte_rfop_cfg);
}

void RteRfopServerInit(void)
{
    RteRfopUpgradeInit();
    RteRfopUpgradeSetSendDataMethod(L_RfopUpgradeSysOptSendDataMethod, NULL);
}

void RteRfopServerDone(void)
{
    
    RteRfopUpgradeDone();
}

void RteRfopServerDestroy(void)
{
    TRfopSrvDestroy(&g_rfop_srv_);
    RteRfopUpgradeDestroy();
    L_RfopDatalinkVirBusDestroy();
}


void RteRfopServerRunOnce(void)
{
    UInt32 tick = 0;
    Int8 runs_ = 5;
    TMfSerialVirBus *virbus = L_RfopDatalinkVirBusHandle();
    
    if (TMfSerialVirBusConnected(virbus))
    {
        Int32 err = TMfSerialVirBusReadAble(virbus);

        while (err > 0 && runs_-- > 0)
        {
            err = TMfSerialVirBusRecv(virbus, g_rfop_datalink_recv_buffer_, sizeof(g_rfop_datalink_recv_buffer_));
            if (err < 0)
            {
                ALOG_E("Datalink Recv failed, err:%d", err);
                break;
            }
            else if (err > 0)
            {
                tick = SfGetTickCount();
                // ALOG_I("Datalink Recv Data %dByte", err);
                // ALOG_HEX(kALogLevelInfo, 32, (const UInt8 *)g_rfop_datalink_recv_buffer_, err);
                
                TRfopSrvRecvDataIndication(&g_rfop_srv_, (char *)g_rfop_datalink_recv_buffer_, err);
                // ALOG_D("OverRecv use %u ticks", SfGetTickCount() - tick);
            }
            else
            {
                err = TMfSerialVirBusReadAble(virbus);
            }
        }
    }
    else
    {
        Int32 err = TMfSerialVirBusConnect(virbus, NULL, 0);
        
        ALOG_D("Rfop xcu Connect, virbus:%u, return:%d", virbus, err);
    }

    RteRfopUpgradeRunOnce();
}




///< Generated on "2024-10-31 16:40:56" by the tool "gen_hq_file.py >> V20231119" 

