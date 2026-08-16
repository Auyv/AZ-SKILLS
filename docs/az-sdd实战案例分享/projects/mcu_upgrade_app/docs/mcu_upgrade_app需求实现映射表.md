# mcu_upgrade_app需求实现映射表

## 1. 映射表说明

本映射表用于记录 mcu_upgrade_app 项目中功能需求与代码实现的对应关系，帮助开发人员快速定位到源代码，理解功能实现细节。

## 2. 需求实现映射表

| 需求编号 | 需求名称 | 实现文件路径 | 关键函数/类 | 行号范围 | 说明 |
|---------|---------|-------------|------------|----------|------|
| FR-MPU-001 | MPU升级功能-升级请求 | common/mpu_upgrade_app.c | MpuUpgradeRequest() | 107-112 | 请求执行MPU自升级，设置超时时间和回调函数 |
| FR-MPU-002 | MPU升级功能-状态查询 | common/mpu_upgrade_app.c | MpuUpgradeResponse() | 124-136 | 获取MPU升级状态、进度和错误码 |
| FR-MCU-001 | MCU升级功能-升级请求 | rfop_server/rte_rfop_server/src/rte_rfop_upgrade.c | RteRfopUpgradeStartTask() | - | 请求执行MCU自升级，指定升级文件路径 |
| FR-MCU-002 | MCU升级功能-状态查询 | rfop_server/rte_rfop_server/src/rte_rfop_upgrade.c | RteRfopUpgradeCurTaskState(), RteRfopUpgradeCurTaskPercent() | - | 获取MCU升级状态和进度 |
| FR-NF-001 | NF升级功能-预检查 | common/nf_upgrade_state_exec.c | NfUpgradeStatePreCheckRunOnce() | 199-417 | 检查升级包有效性，解包升级包，检测MPU和MCU固件 |
| FR-NF-002 | NF升级功能-MPU升级执行 | common/nf_upgrade_state_exec.c | NfUpgradeStateMpuUpgradeRunOnce() | 460-527 | 调用MPU升级模块执行MPU升级，监控进度 |
| FR-NF-003 | NF升级功能-MCU升级执行 | common/nf_upgrade_state_exec.c | NfUpgradeStateMcuUpgradeRunOnce() | 580-631 | 调用MCU升级模块执行MCU升级，监控进度 |
| FR-NF-004 | NF升级功能-重启管理 | common/nf_upgrade_state_exec.c | NfUpgradeStateWaiteRebootRunOnce() | 674-678 | 管理升级后的重启流程，等待设备重启 |
| FR-NF-005 | NF升级功能-升级确认 | common/nf_upgrade_state_exec.c | NfUpgradeStateMpuConfirmRunOnce(), NfUpgradeStateMcuConfirmRunOnce() | 707-748, 782-814 | 确认MPU和MCU升级结果 |
| FR-STATE-001 | 状态管理功能-状态查询 | common/nf_upgrade_app.c | NfUpgradeAppFlushStrData() | 148-195 | 通过SRT接口查询和更新升级状态 |
| FR-STATE-002 | 状态管理功能-进度监控 | common/nf_upgrade_app.c | NfUpgradeAppFlushStrData() | 148-195 | 实时监控和更新升级进度百分比 |
| FR-ERROR-001 | 错误处理功能-错误检测 | 各升级模块 | 错误处理回调函数 | - | 检测升级过程中的各种错误 |
| FR-ERROR-002 | 错误处理功能-错误恢复 | 各升级模块 | 错误处理函数 | - | 从错误状态中恢复，记录错误码 |
| FR-POWER-001 | 掉电恢复功能-状态恢复 | common/nf_upgrade_app.c | NfUpgradeAppRecover() | 197-227 | 掉电后恢复升级状态，从上次中断的地方继续升级 |
| FR-CMD-001 | 命令行界面功能-命令解析 | cmdline/cmdline.c | 命令解析函数 | - | 解析命令行输入，处理命令参数 |
| FR-CMD-002 | 命令行界面功能-命令执行 | cmdline/cmdline.c | 命令执行函数 | - | 执行各种升级相关命令，反馈结果 |
| FR-HTTP-001 | HTTP下载功能-升级包下载 | cmdline/http_download.c | CmdDownloadImg() | - | 通过HTTP协议下载升级包，支持进度监控和错误处理 |
| FR-CORE-001 | 核心应用功能 | common/mcu_upgrade_app.c | main(), Run() | 145-174, 43-79 | 应用程序主入口，初始化和协调各个升级模块 |
| FR-CORE-002 | NF升级应用功能 | common/nf_upgrade_app.c | NfUpgradeAppCreate(), NfUpgradeAppInit(), NfUpgradeAppRunOnce() | 43-47, 54-67, 75-104 | NF升级应用的创建、初始化和运行 |
| FR-CORE-003 | MPU升级应用功能 | common/mpu_upgrade_app.c | MpuUpgradeAppCreate(), MpuUpgradeAppInit(), MpuUpgradeAppRunOnce() | 36-41, 48-67, 74-80 | MPU升级应用的创建、初始化和运行 |
| FR-CORE-004 | RFOP服务器功能 | rfop_server/rte_rfop_server/src/rte_rfop_server.c | RteRfopServerCreate(), RteRfopServerInit(), RteRfopServerRunOnce() | - | RFOP服务器的创建、初始化和运行 |
| FR-LOG-001 | 日志系统功能 | common/a_log_backend_file_impl.c | ALogInit(), ALogRegisterOutput() | - | 初始化日志系统，注册输出后端 |
| FR-LOG-002 | 文件日志功能 | file_log/file_log.c | FileLogFlush() | - | 刷新日志数据到文件，确保日志记录的完整性 |

## 3. 硬件平台适配映射

| 硬件平台 | 实现文件路径 | 关键函数/类 | 说明 |
|---------|-------------|------------|------|
| AG35_ASR1806 | mpu_upgrade/common/mpu_upgrade_ag35_asr1806.c | - | AG35_ASR1806平台的MPU升级适配 |
| AG35_CEVN | mpu_upgrade/common/mpu_upgrade_ag35_cevn.c | - | AG35_CEVN平台的MPU升级适配 |
| SIM7805 | mpu_upgrade/common/mpu_upgrade_sim7805.c | - | SIM7805平台的MPU升级适配 |

## 4. 配置项映射

| 配置项 | 实现文件路径 | 说明 |
|--------|-------------|------|
| LOG_FILE_DIR | common/upgrade_app_cfg.h | 日志文件目录 |
| LOG_FILE_SIZE | common/upgrade_app_cfg.h | 日志文件大小 |
| LOG_FILE_COUNT | common/upgrade_app_cfg.h | 日志文件数量 |
| LOG_FILE_TAG | common/upgrade_app_cfg.h | 日志文件前缀 |
| MPU_UPGRADE_FILE_NAME | common/upgrade_app_cfg.h | MPU升级文件路径 |
| NFCREATE_UPGRADE_IMG_PATH | common/upgrade_app_cfg.h | NFCreator升级包路径 |
| NFCREATE_UPGRADE_SE_IMG_PATH | common/upgrade_app_cfg.h | 安全NFCreator升级包路径 |
| NET_PACKAGE_PEM_KEYS_DIR | common/upgrade_app_cfg.h | 网络升级PEM密钥目录 |
| NET_PACKAGE_URL_FORMAT | common/upgrade_app_cfg.h | 网络升级URL格式 |

## 5. 错误码映射

| 错误码 | 说明 | 实现文件路径 |
|--------|------|-------------|
| kMpuUpgradeErrNone | MPU升级无错误 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrInvalidParam | MPU升级参数错误 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrInvalidState | MPU升级状态错误 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrInvalidPath | MPU升级文件路径错误 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrTimeout | MPU升级操作超时 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrOptFailed | MPU升级操作失败 | common/mpu_upgrade_app.c |
| kMpuUpgradeErrInvalidFile | MPU升级文件错误 | common/mpu_upgrade_app.c |
| kNfUpgradeErrCodeSuccess | NF升级成功 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeFailed | NF升级未知错误 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeMpuUpgrade | NF升级MPU升级异常 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeMcuUpgrade | NF升级MCU升级异常 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeMcuWaitConf | NF升级MCU等待确认超时 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeMpuWaitConf | NF升级MPU等待确认超时 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeImgInvalid | NF升级包无效 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeInitFailed | NF升级初始化失败 | common/nf_upgrade_app.c |
| kNfUpgradeErrCodeMcuState | NF升级MCU状态异常 | common/nf_upgrade_app.c |

## 6. 状态机映射

| 状态名称 | 说明 | 实现文件路径 | 状态处理函数 |
|---------|------|-------------|------------|
| kNfUpgradeStateInit | 初始化状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateInitEnter(), NfUpgradeStateInitRunOnce(), NfUpgradeStateInitGoto() |
| kNfUpgradeStateIdle | 空闲状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateIdleEnter(), NfUpgradeStateIdleRunOnce(), NfUpgradeStateIdleGoto() |
| kNfUpgradeStatePreCheck | 预检查状态 | common/nf_upgrade_state_exec.c | NfUpgradeStatePreCheckEnter(), NfUpgradeStatePreCheckRunOnce(), NfUpgradeStatePreCheckGoto() |
| kNfUpgradeStateMpuUpgrade | MPU升级状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateMpuUpgradeEnter(), NfUpgradeStateMpuUpgradeRunOnce(), NfUpgradeStateMpuUpgradeGoto() |
| kNfUpgradeStateMcuUpgrade | MCU升级状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateMcuUpgradeEnter(), NfUpgradeStateMcuUpgradeRunOnce(), NfUpgradeStateMcuUpgradeGoto() |
| kNfUpgradeStateWaiteReboot | 等待重启状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateWaiteRebootEnter(), NfUpgradeStateWaiteRebootRunOnce(), NfUpgradeStateWaiteRebootGoto() |
| kNfUpgradeStateMpuConfirm | MPU确认状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateMpuConfirmEnter(), NfUpgradeStateMpuConfirmRunOnce(), NfUpgradeStateMpuConfirmGoto() |
| kNfUpgradeStateMcuConfirm | MCU确认状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateMcuConfirmEnter(), NfUpgradeStateMcuConfirmRunOnce(), NfUpgradeStateMcuConfirmGoto() |
| kNfUpgradeStateSuccess | 成功状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateSuccessEnter(), NfUpgradeStateSuccessRunOnce(), NfUpgradeStateSuccessGoto() |
| kNfUpgradeStateFailed | 失败状态 | common/nf_upgrade_state_exec.c | NfUpgradeStateFailedEnter(), NfUpgradeStateFailedRunOnce(), NfUpgradeStateFailedGoto() |

## 7. 总结

本映射表覆盖了 mcu_upgrade_app 项目中的主要功能需求与代码实现的对应关系，包括核心升级功能、状态管理、错误处理、掉电恢复等。通过本映射表，开发人员可以快速定位到源代码，理解功能实现细节，提高开发和维护效率。

随着项目的不断发展和功能的扩展，本映射表应及时更新，确保与代码实现保持一致。
