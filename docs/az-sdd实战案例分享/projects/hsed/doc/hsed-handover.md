# HSED 工程交接文档

> 更新时间：2026-08-02
> 适用工程：`YLB04_A6 / ag35_cetda / hsed`  
> 运行环境：AG35 Linux，SPI2 对应 `/dev/spidev1.0`

## 1. 当前结论

- 一期对外功能的正式集合与状态以 [`az-sdd/SPEC.md`](../az-sdd/SPEC.md) 为准；`az-sdd/spec/` 中每个文件只描述一个独立功能。本文保留硬件、部署、验证记录和未决项。

HSED 已完成从 CIU98 供应商 SDK 通信验证程序向 SPC 硬件加密代理服务的工程重构。当前代码包含：

- 正常 SPC 服务模式：加载 CIU98 动态库，向多进程客户端提供硬件密码能力。
- `hsed --test-spi` 自检模式：不启动正常 SPC 服务，只验证动态库装载和 CIU98 基础通信，完成后立即退出。
- INI 配置：通过 `--cfg <path>` 指定；未指定时统一使用 `/etc/hsed.ini`。
- 进程互斥：正常服务和 SPI 自检共用 `/tmp/hsed-ciu98.lock`，避免同时访问芯片。
- 测试代码隔离：生产代码不得依赖 `test/` 中的测试代码、fake、stub、测试专用头文件或测试数据；正式出包环境会临时删除全部测试代码，产品必须仍可独立构建、部署和运行。

需要特别注意：当前重构后的 `--test-spi` 是“通信烟测”，并不执行供应商 `vendor_sdk_test_main()` 的全量 API 示例。供应商全量 SDK 测试曾在重构前完成设备验证，相关修改已迁移到 `mpu/lib/bsc_hse_plug/vendor/ciu98/sdk/examples/`，但当前 CMake 未编译这些示例。

## 2. 运行流程

### 2.1 正常服务

`common/main.c` → 读取配置 → `RteHseInit()` 动态加载供应商 SO → `BscHseInit()` → 校验固定非对称密钥槽 → 启动 SPC 服务 → 串行访问 CIU98。

`[bsc_hse] debug=0` 时，槽位算法或长度不符合固定映射会阻止服务启动。
仅在受控测试环境设置 `debug=1`，此时初始化会完整遍历 RSA1024、
RSA2048、ECC256 和 SM2 密钥对，并调用插件的 `GenerateKeyPair(force_overwrite=True)`
尝试修复异常槽位。生产配置必须保持 `debug=0`。

正常服务支持：

```text
hsed
hsed --cfg <ini-path>
```

### 2.2 SPI 自检

`common/main.c` → 读取配置 → `RteHseSelfTest()` → 获取进程锁 → 加载供应商 SO → 调用 `BscHseVendorTest()` → 连接 CIU98 并读取 CHIP_ID → 断开并退出。

自检支持：

```text
hsed --test-spi
hsed --test-spi --cfg <ini-path>
```

退出码：

| 退出码 | 含义 |
|---:|---|
| 0 | 自检成功 |
| 2 | 命令行参数错误 |
| 3 | 配置或参数无效 |
| 4 | 正常服务或另一自检进程正在占用 CIU98 |
| 5 | 动态库、SPI 或 CIU98 通信失败 |

## 3. 配置和部署

默认配置文件为 `/etc/hsed.ini`，样例见 `doc/hsed.ini`：

```ini
[spi]
device_path = /dev/spidev1.0

[bsc_hse]
debug = 0
vendor_so_path = /usr/lib/libbsc_hse_ciu98_ylb04_a6.so
```

当前主要构建产物：

```text
mpu/build_output/app/debug/hsed
mpu/build_output/lib/debug/libbsc_hse_ciu98_ylb04_a6.so
```

设备运行前必须同时部署：

1. `hsed` 可执行文件。
2. `libbsc_hse_ciu98_ylb04_a6.so`，路径与 INI 中 `vendor_so_path` 一致。
3. `hsed.ini`，放到 `/etc/hsed.ini`，或通过 `--cfg` 指定其他路径。
4. 项目依赖的运行库；上一轮设备调试使用 `/mnt/card/back/app/hq/lib`。

`mpu/adb_exec_hsed.bat` 用于配合独立的
`adb_exec_hsed_spc_sm2_manual_verify.bat` 验证人工 SM2 用例。脚本会把 `hsed`、
ABI 匹配的供应商 SO 和 INI 部署到 `/media/card/hsed_test`，只修改远端 INI
副本中的插件路径，然后通过 PID 和 `/proc` 检查服务是否存活。默认保持
`debug=0`；只有显式传入 `/repair-keys` 才允许初始化尝试修复密钥，传入
`/stop` 可停止脚本启动的测试服务。由于当前 Windows ADB 不传播远端 shell
退出码，脚本通过远端状态标记判断配置和服务启动结果；失败时打印
`/media/card/hsed_test/hsed.log` 并返回非零。

## 4. 构建入口

Windows 本地 Docker 构建入口：

```bat
C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\az_build_linux_hsed.bat
```

CIU98 插件由独立仓库 `mpu/lib/bsc_hse_plug` 的 `build/build.sh` 先行构建；HSED 构建不再生成
`libbsc_hse_ciu98_ylb04_a6.so`。部署时应选用该仓库 CI 发布、且 ABI 与当前 HSED 匹配的插件产物。

已使用的构建环境：

- Docker 容器：`aa8f918e82f4bf9fd650466921459320c2325649ef82cbdd9472dd8fac907358`
- 模组型号：`ag35_cetda`
- 产品型号：`YLB04_A6`
- 工程名称：`hsed`

遗留的 `app/hsed/build/debug.bat` 因引用无关的 `hq_fota_app` 和 `/usrdata`，已在一期交付清理中移除。HSED 构建统一使用上述 Windows 本地 Docker 构建入口。

## 5. 硬件与底层约束

| 信号 | AG35 Linux GPIO | 当前约束 |
|---|---:|---|
| HSM_RST_N | 42 | 初始化时 export、设为输出；空闲默认拉低 |
| HSM_WAKE | 41 | 芯片工作时应主动拉高 |
| SPI2 CS | 34 | 软件 NSS 时由端口层控制 |
| SPI2 MISO | 35 | SPI 复用 |
| SPI2 MOSI | 36 | SPI 复用 |
| SPI2 CLK | 33 | SPI 复用 |

GPIO 注释中的 73、74 是 pin 标号，不是 Linux GPIO 序号。GPIO5 电源控制已由底层完成，HSED 不应重复操作。

当前源码状态：

- `PORT_SPI_SE0_RST_IO` 已设为 42。
- `PORT_SPI_SE0_WAKE_IO` 已定义为 41，但 `port_spi_gpio_init()` 尚未 export GPIO41、配置输出并拉高；设备完整功能验证前需要补齐或确认底层已代管。
- MOSI 空闲强制拉高的 `/dev/mem` 方案保持关闭：`PORT_SPI_MOSI_IDLE_FORCE_HIGH` 为 0。更换芯片后当前不需要该 workaround。
- SPI 收发 HEX 日志代码目前保留在 `port_spi.c` 中，但处于注释状态；排障时可临时启用，不建议长期打印敏感业务明文。

## 6. 已有验证记录

### 6.1 供应商 SDK 全量验证（重构前）

2026-07-24 曾让 `hsed --test-spi` 直接执行 `vendor_sdk_test_main()`，设备侧完成通信、认证、信息、密钥、密码、控制和文件系统示例测试，并运行到 `test end`。固件升级 `update_test()` 按要求未启用。

为兼容当前未装载 SE ID 的测试芯片，示例代码把 `api_get_id()` 返回 `0x10006985` 记录为预期 `SKIP`，随后继续其他测试；同时修正了 `all_api_test.c:101` 将 `info_test` 失败误报为 `auth_test` 的日志。

全量示例中以下错误日志可能来自固定密钥槽已存在，示例会继续执行更新流程：

```text
key.c@69: failed to call apdu_generate_keypair
key.c@236: failed to call apdu_write_key
```

这类错误只能视为供应商示例的已知现象，不能据此证明生产接口成功。需要结合后续用例结果和返回码判断。

### 6.2 当前重构版本

当前 `BscHseVendorTest()` 只执行注册、选择设备、连接、读取 `CHIP_ID`、断开，适合快速判断 SPI/CIU98 是否可通信。本文档整理期间没有重新进行设备推包验证，因此必须用当前构建产物再执行一次：

```sh
LD_LIBRARY_PATH=/mnt/card/back/app/hq/lib:$LD_LIBRARY_PATH \
./hsed --test-spi --cfg ./hsed.ini
```

随后再启动正常 `hsed`，配合 SPC 客户端验证业务接口和进程互斥。

### 6.3 2026-07-29 固定槽位诊断

使用 `/mnt/card/hsed-sm2-diagnostic.ini` 设置 `debug=1`，同时部署当次构建的
`hsed`、`libbsc_hse_ciu98_ylb04_a6.so` 和
`hsed_spc_sm2_manual_verify` 后完成设备诊断：

- RSA1024 公钥槽 5 导出异常，强制生成返回 `0x10009405`
  (`SE_ERR_KEYTYPE_EXISTED`)。
- RSA2048 公钥槽 7 已由芯片重新生成，复查长度为 260 字节。
- ECC256 公钥槽 9 正常，导出长度为 64 字节。
- SM2 公钥槽 11 实际导出成功但长度为 132 字节，证明槽 11/12
  当前存放的是 RSA1024 密钥对，而不是预期的 SM2 密钥对。
- 对槽 11/12 请求强制生成 SM2 时，芯片返回 `0x10009405`，
  服务保留统一业务映射 `kBscHseResultKeyOccupied`，并因初始化校验失败而不启动 SPC。

当前供应商 SDK 没有删除固定非对称密钥的公开接口，且
`api_generate_keypair()` 不能覆盖已存在的其他算法密钥。CIU98 官方手册和
SDK `storekey_test()` 明确规定 `SECURITY STORAGE` 可更新已占用且类型不同的
固定 KID，因此插件在 `force_overwrite=True` 时使用 OpenSSL 3 临时生成目标算法
密钥材料，再通过 `api_store_key(PRIVILEGE_LEVEL_0, ...)` 先覆盖私钥槽、后覆盖
公钥槽。临时私钥、记录缓冲和 OpenSSL 大数在调用结束前全部清零释放；该路径仅
允许 `debug=1` 的受控测试初始化触发，生产配置仍必须为 `debug=0`。

2026-08-01 在设备上完成槽 5/6 和 11/12 修复，其中槽 11/12 已从错误的
RSA1024 替换为 SM2。随后以 `debug=0` 重启服务，直接执行
`adb_exec_hsed_spc_sm2_manual_verify.bat` 返回 0：三组对象使用不同的随机
16 字节 USER_ID 和 1024 字节原文完成签名，并由设备 OpenSSL 3 对 SM2 ZA
签名逐组验证通过。

## 7. 测试工具

- `test/hsed_spc_adapter`：主机侧适配层、生命周期和会话归属测试。
- `test/ciu98_za_adapter`：CIU98 SM2 ZA 适配测试。
- `test/hsed_spc_smoke`：设备侧 SPC 生命周期和属性烟测，传入 `--aes` 可增加 AES-128 往返验证。
- `test/hsed_spc_sm2_stress`：设备侧多通道 SM2 压力测试。
- `test/hsed_spc_sm2_manual_verify`：设备侧三对象 SM2 验签测试；为 3 个对象设置不同的随机 16 字节 USER_ID，校验导出公钥一致，分别签名 1024 字节随机原文，完整打印 USER_ID、公钥、原文和签名。默认使用设备 OpenSSL 3 逐组验证 SM2 ZA，传入 `--manual` 时等待逐组输入网页验签结论。
- `../spc_hsed_clt_test`：参考 `spc_mcud_clt_test` 组织的独立 HSED SPC 客户端测试工程；业务调用统一使用 `generated/hsed_client.h` 中的 `HcHsed_*` 封装，支持交互菜单、单字符命令、单进程多线程 SM2 签名性能测试，并打印固定 USER_ID 的网页带 Z 验签输入。当前生成客户端只有一个进程级连接，多线程共享该连接并各自创建 HSE 对象；如需每线程独立连接，必须先在接口模型和生成器中增加连接显式封装。Windows 构建入口为 `mpu/az_build_linux_spc_hsed_clt_test.bat`，ADB 运行入口为 `mpu/adb_exec_spc_hsed_clt_test.bat`。

SPC 对外接口以 `doc/hsed.xlsx` 为权威模型，通过 `doc/spc-build.bat` 生成；客户端说明见 `doc/hsed_spc_client_api.md`。

## 0011 固定密钥槽位策略进展

- 接口模型共有 46 个属性标识；BSC 新建对象会装载当前支持的 45 项对象属性默认值。`CHIP_ID` 业务属性读写尚未实现，当前返回 `UNSUPPORTED`，已独立保留为 0012 后续规范。
- CIU98 槽位 1～14 已按 AES128、TDES128、SM1、SM4、RSA1024、RSA2048、ECC256、SM2、SM9 顺序固定分配；槽位 15～24 保留。
- 全部 `KEY_ID`、`PUBLIC_KEY_ID`、`PRIVATE_KEY_ID` 为只读属性，客户端修改或导入公钥覆盖固定槽位时返回 `PERMISSION_DENIED`。
- SM2 测试客户端固定读取并校验公钥槽位 11、私钥槽位 12，不再主动设置 KEY_ID。
- 当前 CIU98 SDK 仅支持 RSA1024、RSA2048、ECC256、SM2 密钥对生成；不支持 AES/TDES/SM1/SM4 持久密钥的芯片内生成，也不支持 SM9 密钥对生成。
- SDK 能导出公钥，但没有私钥存在性/角色查询接口。初始化以公钥算法和长度作为槽位类型判据，修复后再次导出公钥确认；不能把该检查描述为私钥内容可查询。
- 插件 ABI v3 提供 `GenerateKeyPair`，包含 `force_overwrite` 参数；RTE 根据 INI
  的 `debug` 决定是否请求修复。空闲槽调用芯片 `api_generate_keypair()`；异类占用槽
  由插件生成临时密钥材料并通过 `api_store_key()` 覆盖。业务层和 RTE 不接触私钥。

## 8. 已知风险与后续顺序

1. **P0：验证当前重构产物。** 同时部署可执行文件、供应商 SO 和 INI，在 AG35 实机执行 `hsed --test-spi`。
2. **P0：确认自检验收边界。** 决定生产版 `--test-spi` 保持 CHIP_ID 通信烟测，还是重新接入供应商全量 API 示例；两者耗时、破坏性和验收含义不同。
3. **P0：确认 GPIO41。** 当前端口初始化未主动拉高 WAKE；若底层没有代管，需要在 SPI 初始化时 export、设为输出并拉高。
4. **P1：完成 SPC 业务验证。** 覆盖加密、解密、签名、公钥导出、公钥导入拒绝策略、会话归属、并发串行化及正常服务与自检互斥。
5. **已完成：ADB 联动实测。** 已修复槽 5/6、11/12；默认 `debug=0` 启动服务后，
   `adb_exec_hsed_spc_sm2_manual_verify.bat` 三组签名和 OpenSSL SM2 ZA 验签通过。
6. **P1：清理供应商编译告警。** `proto_spi_impl.c` 曾出现指针与整数比较、向整数参数传入 `NULL` 等告警；重构后需重新构建确认现状。
7. **已完成：清理遗留脚本。** 已移除与 HSED 无关的 `build/debug.bat`。

## 9. 接手检查清单

- [ ] 阅读 `AGENTS.md`、本文件和 `az-sdd/` 中已批准规范。
- [ ] 使用 `az_build_linux_hsed.bat` 完整构建并记录 warning/error 数量。
- [ ] 核对 `hsed`、供应商 SO、INI 三者为同一版本并同步到设备。
- [ ] 确认 `/dev/spidev1.0` 存在且权限允许访问。
- [ ] 核对 GPIO42 空闲低、GPIO41 工作时高，且应用不操作 GPIO5。
- [ ] 执行当前 `hsed --test-spi`，保存完整日志和远程退出码。
- [ ] 启动正常 SPC 服务并运行 smoke、AES 和 SM2 压力测试。
- [ ] 将新的硬件结论、测试结果和未决项同步更新到本文件。
