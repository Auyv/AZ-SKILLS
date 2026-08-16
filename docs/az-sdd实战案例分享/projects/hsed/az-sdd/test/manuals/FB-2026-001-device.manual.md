# FB-2026-001 目标设备验收工单

## 前置条件

1. AG35 目标设备可用，SPI 与 CIU98 硬件连接正常。
2. 部署本次构建的 `hsed` 和 RTE 所需 CIU98 供应商动态库。
3. `hsed.ini` 的 `vendor_so_path` 指向实际动态库路径。
4. SPC 客户端及接口生成代码已部署。

## 验收步骤

| 编号 | 操作 | 预期结果 | 实际结果 |
|---|---|---|---|
| DV001 | 启动 HSED，观察 RTE 加载、BSC 初始化和服务启动日志 | RTE 成功加载动态库并注册接口，BSC 初始化成功 | 待执行 |
| DV002 | 将 `vendor_so_path` 改为不存在路径后启动 | 启动失败并返回明确错误，不发生崩溃或资源泄漏 | 待执行 |
| DV003 | 使用 SPC 客户端执行加密、解密、签名、公钥导入和导出 | app/SPC 适配仅调用 BSC API，各接口结果符合 0004～0009 规范 | 待执行 |
| DV004 | 两个 SPC 客户端并发请求硬件密码操作 | BSC 串行访问 CIU98，客户端会话不跨连接共享 | 待执行 |
| DV005 | 正常停止 HSED 并再次启动 | 按 BSC Done、RTE Done 顺序释放，可重复启动 | 待执行 |

## 判定

- DV001～DV005 全部通过后，可关闭 FB-2026-001。
- 任一项失败时记录日志、动态库版本、配置及复现步骤，并将反馈退回处理中。

## 2026-07-24 SPC 现场冒烟入口

本次构建已附带 `hsed_spc_smoke`，无需现场另写客户端。

1. 部署 `hsed`、`hsed_spc_smoke` 和
   `libbsc_hse_ciu98_ylb04_a6.so`，并确认 `hsed.ini` 中
   `vendor_so_path` 指向该动态库。
2. 启动服务：`./hsed --cfg ./hsed.ini`。
3. 另开终端执行 `./hsed_spc_smoke`。该命令验证连接、HseCreate、
   Set/GetPropertyInt、Set/GetPropertyBytes 和 HseDestroy，不写密钥槽。
4. 确认 AES128 默认槽位已正确注入密钥后，再执行
   `./hsed_spc_smoke --aes`，验证 Encrypt/Decrypt 往返。
5. 默认测试输出 `[PASS] HSED SPC lifecycle/property smoke test`；
   AES 测试成功时输出中追加 `and AES128 round trip`。

签名、公钥导出和公钥导入继续按 DV003 使用授权测试槽位执行。公钥导入会
改变芯片持久槽位，禁止在未确认目标槽位和覆盖权限时执行。
